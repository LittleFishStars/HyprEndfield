#include "batterycontroller.hpp"
#include <QDir>
#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

class BatteryPageTests : public QObject
{
    Q_OBJECT
private slots:
    void clickMorphAndBatteryRemoval()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QDir(temp.path()).mkdir("BAT0");
        const auto write = [&](const QString& name, const QByteArray& value)
        {
            QFile file(temp.path() + "/BAT0/" + name);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(value);
        };
        write("type", "Battery");
        write("capacity", "100");
        write("charge_now", "3961000");
        write("charge_full", "3966000");
        BatteryController battery(nullptr, temp.path());
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("batteryController", &battery);
        engine.rootContext()->setContextProperty("harmonyBold", QVariantMap { { "name", "sans-serif" } });
        engine.rootContext()->setContextProperty("harmonyMedium", QVariantMap { { "name", "sans-serif" } });
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(BATTERY_QML_PATH)));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> object(component.create());
        QVERIFY(object);
        auto* badge = qobject_cast<QQuickItem*>(object.get());
        QVERIFY(badge);
        QQuickWindow window;
        window.resize(300, 84);
        badge->setParentItem(window.contentItem());
        badge->setProperty("presented", true);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(150, 42));
        QVERIFY(badge->property("expanded").toBool());
        QTRY_VERIFY(badge->property("morph").toDouble() > .99);
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, QPoint(150, 42));
        QVERIFY(badge->property("expanded").toBool());
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(150, 42));
        QVERIFY(!badge->property("expanded").toBool());
        QTRY_VERIFY(badge->property("morph").toDouble() < .01);
        write("present", "0");
        battery.refresh();
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(150, 42));
        QVERIFY(!badge->property("expanded").toBool());
        badge->setParentItem(nullptr);
    }
};
QTEST_MAIN(BatteryPageTests)
#include "battery_page.moc"
