#include "applicationcontroller.hpp"
#include "settingscontroller.hpp"
#include <LayerShellQt/Window>
#include <QGuiApplication>
#include <QScreen>

#include <QCoreApplication>
#include <QDrag>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>

namespace
{
bool dispatchHyprlandSubmap(const QString &name)
{
    if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
        return false;
    }

    const QString hyprctl = QStandardPaths::findExecutable(QStringLiteral("hyprctl"));
    if (hyprctl.isEmpty()) {
        return false;
    }

    const QString expression = QStringLiteral(
        "hl.dispatch(hl.dsp.submap(\"%1\"))").arg(name);
    return QProcess::startDetached(
        hyprctl,
        {QStringLiteral("eval"), expression});
}
}

ApplicationController::ApplicationController(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
    connect(m_server, &QLocalServer::newConnection,
            this, &ApplicationController::acceptConnection);
    m_focusRequestTimeout.setSingleShot(true);
    m_focusRequestTimeout.setInterval(1000);
    connect(&m_focusRequestTimeout, &QTimer::timeout,
            this, &ApplicationController::finishFocusRequest);
}

ApplicationController::~ApplicationController()
{
    finishFocusRequest();
    resetHyprlandSubmap();
}

bool ApplicationController::hudVisible() const
{
    return m_hudVisible;
}

bool ApplicationController::hudOpen() const
{
    return m_hudOpen;
}

void ApplicationController::setWindow(QQuickWindow *window)
{
    m_window = window;
    if (m_window) {
        auto *layer = LayerShellQt::Window::get(m_window);
        layer->setCloseOnDismissed(false);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
        connect(m_window, &QWindow::activeChanged, this, [this] {
            if (m_window->isActive()) {
                m_receivedFocus = true;
                finishFocusRequest();
            } else {
                QTimer::singleShot(250, this, &ApplicationController::checkFocus);
            }
        });
        connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
            if (!visible) finishFocusRequest();
        });
        if (m_hudVisible && m_hudOpen) showAndFocusWindow();
        else m_window->setVisible(m_hudVisible);
    }
}

void ApplicationController::setSettings(SettingsController *settings)
{
    m_settings = settings;
    connect(settings, &SettingsController::preferenceChanged, this, [this](const QString &key, const QVariant &) {
        if (key == "lowPowerVisualMode" && m_window) {
            recreateSurface();
            return;
        }
        if (key != "displayChoice" || !m_window || !m_hudOpen) return;
        m_receivedFocus = false; m_window->hide(); selectScreen(); showAndFocusWindow();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] {
        QTimer::singleShot(0, this, [this] { if (m_hudOpen && m_window) { selectScreen(); showAndFocusWindow(); } });
    });
}

QString ApplicationController::layerScope() const {
    if (m_projectionActive) return QStringLiteral("hyprendfield-projection");
    return m_settings && m_settings->value("lowPowerVisualMode").toBool()
        ? QStringLiteral("hyprendfield-low-power") : QStringLiteral("hyprendfield");
}
void ApplicationController::setProjectionActive(bool active) {
    if (m_projectionActive == active) return;
    m_projectionActive = active; recreateSurface();
}
void ApplicationController::recreateSurface() {
    // Wayland fixes the layer namespace at surface creation. Only this surface
    // is recreated; global compositor blur and all component state are retained.
    if (!m_window) { emit layerScopeChanged(); return; }
    const bool visible = m_window->isVisible();
    m_receivedFocus = false; m_window->hide(); m_window->destroy();
    emit layerScopeChanged();
    LayerShellQt::Window::get(m_window)->setScope(layerScope());
    if (visible) {
        if (m_hudOpen && !m_fileDragActive) showAndFocusWindow();
        else m_window->show();
    }
}

void ApplicationController::showAndFocusWindow()
{
    if (!m_window) return;
    if (!m_window->isVisible() || !m_window->isActive()) {
        // Hyprland skips OnDemand layers while another surface constrains the
        // pointer. Exclusive transfers focus and releases that old constraint.
        // Drop it as soon as focus arrives so global shortcuts and dialogs work.
        // The timeout also releases it if the compositor declines the request.
        m_focusRequestTimeout.start();
        LayerShellQt::Window::get(m_window)->setKeyboardInteractivity(
            LayerShellQt::Window::KeyboardInteractivityExclusive);
    } else {
        // A cancelled file drag can reveal the still-mapped, still-focused HUD.
        m_receivedFocus = true;
    }
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
    m_window->update(); // Commit keyboard mode changes even on an already mapped HUD.
}

void ApplicationController::finishFocusRequest()
{
    m_focusRequestTimeout.stop();
    if (!m_window) return;
    auto *layer = LayerShellQt::Window::get(m_window);
    if (layer->keyboardInteractivity() != LayerShellQt::Window::KeyboardInteractivityExclusive) return;
    layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    m_window->update();
}

void ApplicationController::selectScreen()
{
    if (!m_settings || !m_window) return;
    auto *layer = LayerShellQt::Window::get(m_window);
    const auto choice = m_settings->value("displayChoice").toString();
    if (choice == "pointer") { layer->setScreen(nullptr); layer->setWantsToBeOnActiveScreen(true); return; }
    layer->setWantsToBeOnActiveScreen(false);
    QScreen *selected = QGuiApplication::primaryScreen();
    for (auto *screen : QGuiApplication::screens()) if (screen->name() == choice) selected = screen;
    if (selected) { layer->setScreen(selected); m_window->setScreen(selected); }
}

void ApplicationController::checkFocus()
{
    if (!m_settings || !m_settings->value("closeOnFocusLost").toBool() || !m_receivedFocus || !m_hudOpen || m_fileDragActive) return;
    for (auto *window = QGuiApplication::focusWindow(); window; window = window->transientParent())
        if (window == m_window) return;
    hideHud();
}

bool ApplicationController::listen(const QString &serverName)
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(serverName);
    return m_server->listen(serverName);
}

void ApplicationController::executeCommand(const QString &command)
{
    const QString normalized = command.trimmed().toLower();
    if (normalized == QStringLiteral("background")) return;
    if (normalized == QStringLiteral("show")) {
        showHud();
    } else if (normalized == QStringLiteral("hide")) {
        hideHud();
    } else if (normalized == QStringLiteral("quit")) {
        quitApplication();
    } else {
        toggleHud();
    }
}

void ApplicationController::showHud()
{
    if (m_fileDragActive) return;
    if (!m_hudVisible) { selectScreen(); m_receivedFocus = false; }
    setHudVisible(true);
    showAndFocusWindow();
    setHudOpen(true);
    activateHyprlandSubmap();
}

void ApplicationController::hideHud()
{
    if (m_settings) { m_settings->revertLayout(); m_settings->cancelBatteryPosition(); }
    finishFocusRequest();
    resetHyprlandSubmap();
    setHudOpen(false);
}

void ApplicationController::toggleHud()
{
    if (m_fileDragActive) { QDrag::cancel(); return; }
    if (m_hudOpen) {
        hideHud();
    } else {
        showHud();
    }
}

void ApplicationController::completeHide()
{
    if (m_hudOpen) {
        return;
    }

    setHudVisible(false);
    if (m_fileDragActive) {
        // A Wayland drag must retain its source wl_surface. An out-of-bounds
        // input region releases the desktop without unmapping that surface.
        m_fileDragRetracted = true;
        if (m_window) { m_window->setMask(QRegion(-2, -2, 1, 1)); m_window->update(); }
        if (m_fileDragFinished) finishFileDrag();
        return;
    }
    if (m_window) {
        m_window->hide();
    }
}

// Same order as EndfieldHUD ShelfDragPresentationState: retract fully, then
// remain closed after a delivered copy or restore after cancellation.
void ApplicationController::beginFileDrag()
{
    if (m_fileDragActive || !m_hudOpen) return;
    m_fileDragActive = true;
    m_fileDragFinished = false;
    m_fileDragDelivered = false;
    m_fileDragRetracted = false;
    hideHud();
}

void ApplicationController::endFileDrag(bool delivered)
{
    if (!m_fileDragActive) return;
    m_fileDragFinished = true;
    m_fileDragDelivered = delivered;
    if (m_fileDragRetracted) finishFileDrag();
}

void ApplicationController::finishFileDrag()
{
    m_fileDragActive = false;
    if (m_fileDragDelivered) {
        if (m_window) m_window->hide();
    }
    if (m_window) m_window->setMask(QRegion());
    if (!m_fileDragDelivered) showHud();
}

void ApplicationController::quitApplication()
{
    finishFocusRequest();
    resetHyprlandSubmap();
    setHudOpen(false);
    QCoreApplication::quit();
}

void ApplicationController::acceptConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        const auto handleCommand = [this, socket] {
            const QString command = QString::fromUtf8(socket->readAll()).trimmed();
            if (!command.isEmpty()) {
                executeCommand(command);
            }
            socket->disconnectFromServer();
            socket->deleteLater();
        };

        connect(socket, &QLocalSocket::readyRead, this, handleCommand);
        QTimer::singleShot(0, this, [socket, handleCommand] {
            if (socket->bytesAvailable() > 0) {
                handleCommand();
            }
        });
    }
}

void ApplicationController::setHudVisible(bool visible)
{
    if (m_hudVisible == visible) {
        return;
    }

    m_hudVisible = visible;
    emit hudVisibleChanged();
}

void ApplicationController::setHudOpen(bool open)
{
    if (m_hudOpen == open) {
        return;
    }

    m_hudOpen = open;
    emit hudOpenChanged();
}

void ApplicationController::activateHyprlandSubmap()
{
    m_hyprlandSubmapActive = dispatchHyprlandSubmap(QStringLiteral("hyprendfield"));
}

void ApplicationController::resetHyprlandSubmap()
{
    if (!m_hyprlandSubmapActive) {
        return;
    }

    dispatchHyprlandSubmap(QStringLiteral("reset"));
    m_hyprlandSubmapActive = false;
}
