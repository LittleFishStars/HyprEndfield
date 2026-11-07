#include "batterycontroller.hpp"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class BatteryTests : public QObject
{
    Q_OBJECT
    static void put(const QString& path, const QByteArray& data)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
    }
private slots:
    void chargeAndMissingBattery()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        QDir root(temp.path());
        root.mkdir("BAT0");
        const auto path = temp.path() + "/BAT0/";
        put(path + "type", "Battery");
        put(path + "present", "1");
        put(path + "capacity", "50");
        put(path + "charge_now", "2000000");
        put(path + "charge_full", "4000000");
        put(path + "charge_full_design", "5000000");
        put(path + "status", "Discharging");
        BatteryController battery(nullptr, temp.path());
        QCOMPARE(battery.state()["current"].toString(), "2,000");
        QCOMPARE(battery.state()["full"].toString(), "4,000");
        QCOMPARE(battery.state()["percentage"].toInt(), 50);
        QVERIFY(!battery.state()["estimated"].toBool());
        put(path + "present", "0");
        battery.refresh();
        QVERIFY(!battery.state()["available"].toBool());
        QCOMPARE(battery.state()["current"].toString(), "—");
    }
    void energyConversionAndUnknownCapacity()
    {
        QTemporaryDir temp;
        QDir(temp.path()).mkdir("BAT0");
        const auto path = temp.path() + "/BAT0/";
        put(path + "type", "Battery");
        put(path + "energy_now", "30000000");
        put(path + "energy_full", "60000000");
        put(path + "voltage_min_design", "15000000");
        BatteryController battery(nullptr, temp.path());
        QCOMPARE(battery.state()["current"].toString(), "2,000");
        QCOMPARE(battery.state()["full"].toString(), "4,000");
        QVERIFY(battery.state()["estimated"].toBool());
        QCOMPARE(battery.state()["percentage"].toInt(), 50);
        put(path + "voltage_min_design", "0");
        battery.refresh();
        QCOMPARE(battery.state()["unit"].toString(), "Wh");
        QCOMPARE(battery.state()["current"].toString(), "30.0");
        QVERIFY(!battery.state()["estimated"].toBool());
        put(path + "energy_full", "invalid");
        put(path + "capacity", "35");
        battery.refresh();
        QCOMPARE(battery.state()["current"].toString(), "—");
        QCOMPARE(battery.state()["percentage"].toInt(), 35);
    }
    void peripheralBatteryIsExcluded()
    {
        QTemporaryDir temp;
        QDir(temp.path()).mkdir("mouse");
        put(temp.path() + "/mouse/type", "Battery");
        put(temp.path() + "/mouse/scope", "Device");
        BatteryController battery(nullptr, temp.path());
        QVERIFY(!battery.state()["available"].toBool());
    }
};
QTEST_GUILESS_MAIN(BatteryTests)
#include "battery.moc"
