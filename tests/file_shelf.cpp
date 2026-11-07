#include "fileshelfcontroller.hpp"
#include "applicationcontroller.hpp"

#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QMimeData>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

namespace {
bool makeFile(const QString &path, const QByteArray &contents = "original file")
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *item = findItem(child, name)) return item;
    return nullptr;
}
QVariantList entries(const FileShelfController &controller) { return controller.state().value("items").toList(); }
QString idAt(const FileShelfController &controller, int index) { return entries(controller)[index].toMap().value("id").toString(); }
}

class ShelfTests final : public QObject
{
    Q_OBJECT
    QUrl m_opened;
public slots:
    void captureUrl(const QUrl &url) { m_opened = url; }
private slots:
    void referencesAndPersistence()
    {
        QTemporaryDir archive, originals;
        const auto first = originals.filePath(QStringLiteral("测试文件 #1.pdf"));
        const auto folder = originals.filePath(QStringLiteral("文件夹"));
        const auto alias = originals.filePath(QStringLiteral("link.pdf"));
        const auto hardLink = originals.filePath(QStringLiteral("hardlink.pdf"));
        QVERIFY(makeFile(first)); QVERIFY(QDir().mkpath(folder));
        QVERIFY(makeFile(folder + QStringLiteral("/child.txt")));
        QVERIFY(::symlink(QFile::encodeName(first).constData(), QFile::encodeName(alias).constData()) == 0);
        QVERIFY(::link(QFile::encodeName(first).constData(), QFile::encodeName(hardLink).constData()) == 0);
        FileShelfController controller(nullptr, archive.path());
        QVERIFY(controller.addUrls({QUrl::fromLocalFile(first), QUrl::fromLocalFile(folder), QUrl::fromLocalFile(first), QUrl::fromLocalFile(alias), QUrl::fromLocalFile(hardLink)}));
        QTRY_VERIFY(!controller.state().value("busy").toBool());
        QCOMPARE(entries(controller).size(), 3);
        const auto fileId = idAt(controller, 0), folderId = idAt(controller, 1), aliasId = idAt(controller, 2);
        QCOMPARE(entries(controller)[0].toMap().value("kind").toString(), QStringLiteral("pdf"));
        controller.readProperties(folderId);
        QTRY_COMPARE(controller.properties().value("count").toString(), QStringLiteral("1 项"));
        QString error;
        const auto payload = controller.exportUrls({fileId, folderId, aliasId}, &error);
        QCOMPARE(payload, QList<QUrl>({QUrl::fromLocalFile(first), QUrl::fromLocalFile(folder), QUrl::fromLocalFile(alias)}));
        QMimeData mime; mime.setUrls(payload);
        QMimeData received; received.setData(QStringLiteral("text/uri-list"), mime.data(QStringLiteral("text/uri-list")));
        QCOMPARE(received.urls(), payload);

        QDesktopServices::setUrlHandler(QStringLiteral("file"), this, "captureUrl");
        QVERIFY(controller.openItem(fileId));
        QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
        QCOMPARE(m_opened, QUrl::fromLocalFile(first));
        FileShelfController reloaded(nullptr, archive.path());
        QCOMPARE(entries(reloaded).size(), 3);
        QVERIFY(reloaded.removeItems({fileId}));
        QVERIFY(reloaded.clear());
        QVERIFY(QFileInfo::exists(first)); QVERIFY(QFileInfo::exists(folder + QStringLiteral("/child.txt")));
        QVERIFY(QFileInfo(alias).isSymbolicLink());
        QFile original(first); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), QByteArray("original file"));
        QCOMPARE(QDir(archive.path()).entryList(QDir::Files), QStringList{QStringLiteral("shelf.json")});
    }

    void missingReplacedAndExternalChanges()
    {
        QTemporaryDir archive, originals;
        const auto file = originals.filePath(QStringLiteral("reference.txt"));
        QVERIFY(makeFile(file));
        FileShelfController controller(nullptr, archive.path());
        QVERIFY(controller.addUrls({QUrl::fromLocalFile(file)}));
        QTRY_VERIFY(!controller.state().value("busy").toBool());
        const auto id = idAt(controller, 0);
        QVERIFY(QFile::rename(file, file + QStringLiteral(".moved")));
        controller.refresh();
        QTRY_VERIFY(!controller.state().value("busy").toBool());
        QVERIFY(!entries(controller)[0].toMap().value("available").toBool());
        QVERIFY(makeFile(file, "replacement"));
        QString error;
        QVERIFY(controller.exportUrls({id}, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("占用")));
        QVERIFY(!controller.addUrls({QUrl(QStringLiteral("https://example.invalid/file"))}));
        const auto missing = originals.filePath(QStringLiteral("missing.txt"));
        QVERIFY(controller.addUrls({QUrl::fromLocalFile(file + QStringLiteral(".moved")), QUrl::fromLocalFile(missing)}));
        QTRY_VERIFY(!controller.state().value("busy").toBool());
        QCOMPARE(entries(controller).size(), 1); // A rejected batch is not partially imported.

        QFile saved(archive.filePath(QStringLiteral("shelf.json")));
        QVERIFY(saved.open(QIODevice::Append)); saved.write(" "); saved.close();
        QVERIFY(!controller.clear());
        QVERIFY(saved.open(QIODevice::ReadOnly)); QVERIFY(saved.readAll().endsWith(' ')); saved.close();
        QVERIFY(QFileInfo::exists(file));
        QVERIFY(makeFile(archive.filePath(QStringLiteral("shelf.json")), "broken"));
        FileShelfController broken(nullptr, archive.path());
        QVERIFY(!broken.state().value("ready").toBool());
        QVERIFY(!broken.clear());
        QVERIFY(saved.open(QIODevice::ReadOnly)); QCOMPARE(saved.readAll(), QByteArray("broken"));
    }

    void pageSelectionDropAndClear()
    {
        QTemporaryDir archive, originals;
        FileShelfController controller(nullptr, archive.path());
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("fileShelfController"), &controller);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int id = QFontDatabase::addApplicationFont(QStringLiteral(SHELF_QML_DIR) + QStringLiteral("/../assets/fonts/HarmonyOS_Sans_") + weight + QStringLiteral(".ttf"));
            QVERIFY(id >= 0);
            engine.rootContext()->setContextProperty(QStringLiteral("harmony") + weight,
                QVariantMap{{"name", QFontDatabase::applicationFontFamilies(id).constFirst()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(SHELF_QML_DIR) + QStringLiteral("/FileShelfPage.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *page = qobject_cast<QQuickItem *>(object.get()); QVERIFY(page);
        QQuickWindow window; window.resize(790, 650); window.setColor(QColor("#202020"));
        page->setParentItem(window.contentItem()); page->setPosition(QPointF(60, 40));
        page->setProperty("reduceMotion", true); page->setProperty("presented", true);
        window.show(); QTest::qWait(80);
        QTRY_VERIFY(!controller.state().value("busy").toBool());
        auto *chooser = page->findChild<QObject *>(QStringLiteral("shelfFileChooser")); QVERIFY(chooser);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(530, 75));
        QTRY_VERIFY(chooser->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(chooser, "reject"));
        QTRY_VERIFY(!chooser->property("visible").toBool());
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(530, 75));
        QTRY_VERIFY(chooser->property("visible").toBool());
        page->setProperty("presented", false);
        QTRY_VERIFY(!chooser->property("visible").toBool());
        page->setProperty("presented", true);
        QTRY_VERIFY(!controller.state().value("busy").toBool());
        if (qEnvironmentVariableIsSet("HYPR_SHELF_RENDER")) QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-shelf-empty.png")));

        const QStringList names{QStringLiteral("Field Report.pdf"), QStringLiteral("Talos Survey.png"), QStringLiteral("Patrol Footage.mp4"), QStringLiteral("Archive Bundle.zip"), QStringLiteral("Notes.txt"), QStringLiteral("Readme"), QStringLiteral("Operations.csv"), QStringLiteral("Signal Log.txt")};
        QList<QUrl> urls;
        for (const auto &name : names) { const auto path = originals.filePath(name); QVERIFY(makeFile(path)); urls.append(QUrl::fromLocalFile(path)); }
        QMimeData mime; mime.setUrls(urls);
        QDragEnterEvent enter(QPointF(120, 160), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &enter); QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(120, 160), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &drop); QVERIFY(drop.isAccepted());
        QTRY_COMPARE(entries(controller).size(), 8);
        QTest::qWait(60);
        QCOMPARE(page->property("columns").toInt(), 4);
        auto *grid = findItem(page, QStringLiteral("shelfGrid")); QVERIFY(grid);
        QCOMPARE(grid->property("contentY").toReal(), 0.0);
        if (qEnvironmentVariableIsSet("HYPR_SHELF_RENDER")) QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-shelf-grid.png")));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 170));
        QCOMPARE(page->property("selectedIds").toList().size(), 1);
        QCOMPARE(page->property("columns").toInt(), 3);
        QTRY_VERIFY(!controller.properties().value("modified").toString().isEmpty());
        QTest::qWait(50);
        if (qEnvironmentVariableIsSet("HYPR_SHELF_RENDER")) QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-shelf-properties.png")));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::ControlModifier, QPoint(235, 170));
        QCOMPARE(page->property("selectedIds").toList().size(), 2);
        QCOMPARE(page->property("columns").toInt(), 4);
        QTest::keyClick(&window, Qt::Key_Space);
        QCOMPARE(page->property("columns").toInt(), 3);
        QTest::keyClick(&window, Qt::Key_Down, Qt::ShiftModifier);
        QVERIFY(page->property("selectedIds").toList().size() > 1);
        QTest::keyClick(&window, Qt::Key_Delete);
        QVERIFY(entries(controller).size() < 8);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(610, 75));
        QVERIFY(page->property("confirmOpen").toBool());
        QTest::keyClick(&window, Qt::Key_Escape);
        QVERIFY(!page->property("confirmOpen").toBool());
        QVERIFY(QMetaObject::invokeMethod(page, "clearShelf"));
        QCOMPARE(entries(controller).size(), 0);
        for (const auto &url : urls) QVERIFY(QFileInfo::exists(url.toLocalFile()));
        page->setParentItem(nullptr);
        if (!warnings.isEmpty()) for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty());
    }

    void dragPresentationOrder()
    {
        qunsetenv("HYPRLAND_INSTANCE_SIGNATURE"); // Do not dispatch to the real desktop.
        QQuickWindow window;
        ApplicationController controller;
        controller.setWindow(&window);
        controller.showHud();
        controller.beginFileDrag();
        QVERIFY(!controller.hudOpen()); QVERIFY(controller.hudVisible());
        controller.endFileDrag(false); // Fast cancellation waits for the close animation.
        QVERIFY(!controller.hudOpen());
        controller.completeHide();
        QVERIFY(controller.hudOpen()); QVERIFY(window.mask().isEmpty());
        controller.beginFileDrag();
        controller.completeHide();
        QVERIFY(!controller.hudVisible()); QVERIFY(window.isVisible());
        QVERIFY(!window.mask().contains(QPoint(0, 0)));
        controller.endFileDrag(false);
        QVERIFY(controller.hudOpen()); QVERIFY(window.mask().isEmpty());
        controller.beginFileDrag();
        controller.endFileDrag(true);
        QVERIFY(window.isVisible());
        controller.completeHide();
        QVERIFY(!controller.hudOpen()); QVERIFY(!window.isVisible()); QVERIFY(window.mask().isEmpty());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    ShelfTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "file_shelf.moc"
