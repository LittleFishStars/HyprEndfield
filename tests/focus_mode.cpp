#include "focusmodecontroller.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

class FocusModeTests final : public QObject
{
    Q_OBJECT

private slots:
    void controllerMatchesSourceTiming()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString settings = directory.filePath(QStringLiteral("focus.ini"));
        double now = 100;

        {
            FocusModeController controller(nullptr, [&now] { return now; }, settings);
            QCOMPARE(controller.state().value("kind").toString(), QStringLiteral("countdown"));
            QCOMPARE(controller.state().value("duration").toInt(), 1800);
            QCOMPARE(controller.state().value("timeText").toString(), QStringLiteral("30:00"));

            QVERIFY(controller.chooseCustomDuration(QStringLiteral("1:30")));
            QCOMPARE(controller.state().value("duration").toInt(), 90);
            QVERIFY(controller.chooseCustomDuration(QStringLiteral("0.5")));
            QCOMPARE(controller.state().value("duration").toInt(), 30);
            QVERIFY(!controller.chooseCustomDuration(QStringLiteral("1:60")));
            QCOMPARE(controller.state().value("duration").toInt(), 30);

            QVERIFY(controller.chooseCountdown(300));
            controller.setActive(true);
            controller.start();
            now = 112.25;
            controller.refresh();
            QCOMPARE(controller.state().value("timeText").toString(), QStringLiteral("04:48"));
            controller.pause();
            QCOMPARE(controller.state().value("phase").toString(), QStringLiteral("paused"));
            QVERIFY(qAbs(controller.state().value("trackedWorkSeconds").toDouble() - 12.25) < 0.0001);

            now = 200;
            controller.resume();
            now = 210;
            controller.stop();
            QCOMPARE(controller.state().value("phase").toString(), QStringLiteral("stopped"));
            QVERIFY(qAbs(controller.state().value("elapsed").toDouble() - 22.25) < 0.0001);
            QVERIFY(qAbs(controller.state().value("trackedWorkSeconds").toDouble() - 22.25) < 0.0001);

            controller.reset();
            controller.chooseStopwatch();
            controller.start();
            now = 275.9;
            controller.refresh();
            QCOMPARE(controller.state().value("timeText").toString(), QStringLiteral("01:05"));
            controller.setActive(false);
            now = 375.9;
            controller.setActive(true);
            QCOMPARE(controller.state().value("timeText").toString(), QStringLiteral("02:45"));
            controller.pause();
            QVERIFY(qAbs(controller.state().value("trackedWorkSeconds").toDouble() - 188.15) < 0.0001);

            controller.reset();
            QVERIFY(controller.chooseCountdown(5));
            now = 400;
            controller.start();
            controller.setActive(false);
            now = 406;
            controller.refresh();
            QCOMPARE(controller.state().value("phase").toString(), QStringLiteral("completed"));
            QCOMPARE(controller.state().value("timeText").toString(), QStringLiteral("00:00"));
            QVERIFY(qAbs(controller.state().value("trackedWorkSeconds").toDouble() - 193.15) < 0.0001);
            controller.shutdown();
        }

        FocusModeController restored(nullptr, [&now] { return now; }, settings);
        QVERIFY(qAbs(restored.state().value("trackedWorkSeconds").toDouble() - 193.15) < 0.0001);
    }

    void pageActions()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        double now = 10;
        FocusModeController controller(nullptr, [&now] { return now; }, directory.filePath("focus.ini"));
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("focusModeController"), &controller);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int font = QFontDatabase::addApplicationFont(
                QStringLiteral(FOCUS_QML_DIR) + "/../assets/fonts/HarmonyOS_Sans_" + weight + ".ttf");
            QVERIFY(font >= 0);
            engine.rootContext()->setContextProperty(
                QStringLiteral("harmony") + weight,
                QVariantMap{{QStringLiteral("name"), QFontDatabase::applicationFontFamilies(font).constFirst()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(FOCUS_QML_DIR) + "/FocusModePage.qml"));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(root.get());
        QVERIFY(item);
        QCOMPARE(item->size(), QSizeF(590, 610));

        QQuickWindow window;
        window.resize(900, 760);
        window.setColor(QColor(QStringLiteral("#151515")));
        item->setParentItem(window.contentItem());
        item->setPosition(QPointF(130, 60));
        root->setProperty("reduceMotion", true);
        root->setProperty("presented", true);
        QVERIFY(controller.active());
        QVERIFY(item->findChild<QObject *>(QStringLiteral("focusModeTimer")));
        QVERIFY(item->findChild<QObject *>(QStringLiteral("focusModePrimary")));
        QVERIFY(item->findChild<QObject *>(QStringLiteral("focusModeSecondary")));
        if (qEnvironmentVariableIsSet("HYPR_FOCUS_RENDER")) {
            window.show();
            QTest::qWait(150);
            QVERIFY(window.grabWindow().save(QStringLiteral(FOCUS_PREVIEW_PATH)));
        }

        QVERIFY(QMetaObject::invokeMethod(root.get(), "primaryAction"));
        QCOMPARE(controller.state().value("phase").toString(), QStringLiteral("running"));
        now = 20;
        controller.refresh();
        QVERIFY(QMetaObject::invokeMethod(root.get(), "secondaryAction"));
        QCOMPARE(controller.state().value("phase").toString(), QStringLiteral("stopped"));
        QVERIFY(QMetaObject::invokeMethod(root.get(), "secondaryAction"));
        QCOMPARE(controller.state().value("phase").toString(), QStringLiteral("idle"));

        QVERIFY(QMetaObject::invokeMethod(root.get(), "chooseKind", Q_ARG(QVariant, 1)));
        QCOMPARE(controller.state().value("kind").toString(), QStringLiteral("stopwatch"));
        QVERIFY(QMetaObject::invokeMethod(root.get(), "primaryAction"));
        now = 25;
        controller.refresh();
        QVERIFY(QMetaObject::invokeMethod(root.get(), "secondaryAction"));
        QCOMPARE(root->property("lapCount").toInt(), 1);

        root->setProperty("presented", false);
        QVERIFY(!controller.active());
        if (!warnings.isEmpty()) {
            for (const auto &warning : warnings) qWarning() << warning;
        }
        QVERIFY(warnings.isEmpty());
        item->setParentItem(nullptr);
    }

    void clockLabelFollowsRunningSession()
    {
        QQmlEngine engine;
        const int font = QFontDatabase::addApplicationFont(
            QStringLiteral(FOCUS_QML_DIR) + "/../assets/fonts/HarmonyOS_Sans_Medium.ttf");
        QVERIFY(font >= 0);
        engine.rootContext()->setContextProperty(
            QStringLiteral("harmonyMedium"),
            QVariantMap{{QStringLiteral("name"), QFontDatabase::applicationFontFamilies(font).constFirst()}});
        engine.rootContext()->setContextProperty(
            QStringLiteral("harmonyBold"),
            QVariantMap{{QStringLiteral("name"), QFontDatabase::applicationFontFamilies(font).constFirst()}});
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(FOCUS_QML_DIR) + "/MenuClock.qml"));
        std::unique_ptr<QObject> root(component.createWithInitialProperties({
            {QStringLiteral("timeText"), QStringLiteral("12:34:56")},
            {QStringLiteral("dateText"), QStringLiteral("SAT NOV 7")}
        }));
        QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto *label = root->findChild<QObject *>(QStringLiteral("menuClockModeLabel"));
        QVERIFY(label);
        QCOMPARE(label->property("text").toString(), QStringLiteral("WORK MODE"));
        root->setProperty("focusActive", true);
        QCOMPARE(label->property("text").toString(), QStringLiteral("FOCUS MODE"));
        root->setProperty("focusActive", false);
        QCOMPARE(label->property("text").toString(), QStringLiteral("WORK MODE"));
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    FocusModeTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "focus_mode.moc"
