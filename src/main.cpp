#include "applicationcontroller.hpp"
#include "watchanimationcontroller.hpp"

#include <QGuiApplication>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include <unistd.h>

namespace
{
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
    if (!controller.listen(serverName)) {
        qCritical("Unable to create the HyprEndfield control socket");
        return 1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("sourceWatchAnimation"), &watchAnimation);
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
