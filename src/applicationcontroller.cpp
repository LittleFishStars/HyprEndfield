#include "applicationcontroller.hpp"

#include <QCoreApplication>
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
}

ApplicationController::~ApplicationController()
{
    resetHyprlandSubmap();
}

bool ApplicationController::hudVisible() const
{
    return m_hudVisible;
}

void ApplicationController::setWindow(QQuickWindow *window)
{
    m_window = window;
    if (m_window) {
        m_window->setVisible(m_hudVisible);
    }
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
    setHudVisible(true);
    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
    }
    activateHyprlandSubmap();
}

void ApplicationController::hideHud()
{
    resetHyprlandSubmap();
    setHudVisible(false);
    if (m_window) {
        m_window->hide();
    }
}

void ApplicationController::toggleHud()
{
    if (m_hudVisible) {
        hideHud();
    } else {
        showHud();
    }
}

void ApplicationController::quitApplication()
{
    resetHyprlandSubmap();
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
