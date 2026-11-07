#include "eventlogcontroller.hpp"
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class EventLogTests : public QObject {
    Q_OBJECT
    static QByteArray bytes(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
private slots:
    void storageAndMetadata() {
        QTemporaryDir dir;
        const QString path = dir.path() + "/EventLog/events.json";
        {
            EventLogController log(nullptr, dir.path());
            log.record("moduleOpened", {{"module", "notes"}});
            log.record("clipboardCopied", {{"kind", "text"}, {"content", "SECRET"}, {"cred", "SECRET"}});
            log.record("shelfAdded", {{"filename", "/private/path/文档.txt"}});
            log.record("appShortcutOpened", {{"app", "Dolphin\nTest"}});
            log.record("mapRecentered"); log.record("invalid");
            QCOMPARE(log.state()["count"].toInt(), 3);
            log.flush();
            const auto saved = bytes(path); QVERIFY(!saved.contains("SECRET")); QVERIFY(!saved.contains("/private/path"));
            QCOMPARE(QJsonDocument::fromJson(saved).object()["events"].toArray().size(), 4);
        }
        EventLogController restored(nullptr, dir.path()); QCOMPARE(restored.state()["count"].toInt(), 3);
        for (int i = 0; i < 510; ++i) restored.record("noteAction", {{"action", "createdText"}});
        QCOMPARE(restored.state()["count"].toInt(), 500);
        restored.clear(); restored.flush(); QCOMPARE(restored.state()["count"].toInt(), 0);
        QCOMPARE(QJsonDocument::fromJson(bytes(path)).object()["events"].toArray().size(), 0);
    }
    void originalFilePreserved() {
        for (const QByteArray initial : {QByteArray("broken"), QByteArray("{\"version\":2,\"events\":[]}")}) {
            QTemporaryDir dir; QDir().mkpath(dir.path() + "/EventLog");
            const QString path = dir.path() + "/EventLog/events.json";
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(initial); file.close();
            EventLogController log(nullptr, dir.path()); QVERIFY(!log.state()["status"].toString().isEmpty());
            log.recordAction("calendarAction", "created"); log.flush(); QCOMPARE(bytes(path), initial);
            log.clear(); log.flush(); QCOMPARE(bytes(path), initial);
        }
    }
    void baselinesAndDebounce() {
        QTemporaryDir dir; EventLogController log(nullptr, dir.path());
        QVariantMap work{{"phase", "idle"}, {"kind", "countdown"}, {"duration", 1800}};
        log.observeWork(work); work["phase"] = "running"; log.observeWork(work);
        log.observeWork(work); work["phase"] = "paused"; log.observeWork(work);
        work["phase"] = "running"; log.observeWork(work); work["phase"] = "completed"; log.observeWork(work);
        QCOMPARE(log.state()["count"].toInt(), 4);
        QVariantMap battery{{"available", true}, {"plugged", false}, {"stateKey", "battery"}, {"percentage", 50}};
        log.observeBattery(battery); battery["percentage"] = 49; log.observeBattery(battery);
        QCOMPARE(log.state()["count"].toInt(), 4);
        battery["plugged"] = true; battery["stateKey"] = "charging"; log.observeBattery(battery);
        QCOMPARE(log.state()["count"].toInt(), 5);
        for (int i = 0; i < 10; ++i) log.recordAction("noteAction", "editedText", true);
        QTest::qWait(700); QCOMPARE(log.state()["count"].toInt(), 6);
        log.flush(); QVERIFY(log.state()["status"].toString().isEmpty());
    }
    void pageAndScroll() {
        QTemporaryDir dir; EventLogController log(nullptr, dir.path()); QQmlEngine engine;
        engine.rootContext()->setContextProperty("eventLogController", &log);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int id = QFontDatabase::addApplicationFont(QString(EVENT_LOG_SOURCE_DIR) + "/assets/fonts/HarmonyOS_Sans_" + weight + ".ttf");
            QVERIFY(id >= 0);
            engine.rootContext()->setContextProperty(QString("harmony") + weight, QVariantMap{{"name", QFontDatabase::applicationFontFamilies(id).first()}});
        }
        for (int i = 0; i < 30; ++i) log.record("shelfAdded", {{"filename", QString("Demo-%1.txt").arg(i)}});
        log.record("workCompleted", {{"kind", "countdown"}, {"seconds", 1800}});
        log.record("powerConnected", {{"percentage", 76}});
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString(EVENT_LOG_SOURCE_DIR) + "/qml/EventLogPage.qml"));
        std::unique_ptr<QObject> page(component.create()); QVERIFY2(page, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(page.get());
        QQuickWindow window; window.resize(690, 600); window.setColor(QColor("#181818")); item->setParentItem(window.contentItem()); item->setPosition({50, 40});
        window.show(); page->setProperty("presented", true); QTest::qWait(100);
        auto *rows = page->findChild<QObject *>("eventLogRows"); QVERIFY(rows);
        rows->setProperty("contentY", 200); log.recordAction("minigameAction", "started"); QTest::qWait(20);
        QCOMPARE(rows->property("contentY").toDouble(), 246.);
        QMetaObject::invokeMethod(page.get(), "selectFilter", Q_ARG(QVariant, 1)); QTest::qWait(20); QCOMPARE(rows->property("count").toInt(), 1);
        QMetaObject::invokeMethod(page.get(), "selectFilter", Q_ARG(QVariant, 0)); QTest::qWait(20);
        if (qEnvironmentVariableIsSet("HYPR_EVENT_RENDER")) QVERIFY(window.grabWindow().save(QString(EVENT_LOG_PREVIEW_DIR) + "/event-log-page.png"));
        for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty()); item->setParentItem(nullptr);
    }
};
int main(int argc, char **argv) { QGuiApplication app(argc, argv); EventLogTests tests; return QTest::qExec(&tests, argc, argv); }
#include "event_log.moc"
