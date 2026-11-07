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
#include "fileshelfcontroller.hpp"
#include "archivecontroller.hpp"

#include <QGuiApplication>
#include <QIcon>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QProcess>
#include <QQuickAsyncImageProvider>
#include <QQuickImageProvider>
#include <QQuickTextureFactory>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>
#include <QtConcurrentRun>

#include <unistd.h>

#include <algorithm>

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
class ArchiveThumbnailResponse final : public QQuickImageResponse
{
public:
    ArchiveThumbnailResponse(QString id, QSize requested)
    {
        connect(&m_watcher, &QFutureWatcherBase::finished, this, &QQuickImageResponse::finished);
        m_watcher.setFuture(QtConcurrent::run([id = std::move(id), requested] {
            const QByteArray encoded = id.toLatin1();
            const QString path = QString::fromUtf8(QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding));
            if (path.isEmpty()) return QImage();
            QImageReader reader(path);
            reader.setAutoTransform(true);
            if (reader.canRead()) {
                QSize size = reader.size();
                const QSize wanted = requested.isValid() ? requested : QSize(256, 256);
                if (size.isValid()) { size.scale(wanted, Qt::KeepAspectRatio); reader.setScaledSize(size); }
                return reader.read();
            }
            QProcess process;
            process.start(QStringLiteral("ffmpegthumbnailer"), {QStringLiteral("-i"), path, QStringLiteral("-o"), QStringLiteral("/dev/stdout"),
                QStringLiteral("-s"), QString::number(std::max(64, requested.isValid() ? std::max(requested.width(), requested.height()) : 256)), QStringLiteral("-q"), QStringLiteral("8")});
            if (!process.waitForStarted(2000) || !process.waitForFinished(8000)) { process.kill(); process.waitForFinished(); return QImage(); }
            return QImage::fromData(process.readAllStandardOutput());
        }));
    }
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(m_watcher.result()); }
private:
    QFutureWatcher<QImage> m_watcher;
};
class ArchiveThumbnailProvider final : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override
    {
        return new ArchiveThumbnailResponse(id, requestedSize);
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
    FileShelfController fileShelf;
    ArchiveController archive;
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
    engine.rootContext()->setContextProperty(QStringLiteral("fileShelfController"), &fileShelf);
    engine.rootContext()->setContextProperty(QStringLiteral("archiveController"), &archive);
    engine.addImageProvider(QStringLiteral("activity-icons"), new ActivityIconProvider);
    engine.addImageProvider(QStringLiteral("audio-icons"), new AudioIconProvider);
    engine.addImageProvider(QStringLiteral("archive-thumbnails"), new ArchiveThumbnailProvider);
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
    fileShelf.setWindow(window);
    QObject::connect(&fileShelf, &FileShelfController::externalOpened, &controller, &ApplicationController::hideHud);
    QObject::connect(&fileShelf, &FileShelfController::dragStarted, &controller, &ApplicationController::beginFileDrag);
    QObject::connect(&fileShelf, &FileShelfController::dragFinished, &controller, &ApplicationController::endFileDrag);
    QTimer::singleShot(0, &controller, [&controller, command] {
        controller.executeCommand(command);
    });

    return application.exec();
}
