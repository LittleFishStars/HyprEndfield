#include "archivecontroller.hpp"
#include "fileshelfcontroller.hpp"

#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *item = findItem(child, name)) return item;
    return nullptr;
}

QVariantMap selectedDocument(const ArchiveController &controller)
{
    return controller.state().value(QStringLiteral("document")).toMap();
}

int schemaVersion(const QString &path)
{
    const QString connection = QStringLiteral("archive-test-version-%1").arg(QUuid::createUuid().toString());
    int version = -1;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database);
            if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next()) version = query.value(0).toInt();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return version;
}

QString classificationKey(const QString &path, const QString &id)
{
    const QString connection = QStringLiteral("archive-test-key-%1").arg(QUuid::createUuid().toString());
    QString key;
    {
        auto database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        database.setDatabaseName(path);
        if (database.open()) {
            QSqlQuery query(database); query.prepare(QStringLiteral("SELECT classification FROM entries WHERE id=?"));
            query.addBindValue(id); if (query.exec() && query.next()) key = query.value(0).toString();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return key;
}

void installFonts(QQmlEngine &engine)
{
    for (const auto &weight : {QStringLiteral("Regular"), QStringLiteral("Medium"), QStringLiteral("Bold")}) {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(ARCHIVE_QML_DIR) + QStringLiteral("/../assets/fonts/HarmonyOS_Sans_") + weight + QStringLiteral(".ttf"));
        QVERIFY(id >= 0);
        engine.rootContext()->setContextProperty(QStringLiteral("harmony") + weight,
            QVariantMap{{QStringLiteral("name"), QFontDatabase::applicationFontFamilies(id).constFirst()}});
    }
}
}

class ArchiveTests final : public QObject
{
    Q_OBJECT
private slots:
    void categoriesDocumentsMediaAndPersistence()
    {
        QTemporaryDir archive, originals;
        auto vault = std::make_shared<ArchiveMemoryPasswordVault>();
        QString categoryId, documentId;
        {
            ArchiveController controller(nullptr, archive.path(), vault);
            QVERIFY(controller.state().value("ready").toBool());
            QCOMPARE(controller.state().value("page").toString(), QStringLiteral("list"));
            categoryId = controller.addCategory(); QVERIFY(!categoryId.isEmpty());
            QVERIFY(controller.renameCategory(categoryId, QStringLiteral("外勤记录")));
            QVERIFY(controller.selectCategory(categoryId));
            documentId = controller.createDocument(); QVERIFY(!documentId.isEmpty());
            QVERIFY(controller.updateSelected(QStringLiteral("勘探日志"), QStringLiteral("第一行\n第二行"), QStringLiteral("<p><b>第一行</b><br>第二行</p>")));
            QImage image(40, 30, QImage::Format_ARGB32_Premultiplied); image.fill(QColor(QStringLiteral("#FFFA00")));
            const QString mediaPath = originals.filePath(QStringLiteral("现场照片.png")); QVERIFY(image.save(mediaPath));
            QVERIFY(controller.importMedia({QUrl::fromLocalFile(mediaPath)}));
            QTest::qWait(420);
            const auto document = selectedDocument(controller);
            QCOMPARE(document.value("title").toString(), QStringLiteral("勘探日志"));
            QCOMPARE(document.value("media").toList().size(), 1);
        }
        ArchiveController reloaded(nullptr, archive.path(), vault);
        QVERIFY(reloaded.state().value("ready").toBool());
        QCOMPARE(reloaded.state().value("page").toString(), QStringLiteral("editor"));
        QCOMPARE(selectedDocument(reloaded).value("title").toString(), QStringLiteral("勘探日志"));
        QCOMPARE(selectedDocument(reloaded).value("body").toString(), QStringLiteral("第一行\n第二行"));
        QCOMPARE(selectedDocument(reloaded).value("media").toList().size(), 1);
        QVERIFY(reloaded.moveSelected(QStringLiteral("uncategorized")));
        reloaded.closeDocument();
        QVERIFY(reloaded.selectCategory(categoryId));
        QCOMPARE(reloaded.state().value("entries").toList().size(), 0);
        QVERIFY(reloaded.removeCategory(categoryId));
    }

    void classificationLockingAndMissingVerifier()
    {
        QTemporaryDir archive;
        auto vault = std::make_shared<ArchiveMemoryPasswordVault>();
        ArchiveController controller(nullptr, archive.path(), vault);
        const QString id = controller.createDocument(); QVERIFY(!id.isEmpty());
        QVERIFY(controller.updateSelected(QStringLiteral("机密简报"), QStringLiteral("仅供负责人阅览。"), QStringLiteral("仅供负责人阅览。")));
        QSignalSpy actions(&controller, &ArchiveController::actionFinished);
        controller.classifySelected(QStringLiteral("field-pass"), QStringLiteral("field-pass"));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.state().value("busy").toBool(), 5000);
        QCOMPARE(controller.state().value("page").toString(), QStringLiteral("locked"));
        QCOMPARE(selectedDocument(controller).value("passwordStatus").toString(), QStringLiteral("protected"));
        QCOMPARE(schemaVersion(archive.filePath(QStringLiteral("archive.sqlite"))), 3);
        QVERIFY(!selectedDocument(controller).contains(QStringLiteral("body")));

        QSignalSpy unlocked(&controller, &ArchiveController::unlockFinished);
        controller.unlockSelected(QStringLiteral("incorrect"));
        QTRY_COMPARE_WITH_TIMEOUT(unlocked.size(), 1, 5000);
        QVERIFY(unlocked.constFirst().at(1).toBool());
        controller.unlockSelected(QStringLiteral("field-pass"));
        QTRY_COMPARE_WITH_TIMEOUT(unlocked.size(), 2, 5000);
        QVERIFY(unlocked.constLast().at(0).toBool());
        QCOMPARE(controller.state().value("page").toString(), QStringLiteral("editor"));
        QCOMPARE(selectedDocument(controller).value("body").toString(), QStringLiteral("仅供负责人阅览。"));
        controller.setActive(true); controller.setActive(false);
        QCOMPARE(controller.state().value("page").toString(), QStringLiteral("locked"));

        const QString key = classificationKey(archive.filePath(QStringLiteral("archive.sqlite")), id);
        QVERIFY(!key.isEmpty()); vault->forget(id + '/' + key);
        controller.closeDocument(); QVERIFY(controller.selectDocument(id));
        QTRY_COMPARE_WITH_TIMEOUT(selectedDocument(controller).value("passwordStatus").toString(), QStringLiteral("missing"), 3000);
        QVERIFY(actions.size() >= 1);
    }

    void pageListEditorAndLockedView()
    {
        QTemporaryDir archive, shelf;
        auto vault = std::make_shared<ArchiveMemoryPasswordVault>();
        ArchiveController controller(nullptr, archive.path(), vault);
        FileShelfController shelfController(nullptr, shelf.path());
        const QString category = controller.addCategory(); QVERIFY(controller.renameCategory(category, QStringLiteral("研究")));
        QVERIFY(controller.selectCategory(category));
        const QString id = controller.createDocument();
        QVERIFY(controller.updateSelected(QStringLiteral("晶体报告"), QStringLiteral("样本稳定。"), QStringLiteral("<p>样本稳定。</p>")));
        controller.closeDocument();

        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("archiveController"), &controller);
        engine.rootContext()->setContextProperty(QStringLiteral("fileShelfController"), &shelfController);
        installFonts(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(ARCHIVE_QML_DIR) + QStringLiteral("/ArchivePage.qml")));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY2(object != nullptr, qPrintable(component.errorString()));
        auto *page = qobject_cast<QQuickItem *>(object.get()); QVERIFY(page);
        QCOMPARE(page->size(), QSizeF(590, 510));
        page->setProperty("reduceMotion", true); page->setProperty("presented", true);
        QVERIFY(findItem(page, QStringLiteral("archiveCategories")));
        QVERIFY(findItem(page, QStringLiteral("archiveDocuments")));

        QQuickWindow window; window.resize(790, 650); window.setColor(QColor(QStringLiteral("#202020")));
        page->setParentItem(window.contentItem()); page->setPosition({80, 60}); window.show(); QTest::qWait(100);
        if (qEnvironmentVariableIsSet("HYPR_ARCHIVE_RENDER")) QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-archive-list.png")));
        QVERIFY(QMetaObject::invokeMethod(page, "navigate", Q_ARG(QVariant, QStringLiteral("open")), Q_ARG(QVariant, id), Q_ARG(QVariant, 1), Q_ARG(QVariant, 0)));
        QCoreApplication::processEvents();
        QCOMPARE(page->property("visualPage").toString(), QStringLiteral("editor"));
        QVERIFY(findItem(page, QStringLiteral("archiveTitle")));
        QVERIFY(findItem(page, QStringLiteral("archiveBody")));
        if (qEnvironmentVariableIsSet("HYPR_ARCHIVE_RENDER")) QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-archive-editor.png")));

        controller.classifySelected(QStringLiteral("test-pass"), QStringLiteral("test-pass"));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.state().value("busy").toBool(), 5000);
        page->setProperty("visualPage", QStringLiteral("locked"));
        page->setProperty("lockedSnapshot", controller.state().value("document"));
        QCoreApplication::processEvents();
        QVERIFY(findItem(page, QStringLiteral("archiveLockedPanel")));
        if (qEnvironmentVariableIsSet("HYPR_ARCHIVE_RENDER")) QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-archive-locked.png")));
        page->setParentItem(nullptr);
        if (!warnings.isEmpty()) for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    ArchiveTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "archive.moc"
