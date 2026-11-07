#include "settingscontroller.hpp"
#include "facilitycontroller.hpp"
#include "centerlogo.hpp"
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

class TestLogos final : public QQuickImageProvider {
public:
    TestLogos() : QQuickImageProvider(Image) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override { QImage image(130, 16, QImage::Format_RGBA8888); image.fill(Qt::white); if (size) *size = image.size(); return image; }
};
class SettingsTests final : public QObject {
    Q_OBJECT
    static QQuickItem *findItem(QQuickItem *item, const QString &name) {
        if (item->objectName() == name) return item;
        for (auto *child : item->childItems()) if (auto *found = findItem(child, name)) return found;
        return nullptr;
    }
private slots:
    void initTestCase() {
        for (const auto &weight : {"Regular", "Medium", "Bold"})
            QVERIFY(QFontDatabase::addApplicationFont(QString(SETTINGS_QML_DIR) + "/../assets/fonts/HarmonyOS_Sans_" + weight + ".ttf") >= 0);
    }
    void preferencesAndRollback() {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto prefs = dir.filePath("settings.ini"), startup = dir.filePath("autostart/hud.desktop");
        QSettings seed(prefs, QSettings::IniFormat); seed.setValue("focusMode/trackedWorkSeconds", 1234); seed.setValue("display/uiScale", 1.18); seed.sync();
        SettingsController settings(nullptr, prefs, startup, dir.path());
        QCOMPARE(settings.value("uiScale").toDouble(), 1.18);
        settings.previewLayout("uiScale", 1.6); QCOMPARE(settings.layoutRemaining(), 12); QCOMPARE(settings.value("uiScale").toDouble(), 1.6);
        { QSettings disk(prefs, QSettings::IniFormat); QCOMPARE(disk.value("display/uiScale").toDouble(), 1.18); }
        settings.revertLayout(); QCOMPARE(settings.value("uiScale").toDouble(), 1.18);
        settings.previewLayout("hudOffsetX", .2); settings.confirmLayout(); QCOMPARE(settings.layoutRemaining(), 0);
        settings.previewLayout("hudOffsetX", -.5);
        QTRY_COMPARE_WITH_TIMEOUT(settings.value("hudOffsetX").toDouble(), .2, 13000);
        QCOMPARE(settings.layoutRemaining(), 0);
        QVERIFY(settings.set("clockStyleIndex", 99)); QCOMPARE(settings.value("clockStyleIndex").toInt(), 4);
        QVERIFY(settings.set("uiScale", std::numeric_limits<double>::infinity())); QCOMPARE(settings.value("uiScale").toDouble(), 1.0);
        QVERIFY(settings.set("launchAtLogin", true)); QFile entry(startup); QVERIFY(entry.open(QIODevice::ReadOnly)); const auto contents = entry.readAll(); QVERIFY(contents.contains("--background")); QVERIFY(contents.contains("X-HyprEndfield=true")); entry.close();
        QVERIFY(settings.set("launchAtLogin", false)); QVERIFY(!QFile::exists(startup));
        settings.restoreDefaults();
        QSettings disk(prefs, QSettings::IniFormat); QCOMPARE(disk.value("focusMode/trackedWorkSeconds").toInt(), 1234);
        QCOMPARE(settings.value("hudOffsetX").toDouble(), 0.);
        QVERIFY(settings.beginBootLog()); QVERIFY(!settings.beginBootLog());
        settings.set("languageIndex", 1); QCOMPARE(QCoreApplication::translate("test", "语言"), QString("Language"));
        settings.set("languageIndex", 2); QCOMPARE(QCoreApplication::translate("test", "语言"), QString("语言"));
        settings.set("themeIndex", 1); QVERIFY(settings.tone(QColor("#f4f4f4")).lightnessF() < .2);
        settings.set("accentIndex", 1); QCOMPARE(settings.color(QColor("#fffa00")).name(), QString("#6edfe8"));
    }
    void moduleOrderAndExistingStartup() {
        QTemporaryDir dir;
        const auto startup = dir.filePath("existing.desktop"); QFile file(startup); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("user-owned entry"); file.close();
        SettingsController settings(nullptr, dir.filePath("s.ini"), startup, dir.path());
        QVERIFY(!settings.set("launchAtLogin", true)); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("user-owned entry"));
        settings.moveModule("calendar", 0); QCOMPARE(settings.value("moduleOrder").toStringList().first(), QString("calendar"));
        for (const auto &key : SettingsController::defaultModules()) settings.showModule(key, false);
        QCOMPARE(settings.value("hiddenModules").toStringList().size(), SettingsController::defaultModules().size() - 1);
        settings.resetModules(); QCOMPARE(settings.value("moduleOrder").toStringList(), SettingsController::defaultModules()); QVERIFY(settings.value("hiddenModules").toStringList().isEmpty());
        QVERIFY(!settings.icons().isEmpty());
    }
    void batteryEdgesAndDuration() {
        QTemporaryDir dir; SettingsController settings(nullptr, dir.filePath("s.ini"), dir.filePath("autostart"), dir.path());
        settings.set("batteryDuration", 1);
        settings.updateBattery({{"available", true}, {"plugged", true}}); QVERIFY(!settings.batteryAlertVisible());
        settings.updateBattery({{"available", true}, {"plugged", false}}); QVERIFY(settings.batteryAlertVisible());
        QTRY_VERIFY_WITH_TIMEOUT(!settings.batteryAlertVisible(), 1500);
        settings.set("batteryMethod", 1); QVERIFY(settings.batteryAlertVisible());
        settings.set("batteryMethod", 2); QVERIFY(!settings.batteryAlertVisible());
        settings.editBatteryPosition(); QVERIFY(settings.editingBatteryPosition());
        settings.placeBattery(.3, .6); QCOMPARE(settings.value("batteryX").toDouble(), .3); QVERIFY(!settings.editingBatteryPosition());
        settings.editBatteryPosition(); settings.cancelBatteryPosition(); QVERIFY(!settings.editingBatteryPosition());
    }
    void facilityHysteresis() {
        FacilityController facility(nullptr, false); facility.setEnabled(true);
        const auto sample = [](double available, double used) { return QVariantMap{{"disk", QVariantMap{{"total", 1000}, {"available", available}, {"name", "/"}}}, {"mem", QVariantMap{{"total", 1000}, {"used", used}}}}; };
        facility.accept(sample(99, 900), 0); QCOMPARE(facility.alerts().size(), 2);
        facility.dismiss("disk"); QCOMPARE(facility.alerts().size(), 1);
        facility.accept(sample(95, 910), 60); QCOMPARE(facility.alerts().size(), 1);
        facility.accept(sample(120, 800), 120); QCOMPARE(facility.alerts().size(), 1);
        facility.accept(sample(120, 800), 239); QCOMPARE(facility.alerts().size(), 1);
        facility.accept(sample(120, 800), 240); QVERIFY(facility.alerts().isEmpty());
        facility.accept(sample(99, 800), 300); QCOMPARE(facility.alerts().size(), 1);
        facility.markPresented("disk"); QVERIFY(!facility.alerts().first().toMap().value("isNew").toBool());
        facility.setEnabled(false); QVERIFY(facility.alerts().isEmpty());
    }
    void logoCropping() {
        QImage image(40, 40, QImage::Format_RGBA8888); image.fill(Qt::transparent);
        for (int y = 10; y < 30; ++y) for (int x = 4; x < 36; ++x) image.setPixelColor(x, y, QColor("#ff0000"));
        const auto art = centerLogoArtwork(image); QCOMPARE(art.size(), QSize(32, 20)); QCOMPARE(art.pixelColor(16, 10), QColor("#ff0000"));
        image.fill(Qt::transparent); QVERIFY(centerLogoArtwork(image).isNull());
    }
    void pagesAndBindings() {
        QTemporaryDir dir; SettingsController settings(nullptr, dir.filePath("s.ini"), dir.filePath("startup"), dir.path());
        settings.set("languageIndex", 2); settings.set("reduceMotion", true);
        QQmlEngine engine; engine.addImageProvider("settings-logos", new TestLogos);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("settingsController", &settings);
        for (const auto *font : {"harmonyRegular", "harmonyMedium", "harmonyBold"}) engine.rootContext()->setContextProperty(font, QVariantMap{{"name", "HarmonyOS Sans"}});
        QObject::connect(&settings, &SettingsController::languageChanged, &engine, &QQmlEngine::retranslate);
        for (const auto &page : {"SystemPage", "DisplayPage", "AboutPage"}) {
            QQmlComponent component(&engine, QUrl::fromLocalFile(QString(SETTINGS_QML_DIR) + '/' + page + ".qml"));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            std::unique_ptr<QObject> object(component.createWithInitialProperties({{"settings", QVariant::fromValue(&settings)}}));
            QVERIFY2(object, qPrintable(component.errorString()));
            auto *item = qobject_cast<QQuickItem *>(object.get()); QVERIFY(item);
            QQuickWindow window; window.setColor(QColor("#191B1D")); window.resize(630, 540); item->setParentItem(window.contentItem()); window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
            QDir().mkpath(QString(SETTINGS_PREVIEW_DIR));
            QVERIFY(window.grabWindow().save(QString(SETTINGS_PREVIEW_DIR) + '/' + page + ".png"));
            if (QString(page) == "SystemPage") {
                QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(540, 218)); QVERIFY(settings.value("closeOnFocusLost").toBool());
                QVERIFY(object->setProperty("section", "modules")); QTest::qWait(20); QVERIFY(findItem(item, "module-notes"));
                QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(456, 106));
                QCOMPARE(settings.value("moduleOrder").toStringList().at(1), QString("notes"));
                QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(496, 106));
                QVERIFY(settings.value("hiddenModules").toStringList().contains("fileShelf"));
                object->setProperty("confirming", true); QTest::keyClick(&window, Qt::Key_Escape); QVERIFY(!object->property("confirming").toBool());
            } else if (QString(page) == "DisplayPage") {
                QVERIFY(object->setProperty("section", "battery")); QTest::qWait(20);
                QVERIFY(window.grabWindow().save(QString(SETTINGS_PREVIEW_DIR) + "/BatterySettings.png"));
                QVERIFY(object->setProperty("section", "appIcon")); QTest::qWait(20);
                QVERIFY(window.grabWindow().save(QString(SETTINGS_PREVIEW_DIR) + "/ApplicationIcons.png"));
                QVERIFY(object->setProperty("section", "main"));
                object->setProperty("confirmFacility", true); QTest::keyClick(&window, Qt::Key_Escape); QVERIFY(!object->property("confirmFacility").toBool());
            } else { QVERIFY(object->setProperty("credits", true)); }
            settings.set("themeIndex", 1); settings.set("languageIndex", 1); window.setColor(QColor("#E6E6E6")); QTest::qWait(30);
            QVERIFY(window.grabWindow().save(QString(SETTINGS_PREVIEW_DIR) + '/' + page + "-light.png"));
            settings.set("languageIndex", 2); settings.set("themeIndex", 0); window.setColor(QColor("#191B1D")); QTest::qWait(30);
            QVERIFY(window.grabWindow().save(QString(SETTINGS_PREVIEW_DIR) + '/' + page + "-subpage.png"));
            item->setParentItem(nullptr);
        }
        QQmlComponent slider(&engine, QUrl::fromLocalFile(QString(SETTINGS_QML_DIR) + "/HudSlider.qml"));
        QVERIFY2(slider.isReady(), qPrintable(slider.errorString()));
        std::unique_ptr<QObject> object(slider.create()); QVERIFY(object);
        QVariant result; QVERIFY(QMetaObject::invokeMethod(object.get(), "formattedValue", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 12)));
        QCOMPARE(result.toString(), QString("12%")); object->setProperty("valueFormat", "seconds");
        QVERIFY(QMetaObject::invokeMethod(object.get(), "formattedValue", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, 12))); QCOMPARE(result.toString(), QString("12 s"));
        QVERIFY(warnings.isEmpty());
    }
    void mainSettingsBinding() {
        QTemporaryDir dir; SettingsController settings(nullptr, dir.filePath("s.ini"), dir.filePath("startup"), dir.path());
        QFile main(QString(SETTINGS_QML_DIR) + "/Main.qml"); QVERIFY(main.open(QIODevice::ReadOnly));
        const QString source = QString::fromUtf8(main.readAll());
        const auto start = source.indexOf("    WatchShell {"); QVERIFY(start >= 0);
        const auto end = source.indexOf("\n    }", start); QVERIFY(end > start);
        // Exercise the actual Main.qml shell binding without loading unrelated modules.
        QString shell = source.mid(start, end - start + 6);
        shell.replace("WatchShell {", "Item { property var displaySettings; objectName: 'shellBinding';");
        QQmlEngine engine; QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("settingsController", &settings);
        QQmlComponent component(&engine);
        component.setData(("import QtQuick\nItem { id: root; width: 1600; height: 900; readonly property var displaySettings: settingsController;\n" + shell + "\n}").toUtf8(), QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> object(component.create()); QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = object->findChild<QQuickItem *>("shellBinding"); QVERIFY(item);
        QCOMPARE(item->property("displaySettings").value<QObject *>(), &settings);
        settings.set("uiScale", 1.18);
        QCOMPARE(item->scale(), std::min({1600. / 1100, 900. / 740, 1.75}) * 1.18);
        QVERIFY(warnings.isEmpty());
    }
    void overlaysAndClock() {
        QTemporaryDir dir; SettingsController settings(nullptr, dir.filePath("s.ini"), dir.filePath("startup"), dir.path());
        settings.set("languageIndex", 2); settings.set("reduceMotion", true);
        FacilityController facility(nullptr, false); facility.setEnabled(true);
        facility.accept({{"disk", QVariantMap{{"total", 1000}, {"available", 50}, {"name", "/"}}}}, 0);
        QQmlEngine engine; QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        auto *context = engine.rootContext();
        context->setContextProperty("settingsController", &settings); context->setContextProperty("facilityController", &facility);
        for (const auto *font : {"harmonyRegular", "harmonyMedium", "harmonyBold"}) context->setContextProperty(font, QVariantMap{{"name", "HarmonyOS Sans"}});
        context->setContextProperty("batteryController", QVariantMap{{"state", QVariantMap{{"available", true}, {"percentage", 75}, {"plugged", true}}}});
        context->setContextProperty("calendarController", QVariantMap{{"state", QVariantMap{{"events", QVariantList{}}}}});
        context->setContextProperty("focusModeController", QVariantMap{{"state", QVariantMap{{"trackedWorkSeconds", 3600}}}});
        context->setContextProperty("appController", QVariantMap{{"localUserName", "TEST"}});
        QQuickWindow window; window.setColor(QColor("#191B1D")); window.resize(630, 540); window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        for (const auto &name : {"BootLog", "FacilityTags", "FacilityBanner", "RedactionBlock", "NotesFormatBar", "MenuClock"}) {
            QQmlComponent component(&engine, QUrl::fromLocalFile(QString(SETTINGS_QML_DIR) + '/' + name + ".qml"));
            QVERIFY2(component.isReady(), qPrintable(component.errorString()));
            QVariantMap initial;
            if (QString(name) == "MenuClock") initial = {{"timeText", "16:32"}, {"dateText", "SAT NOV 7"}, {"reduceMotion", true}};
            std::unique_ptr<QObject> object(component.createWithInitialProperties(initial)); QVERIFY2(object, qPrintable(component.errorString()));
            auto *item = qobject_cast<QQuickItem *>(object.get()); QVERIFY(item); item->setParentItem(window.contentItem());
            if (QString(name) == "BootLog") { QVERIFY(QMetaObject::invokeMethod(object.get(), "play")); object->setProperty("revealed", 10); QTest::qWait(330); }
            if (QString(name) == "FacilityBanner") { object->setProperty("presented", true); QVERIFY(object->property("active").toBool()); }
            if (QString(name) == "RedactionBlock") { item->setWidth(400); item->setHeight(250); object->setProperty("content", "Private note content for recording mode"); }
            QTest::qWait(30);
            QVERIFY(window.grabWindow().save(QString(SETTINGS_PREVIEW_DIR) + '/' + name + ".png"));
            if (QString(name) == "MenuClock") for (int i = 0; i < 5; ++i) { object->setProperty("styleIndex", i); QTest::qWait(20); QVERIFY(window.grabWindow().save(QString(SETTINGS_PREVIEW_DIR) + "/Clock-" + QString::number(i) + ".png")); }
            if (QString(name) == "BootLog") { QVERIFY(QMetaObject::invokeMethod(object.get(), "skip")); QTRY_VERIFY_WITH_TIMEOUT(!object->property("playing").toBool(), 600); }
            item->setParentItem(nullptr);
        }
        QVERIFY(warnings.isEmpty());
    }
};
QTEST_MAIN(SettingsTests)
#include "settings.moc"
