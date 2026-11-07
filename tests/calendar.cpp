#include "calendarcontroller.hpp"

#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJSEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>

class CalendarTests final : public QObject
{
    Q_OBJECT
    static QQuickItem *findItem(QQuickItem *root, const QString &name) {
        if (root->objectName() == name) return root;
        for (auto *child : root->childItems()) if (auto *found = findItem(child, name)) return found;
        return nullptr;
    }

private slots:
    void persistenceAndLimits() {
        QTemporaryDir directory;
        CalendarController controller(nullptr, directory.path(), false);
        QCOMPARE(controller.state()["total"].toInt(), 0);
        QVERIFY(!controller.saveEvent("  ", "", QDate::currentDate().toString(Qt::ISODate)));
        QVERIFY(!controller.saveEvent("Bad", "", "2026-02-30"));
        const QString tomorrow = QDate::currentDate().addDays(1).toString(Qt::ISODate);
        QVERIFY(controller.saveEvent("测试事项", "详细内容", tomorrow));
        auto events = controller.state()["events"].toList(); QCOMPARE(events.size(), 1);
        const QString id = events[0].toMap()["id"].toString();
        QVERIFY(controller.saveEvent("修改事项", "新的详情", tomorrow, id));
        QCOMPARE(controller.state()["events"].toList()[0].toMap()["title"].toString(), QString("修改事项"));

        CalendarController reloaded(nullptr, directory.path(), false);
        QCOMPARE(reloaded.state()["total"].toInt(), 1);
        QCOMPARE(reloaded.state()["events"].toList()[0].toMap()["details"].toString(), QString("新的详情"));
        QVERIFY(reloaded.removeEvent(id));
        QCOMPARE(reloaded.state()["total"].toInt(), 0);

        for (int index = 0; index < 30; ++index)
            QVERIFY(reloaded.saveEvent(QString("事项 %1").arg(index), "", QDate::currentDate().addDays(index).toString(Qt::ISODate)));
        QVERIFY(!reloaded.saveEvent("超出数量", "", QDate::currentDate().addDays(31).toString(Qt::ISODate)));
        QCOMPARE(reloaded.state()["total"].toInt(), 30);
    }

    void preservesExternalChanges() {
        QTemporaryDir directory;
        CalendarController controller(nullptr, directory.path(), false);
        QVERIFY(controller.saveEvent("初始事项", "", QDate::currentDate().toString(Qt::ISODate)));
        QFile file(directory.path() + "/calendar.json"); QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write(" "), qint64(1)); file.close();
        QVERIFY(!controller.saveEvent("另一个事项", "", QDate::currentDate().toString(Qt::ISODate)));
        QVERIFY(controller.state()["error"].toString().contains("其他程序"));
        QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.readAll().endsWith(' '));
    }

    void format() {
        QFile file(QStringLiteral(CALENDAR_QML_DIR) + "/CalendarFormat.js"); QVERIFY(file.open(QIODevice::ReadOnly));
        QString script = QString::fromUtf8(file.readAll()); script.remove(".pragma library");
        QJSEngine engine; QVERIFY(!engine.evaluate(script).isError());
        QCOMPARE(engine.evaluate("daysInMonth(2024, 2)").toInt(), 29);
        QCOMPARE(engine.evaluate("key(2026, 11, 7)").toString(), QString("2026-11-07"));
        QCOMPARE(engine.evaluate("cells(2026, 11).length").toInt(), 42);
        QCOMPARE(engine.evaluate("cells(2026, 11)[0].key").toString(), QString("2026-11-01"));
        QCOMPARE(engine.evaluate("weekdayName(2026, 11, 7)").toString(), QString("星期六"));
    }

    void page() {
        QTemporaryDir directory;
        CalendarController controller(nullptr, directory.path(), false);
        const QString today = QDate::currentDate().toString(Qt::ISODate);
        QVERIFY(controller.saveEvent("牙医", "09:30", today));
        QVERIFY(controller.saveEvent("提交报告", "季度总结", today));

        QQmlEngine engine; engine.rootContext()->setContextProperty("calendarController", &controller);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int font = QFontDatabase::addApplicationFont(QStringLiteral(CALENDAR_QML_DIR) + "/../assets/fonts/HarmonyOS_Sans_" + weight + ".ttf");
            engine.rootContext()->setContextProperty(QString("harmony%1").arg(weight), QVariantMap{{"name", QFontDatabase::applicationFontFamilies(font).first()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CALENDAR_QML_DIR) + "/CalendarPage.qml"));
        QQuickWindow window; window.resize(960, 760); window.setColor(QColor("#222222"));
        std::unique_ptr<QObject> root(component.create()); QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(root.get()); QVERIFY(item); QCOMPARE(item->size(), QSizeF(590, 510));
        item->setParentItem(window.contentItem()); item->setPosition({50, 20}); item->setTransformOrigin(QQuickItem::TopLeft); item->setScale(1.4);
        window.show(); root->setProperty("reduceMotion", true); root->setProperty("presented", true);
        QVERIFY(controller.active()); QCOMPARE(root->property("selectedDay").toString(), today);
        auto *eventList = findItem(item, "calendarEvents"); QVERIFY(eventList); QCOMPARE(eventList->position(), QPointF(0, 55));
        QTest::qWait(150);
        if (qEnvironmentVariableIsSet("HYPR_CALENDAR_RENDER")) {
            root->setProperty("toastOpen", false);
            QVERIFY(window.grabWindow().save("/tmp/hyprendfield-calendar.png"));
            auto *picker = findItem(item, "calendarMonthPicker"); QVERIFY(picker); picker->setVisible(true);
            QVERIFY(window.grabWindow().save("/tmp/hyprendfield-calendar-picker.png")); picker->setVisible(false);
        }
        const QString firstId = controller.state()["events"].toList()[0].toMap()["id"].toString();
        root->setProperty("editingId", firstId);
        QVERIFY(QMetaObject::invokeMethod(root.get(), "requestDelete"));
        auto *confirm = findItem(item, "calendarConfirm"); QVERIFY(confirm); QVERIFY(confirm->isVisible());
        if (qEnvironmentVariableIsSet("HYPR_CALENDAR_RENDER"))
            QVERIFY(window.grabWindow().save("/tmp/hyprendfield-calendar-confirm.png"));
        confirm->setVisible(false); root->setProperty("editingId", "");
        auto *title = findItem(item, "calendarTitle"); QVERIFY(title); title->setProperty("text", "新增事项");
        QVERIFY(QMetaObject::invokeMethod(root.get(), "addEvent"));
        QCOMPARE(controller.state()["total"].toInt(), 3);
        QCOMPARE(title->property("text").toString(), QString());
        const int originalMonth = root->property("shownMonth").toInt();
        QVERIFY(QMetaObject::invokeMethod(root.get(), "changeMonth", Q_ARG(QVariant, 1)));
        QVERIFY(root->property("shownMonth").toInt() != originalMonth || originalMonth == 12);
        QVERIFY(QMetaObject::invokeMethod(root.get(), "goToday"));
        QCOMPARE(root->property("selectedDay").toString(), today);
        root->setProperty("reduceMotion", false);
        QVERIFY(QMetaObject::invokeMethod(root.get(), "changeMonth", Q_ARG(QVariant, 1)));
        QCOMPARE(root->property("motionPhase").toString(), QString("leave"));
        QTRY_COMPARE(root->property("motionPhase").toString(), QString("idle"));
        root->setProperty("presented", false); QVERIFY(!controller.active());
        if (!warnings.isEmpty()) for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty()); item->setParentItem(nullptr);
    }
};

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsSet("HYPR_CALENDAR_RENDER")) {
        qputenv("QT_QUICK_BACKEND", "rhi"); QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        QSurfaceFormat format; format.setSamples(4); QSurfaceFormat::setDefaultFormat(format);
    }
    QGuiApplication application(argc, argv);
    CalendarTests tests; return QTest::qExec(&tests, argc, argv);
}

#include "calendar.moc"
