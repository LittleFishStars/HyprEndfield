#include "applicationcontroller.hpp"
#include "watchanimationcontroller.hpp"
#include "nowplayingcontroller.hpp"
#include "splayerlyrics.hpp"
#include "audiocontroller.hpp"
#include "terminalsession.hpp"
#include "terminalview.hpp"
#include "activitycontroller.hpp"
#include "calendarcontroller.hpp"
#include "clipboardcontroller.hpp"
#include "notescontroller.hpp"

#include <QGuiApplication>
#include <QIcon>
#include <QFileInfo>
#include <QQuickImageProvider>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

#include <unistd.h>

namespace
{
class AudioIconProvider final : public QQuickImageProvider
{
public:
    AudioIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const QSize target = requested.isValid() ? requested : QSize(64, 64);
        QImage image = QIcon::fromTheme(QUrl::fromPercentEncoding(id.toUtf8())).pixmap(target).toImage();
        if (image.isNull()) { image = QImage(target, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent); }
        if (size) *size = image.size();
        return image;
    }
};
class ActivityIconProvider final : public QQuickImageProvider
{
public:
    ActivityIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const QString name = QUrl::fromPercentEncoding(id.toUtf8());
        const QSize target = requested.isValid() ? requested : QSize(64, 64);
        const QIcon icon = QFileInfo(name).isAbsolute() ? QIcon(name) : QIcon::fromTheme(name);
        const auto image = icon.pixmap(target).toImage();
        if (size) *size = image.size();
        return image;
    }
};
QString requestedCommand(const QStringList &arguments, bool &valid)
{
    valid = true;
    if (arguments.size() <= 1) {
        return QStringLiteral("toggle");
    }

    const QString argument = arguments.at(1).toLower();
    if (argument == QStringLiteral("--show")) {
        return QStringLiteral("show");
    }
    if (argument == QStringLiteral("--hide")) {
        return QStringLiteral("hide");
    }
    if (argument == QStringLiteral("--toggle")) {
        return QStringLiteral("toggle");
    }
    if (argument == QStringLiteral("--quit")) {
        return QStringLiteral("quit");
    }

    valid = false;
    return {};
}

QString localServerName()
{
    return QStringLiteral("hyprendfield-%1").arg(static_cast<qulonglong>(geteuid()));
}

bool sendToExistingInstance(const QString &serverName, const QString &command)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::WriteOnly);
    if (!socket.waitForConnected(250)) {
        return false;
    }

    socket.write(command.toUtf8());
    socket.flush();
    socket.waitForBytesWritten(250);
    socket.disconnectFromServer();
    return true;
}
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
    QSurfaceFormat surfaceFormat;
    surfaceFormat.setSamples(4);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("HyprEndfield"));
    application.setApplicationDisplayName(QStringLiteral("HyprEndfield"));
    application.setOrganizationName(QStringLiteral("HyprEndfield"));
    application.setQuitOnLastWindowClosed(false);

    bool commandValid = false;
    const QString command = requestedCommand(application.arguments(), commandValid);
    if (!commandValid) {
        qCritical("Usage: HyprEndfield [--show|--hide|--toggle|--quit]");
        return 2;
    }

    const QString serverName = localServerName();
    if (sendToExistingInstance(serverName, command)) {
        return 0;
    }

    if (command == QStringLiteral("hide") || command == QStringLiteral("quit")) {
        return 0;
    }

    ApplicationController controller;
    WatchAnimationController watchAnimation;
    NowPlayingController nowPlaying;
    SPlayerLyrics lyrics(&nowPlaying);
    AudioController audio;
    TerminalSession terminal;
    ActivityController activity;
    CalendarController calendar;
    ClipboardController clipboard;
    NotesController notes;
    qmlRegisterType<TerminalView>("HyprEndfield.Terminal", 1, 0, "TerminalView");
    QObject::connect(&audio, &AudioController::commandFailed, [](const QString &error) {
        qWarning().noquote() << "Audio:" << error;
    });
    QObject::connect(&nowPlaying, &NowPlayingController::commandFailed, [](const QString &error) {
        qWarning().noquote() << "SPlayer:" << error;
    });
    if (!controller.listen(serverName)) {
        qCritical("Unable to create the HyprEndfield control socket");
        return 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("sourceWatchAnimation"), &watchAnimation);
    engine.rootContext()->setContextProperty(QStringLiteral("nowPlayingController"), &nowPlaying);
    engine.rootContext()->setContextProperty(QStringLiteral("splayerLyrics"), &lyrics);
    engine.rootContext()->setContextProperty(QStringLiteral("audioController"), &audio);
    engine.rootContext()->setContextProperty(QStringLiteral("terminalSession"), &terminal);
    engine.rootContext()->setContextProperty(QStringLiteral("activityController"), &activity);
    engine.rootContext()->setContextProperty(QStringLiteral("calendarController"), &calendar);
    engine.rootContext()->setContextProperty(QStringLiteral("clipboardController"), &clipboard);
    engine.rootContext()->setContextProperty(QStringLiteral("notesController"), &notes);
    engine.addImageProvider(QStringLiteral("activity-icons"), new ActivityIconProvider);
    engine.addImageProvider(QStringLiteral("audio-icons"), new AudioIconProvider);
    engine.loadFromModule(QStringLiteral("HyprEndfield"), QStringLiteral("Main"));

    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window) {
        qCritical("The HyprEndfield root object is not a window");
        return 1;
    }

    controller.setWindow(window);
    QTimer::singleShot(0, &controller, [&controller, command] {
        controller.executeCommand(command);
    });

    return application.exec();
}
