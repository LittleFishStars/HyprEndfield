#include "projectioncanvas.hpp"
#include "storagecontroller.hpp"
#include "mediaeditor.hpp"
#include "mediaengine.hpp"
#include "mediainfo.hpp"
#include "notescontroller.hpp"
#include "profilecontroller.hpp"
#include <QFile>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPainter>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class AppStub : public QObject {
    Q_OBJECT
public:
    QQuickWindow *projectionWindow = nullptr;
    Q_INVOKABLE void setProjectionActive(bool) {
        if (projectionWindow) {
            const bool visible = projectionWindow->isVisible();
            projectionWindow->hide(); projectionWindow->destroy();
            if (visible) projectionWindow->show();
        }
    }
};
class MediaTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_files;
    QString m_video;
    QString m_gif;
    static QQuickItem *find(QQuickItem *root, const QString &name) {
        if (root->objectName() == name) return root;
        for (auto *child : root->childItems()) if (auto *item = find(child, name)) return item;
        return nullptr;
    }
private slots:
    void initTestCase() {
        QVERIFY(m_files.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_files.path());
        m_video = m_files.filePath("test.mp4");
        QProcess process;
        process.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i", "testsrc2=size=64x48:rate=10", "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100", "-t", "1", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", m_video});
        QVERIFY(process.waitForFinished(15'000)); QCOMPARE(process.exitCode(), 0);
        m_gif = m_files.filePath("test.gif");
        process.start("ffmpeg", {"-v", "error", "-i", m_video, "-vf", "fps=5", m_gif});
        QVERIFY(process.waitForFinished(15'000)); QCOMPARE(process.exitCode(), 0);
    }
    void videoNotesAndRichText() {
        QTemporaryDir directory;
        NotesController notes(nullptr, directory.path());
        notes.importMedia(QUrl::fromLocalFile(m_video), 40, 60);
        QTRY_COMPARE_WITH_TIMEOUT(notes.state().value("total").toInt(), 1, 10'000);
        const auto note = notes.state().value("notes").toList().first().toMap();
        QCOMPARE(note.value("kind").toString(), QString("video"));
        const auto saved = QUrl(note.value("mediaUrl").toString()).toLocalFile();
        QVERIFY(QFileInfo::exists(saved)); QVERIFY(saved != m_video);
        const auto id = notes.createNote("text", 100, 100);
        QVERIFY(notes.updateNoteRichText(id, "<p><b>hello</b> world</p>"));
        NotesController restored(nullptr, directory.path());
        const auto entries = restored.state().value("notes").toList(); QCOMPARE(entries.size(), 2);
        QVERIFY(entries.last().toMap().value("richText").toString().contains("font-weight:700"));
        QVERIFY(restored.removeNote(note.value("id").toString())); QVERIFY(!QFileInfo::exists(saved)); QVERIFY(QFileInfo::exists(m_video));
        restored.importMedia(QUrl::fromLocalFile(m_gif), 40, 60);
        QTRY_COMPARE_WITH_TIMEOUT(restored.state().value("total").toInt(), 2, 4000);
        NotesController withGif(nullptr, directory.path());
        QCOMPARE(withGif.state().value("notes").toList().last().toMap().value("kind").toString(), QString("gif"));
    }
    void originalProcessingAndExports() {
        QImage source(24, 16, QImage::Format_RGBA8888); source.fill(QColor("#40A0E0"));
        source.setPixelColor(0, 0, Qt::red);
        endfield::modules::MediaAssemblyAdjustments a;
        const auto unchanged = renderMediaFrame(source, a, true);
        QCOMPARE(unchanged.size(), source.size());
        for (int y = 0; y < source.height(); ++y) for (int x = 0; x < source.width(); ++x) QCOMPARE(unchanged.pixelColor(x,y), source.pixelColor(x,y));
        a.rotationQuarterTurns = 1; QCOMPARE(renderMediaFrame(source, a, true).size(), QSize(16, 24));
        a.crop = {.25, .25, .5, .5}; QCOMPARE(renderMediaFrame(source, a, true).size(), QSize(8, 12));
        const auto input = m_files.filePath("source.png"); QVERIFY(source.save(input));
        const auto meta = inspectLocalMedia(QUrl::fromLocalFile(input)); QVERIFY(!meta.contains("error"));
        std::atomic_bool cancel{false}; std::atomic<double> progress{0};
        const auto output = m_files.filePath("result.png");
        QVERIFY(exportMedia(meta, a, output, false, cancel, progress).isEmpty()); QCOMPARE(QImage(output).size(), QSize(8, 12));
        QVERIFY(!exportMedia(meta, a, output, false, cancel, progress).isEmpty());
        a = {}; a.filter = endfield::modules::MediaAssemblyFilter::special1;
        QVERIFY(renderMediaFrame(source, a, false) != source);
        a = {}; a.stickers.push_back({"test", endfield::modules::MediaAssemblyStickerKind::sticker7});
        QVERIFY(!renderMediaFrame(source, a, true).isNull());
        const auto video = inspectLocalMedia(QUrl::fromLocalFile(m_video)); QVERIFY(!video.contains("error"));
        QCOMPARE(video.value("width").toInt(), 64); QVERIFY(video.value("duration").toDouble() >= .9);
        a = {}; a.rotationQuarterTurns = 1; a.trimStart = .2; a.trimEnd = .8;
        const auto videoOutput = m_files.filePath("edited.mp4");
        const auto error = exportMedia(video, a, videoOutput, false, cancel, progress); QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto exported = inspectLocalMedia(QUrl::fromLocalFile(videoOutput));
        QCOMPARE(exported.value("width").toInt(), 48); QCOMPARE(exported.value("height").toInt(), 64);
        QVERIFY(exported.value("duration").toDouble() < .85);
        cancel = true; const auto cancelled = m_files.filePath("cancelled.png");
        QVERIFY(!exportMedia(meta, a, cancelled, false, cancel, progress).isEmpty()); QVERIFY(!QFileInfo::exists(cancelled));
    }
    void projectionInkAndMedia() {
        ProjectionCanvas canvas; canvas.setSize({800, 600});
        canvas.beginStroke({100, 100}); canvas.moveStroke({400, 200}); canvas.endStroke(); QCOMPARE(canvas.strokeCount(), 1);
        QImage image(800, 600, QImage::Format_ARGB32_Premultiplied); image.fill(Qt::transparent); QPainter painter(&image); canvas.paint(&painter); painter.end();
        QVERIFY(image.pixelColor(100, 100).alpha() > 0);
        canvas.setProperty("erasing", true); canvas.beginStroke({250, 150}); canvas.endStroke(); QCOMPARE(canvas.strokeCount(), 0);
        canvas.importMedia(QUrl::fromLocalFile(m_video)); QTRY_COMPARE_WITH_TIMEOUT(canvas.media().size(), 1, 8000);
        const auto id = canvas.media().first().toMap().value("id").toString(); canvas.placeMedia(id, -30, -80, 2000, 2000);
        const auto entry = canvas.media().first().toMap(); QCOMPARE(entry.value("x").toDouble(), 0.); QCOMPARE(entry.value("width").toDouble(), 800.);
        canvas.clearContent(); QVERIFY(canvas.media().isEmpty()); QVERIFY(QFileInfo::exists(m_video));
    }
    void projectionBackground() {
        QQmlEngine engine; AppStub app;
        engine.rootContext()->setContextProperty("appController", &app);
        for (const auto *font : {"harmonyRegular", "harmonyMedium", "harmonyBold"}) engine.rootContext()->setContextProperty(font, QVariantMap{{"name", "sans-serif"}});
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString(MEDIA_QML_DIR) + "/ProjectionWorkspace.qml"));
        std::unique_ptr<QObject> workspace(component.create()); QVERIFY2(workspace, qPrintable(component.errorString()));
        QQuickWindow window; window.setColor(QColor("#808080")); window.resize(1000, 700);
        auto *item = qobject_cast<QQuickItem *>(workspace.get()); QVERIFY(item); item->setParentItem(window.contentItem()); item->setSize({1000, 700});
        workspace->setProperty("presented", true);
        window.show(); QTest::qWait(100); app.projectionWindow = &window;
        workspace->setProperty("dotted", true); workspace->setProperty("darkness", .63);
        workspace->setProperty("opened", true); QTest::qWait(100);
        const auto on = window.grabWindow(); QVERIFY(!on.isNull());
        const auto pixel = [&](const QImage &image, int x, int y) { return image.pixelColor(qRound(x * image.devicePixelRatio()), qRound(y * image.devicePixelRatio())).red(); };
        QVERIFY2(pixel(on, 120, 192) > pixel(on, 125, 192) + 15, "The source's 2px dot must be visibly brighter than the dark background");
        workspace->setProperty("dotted", false); QTest::qWait(60); const auto off = window.grabWindow();
        QCOMPARE(pixel(off, 120, 192), 128); QCOMPARE(pixel(off, 125, 192), 128);
        workspace->setProperty("dotted", true); workspace->setProperty("presented", false); workspace->setProperty("presented", true); QTest::qWait(60);
        const auto reopened = window.grabWindow(); QVERIFY(pixel(reopened, 120, 192) > pixel(reopened, 125, 192) + 15);
        workspace->setProperty("opened", false); QTest::qWait(60);
        workspace->setProperty("opened", true); QTest::qWait(100);
        const auto recreated = window.grabWindow(); QVERIFY2(pixel(recreated, 120, 192) > pixel(recreated, 125, 192) + 15, "The dot texture must survive recreation of the HUD's Wayland surface");
        workspace->setProperty("darkness", 0); QTest::qWait(60);
        const auto transparent = window.grabWindow();
        QVERIFY(pixel(transparent, 120, 192) > pixel(transparent, 125, 192) + 15);
        QCOMPARE(pixel(transparent, 125, 192), 128);
        window.resize(1200, 800); item->setSize({1200, 800}); QTest::qWait(60);
        const auto resized = window.grabWindow();
        QVERIFY(pixel(resized, 1164, 768) > pixel(resized, 1169, 768) + 15);
        app.projectionWindow = nullptr;
        item->setParentItem(nullptr);
    }
    void profilePersistenceAndImages() {
        QTemporaryDir directory; ProfileController profile(nullptr, directory.path());
        const auto uid = profile.state().value("playerID");
        QVERIFY(profile.setField("name", "Local user")); QVERIFY(profile.setField("birthday", "02-29"));
        QVERIFY(!profile.setField("birthday", "02-30")); QVERIFY(!profile.setField("permissionLevel", 61));
        QVERIFY(!profile.setField("tag", "bad # tag")); QVERIFY(!profile.setField("avatarZoom", std::nan("")));
        QVERIFY(profile.setField("themeColor", "#C9A2FF"));
        ProfileController restored(nullptr, directory.path()); QCOMPARE(restored.state().value("name").toString(), QString("Local user")); QCOMPARE(restored.state().value("playerID"), uid);
        QImage source(80,40,QImage::Format_RGB32); source.fill(Qt::blue); const auto path=m_files.filePath("portrait.png"); QVERIFY(source.save(path));
        profile.importImage("avatar", QUrl::fromLocalFile(path)); QTRY_VERIFY_WITH_TIMEOUT(!profile.state().value("avatar").toString().isEmpty(), 4000);
        const auto owned = directory.filePath(profile.state().value("avatar").toString()); QVERIFY(QFileInfo::exists(owned)); QCOMPARE(profile.image("avatar").size(), source.size());
        QVERIFY(profile.resetImage("avatar")); QVERIFY(!QFileInfo::exists(owned)); QVERIFY(QFileInfo::exists(path));
        QVERIFY(profile.setField("permissionLevel", 2));
        QVERIFY(!restored.setField("name", "External stale edit"));
        ProfileController latest(nullptr, directory.path()); QCOMPARE(latest.state().value("name").toString(), QString("Local user"));
    }
    void pagesAndContinuousEditing() {
        QQmlEngine engine; AppStub app; StorageController storage;
        QTemporaryDir notesDirectory; NotesController notes(nullptr, notesDirectory.path());
        ProfileController profile(nullptr, notesDirectory.filePath("Profile"));
        engine.rootContext()->setContextProperty("profileController", &profile);
        engine.rootContext()->setContextProperty("appController", &app);
        engine.rootContext()->setContextProperty("storageController", &storage);
        engine.rootContext()->setContextProperty("notesController", &notes);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int font = QFontDatabase::addApplicationFont(QString(MEDIA_QML_DIR) + "/../assets/fonts/HarmonyOS_Sans_" + weight + ".ttf");
            QVERIFY(font >= 0); engine.rootContext()->setContextProperty(QString("harmony") + weight, QVariantMap{{"name", QFontDatabase::applicationFontFamilies(font).first()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent previewComponent(&engine, QUrl::fromLocalFile(QString(MEDIA_QML_DIR) + "/MediaPreview.qml"));
        std::unique_ptr<QObject> preview(previewComponent.createWithInitialProperties({{"kind", "gif"}, {"source", QUrl::fromLocalFile(m_gif)}, {"presented", true}}));
        QVERIFY2(preview != nullptr, qPrintable(previewComponent.errorString()));
        auto *animation = preview->findChild<QObject *>("mediaAnimation"); QVERIFY(animation);
        QTRY_VERIFY_WITH_TIMEOUT(animation->property("frameCount").toInt() > 1, 2000);
        QVERIFY(animation->property("playing").toBool());
        preview->setProperty("presented", false); QVERIFY(!animation->property("playing").toBool());
        preview->setProperty("kind", "video"); preview->setProperty("source", QUrl::fromLocalFile(m_video));
        auto *player = preview->findChild<QMediaPlayer *>("mediaPlayer"); QVERIFY(player);
        preview->findChild<QAudioOutput *>("mediaAudio")->setMuted(true);
        QTRY_VERIFY_WITH_TIMEOUT(player->duration() > 0, 4000);
        preview->setProperty("presented", true); player->play();
        QTRY_COMPARE_WITH_TIMEOUT(player->playbackState(), QMediaPlayer::PlayingState, 2000);
        preview->setProperty("presented", false);
        QCOMPARE(player->playbackState(), QMediaPlayer::PausedState);
        QQmlComponent workspace(&engine, QUrl::fromLocalFile(QString(MEDIA_QML_DIR) + "/ProjectionWorkspace.qml"));
        std::unique_ptr<QObject> projection(workspace.create()); QVERIFY2(projection != nullptr, qPrintable(workspace.errorString()));
        auto *projectionItem = qobject_cast<QQuickItem *>(projection.get()); projectionItem->setSize({1000, 700});
        projection->setProperty("presented", true); projection->setProperty("opened", true);
        for (const auto &filename : {"StoragePage.qml", "MediaAssemblyPage.qml", "ProjectionPage.qml", "ProfilePage.qml"}) {
            QQmlComponent component(&engine, QUrl::fromLocalFile(QString(MEDIA_QML_DIR) + '/' + filename));
            std::unique_ptr<QObject> page(component.createWithInitialProperties(QString(filename) == "ProjectionPage.qml" ? QVariantMap{{"workspace", QVariant::fromValue(projection.get())}} : QVariantMap{}));
            QVERIFY2(page != nullptr, qPrintable(component.errorString()));
            QQuickWindow window; window.setColor(QColor("#191B1D")); window.resize(780, 640); auto *item = qobject_cast<QQuickItem *>(page.get()); item->setParentItem(window.contentItem()); item->setPosition({60, 50});
            window.show(); QTest::qWait(70);
            if (QString(filename) == "ProfilePage.qml") {
                auto *name = find(item, "profileField_name"); QVERIFY(name);
                QVERIFY(QMetaObject::invokeMethod(name->parentItem(), "beginEditing"));
                QTest::keyClick(&window, Qt::Key_Z); QTest::keyClick(&window, Qt::Key_Return);
                QCOMPARE(profile.state().value("name").toString(), QString("z"));
                page->setProperty("popup", "theme"); QTest::qWait(30); page->setProperty("popup", "portrait"); QTest::qWait(30); page->setProperty("popup", "");
            }
            if (QString(filename) == "MediaAssemblyPage.qml") {
                auto *editor = qobject_cast<MediaEditor *>(find(item,"mediaEditor")); QVERIFY(editor);
                editor->openSource(QUrl::fromLocalFile(m_files.filePath("source.png")));
                QTRY_VERIFY_WITH_TIMEOUT(editor->state().value("loaded").toBool(), 4000);
                editor->setFilter("special1"); editor->addSticker("sticker7"); QTest::qWait(120);
            }
            if (qEnvironmentVariableIsSet("HYPR_MEDIA_RENDER")) QVERIFY(window.grabWindow().save(QString(MEDIA_PREVIEW_DIR) + '/' + filename + ".png"));
            item->setParentItem(nullptr);
        }
        const auto id = notes.createNote("text", 40, 40);
        QQmlComponent cards(&engine, QUrl::fromLocalFile(QString(MEDIA_QML_DIR) + "/NotesWorkspace.qml"));
        std::unique_ptr<QObject> object(cards.create()); QVERIFY2(object != nullptr, qPrintable(cards.errorString()));
        object->setProperty("presented", true); object->setProperty("selectedModule", "notes");
        QQuickWindow window; window.resize(800, 600); auto *item = qobject_cast<QQuickItem *>(object.get()); item->setParentItem(window.contentItem()); window.show();
        QTest::qWait(50); auto *editor = find(item, "floatingNoteText"); QVERIFY(editor); editor->forceActiveFocus();
        for (const auto key : {Qt::Key_H, Qt::Key_E, Qt::Key_L, Qt::Key_L, Qt::Key_O}) QTest::keyClick(&window, key);
        QTest::qWait(700); QVERIFY(editor->hasActiveFocus());
        QCOMPARE(find(item, "floatingNoteText"), editor);
        QTest::keyClick(&window, Qt::Key_Q); QTest::keyClick(&window, Qt::Key_E); QTest::qWait(700);
        QCOMPARE(notes.state().value("notes").toList().first().toMap().value("text").toString(), QString("helloqe"));
        QTest::keyClick(&window, Qt::Key_X); item->forceActiveFocus();
        QCOMPARE(notes.state().value("notes").toList().first().toMap().value("text").toString(), QString("helloqex"));
        item->setParentItem(nullptr);
        if (!warnings.isEmpty()) for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty());
    }
};
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv); QCoreApplication::setOrganizationName("HyprEndfieldTest"); QCoreApplication::setApplicationName("MediaTest");
    qmlRegisterType<ProjectionCanvas>("HyprEndfield.Projection", 1, 0, "ProjectionCanvas");
    qmlRegisterType<ProjectionDots>("HyprEndfield.Projection", 1, 0, "ProjectionDots");
    qmlRegisterType<MediaEditor>("HyprEndfield.Media", 1, 0, "MediaEditor");
    qmlRegisterType<ProfileArtwork>("HyprEndfield.Profile", 1, 0, "ProfileArtwork");
    MediaTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "media_components.moc"
