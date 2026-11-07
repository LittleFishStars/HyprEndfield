#include "settingscontroller.hpp"
#include "facilitycontroller.hpp"
#include "centerlogo.hpp"
#include "applicationcontroller.hpp"
#include <LayerShellQt/Window>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
    void waylandSurfaceModes() {
        if (!qEnvironmentVariableIsSet("HYPR_TEST_WAYLAND_SURFACE")) QSKIP("Opt-in test on the real compositor");
        QTemporaryDir dir; SettingsController settings(nullptr, dir.filePath("s.ini"), dir.filePath("startup"), dir.path());
        QQuickWindow window; window.setColor(Qt::transparent); window.resize(96,96);
        ApplicationController controller; controller.setSettings(&settings); controller.setWindow(&window);
        auto *layer = LayerShellQt::Window::get(&window); layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone); layer->setExclusiveZone(-1); layer->setScope(controller.layerScope());
        const auto compositorScope = [] {
            QProcess process; process.start("hyprctl", {"-j", "layers"}); if (!process.waitForFinished(2000)) return QString();
            const auto monitors = QJsonDocument::fromJson(process.readAllStandardOutput()).object();
            for (const auto &monitor : monitors) for (const auto &level : monitor.toObject().value("levels").toObject()) for (const auto &entry : level.toArray()) {
                const auto object = entry.toObject(); if (object.value("pid").toInteger() == QCoreApplication::applicationPid()) return object.value("namespace").toString();
            }
            return QString();
        };
        window.show(); QTRY_COMPARE_WITH_TIMEOUT(compositorScope(), QString("hyprendfield"), 3000);
        settings.set("lowPowerVisualMode", true); QTRY_COMPARE_WITH_TIMEOUT(compositorScope(), QString("hyprendfield-low-power"), 3000);
        controller.setProjectionActive(true); QTRY_COMPARE_WITH_TIMEOUT(compositorScope(), QString("hyprendfield-projection"), 3000);
        controller.setProjectionActive(false); QTRY_COMPARE_WITH_TIMEOUT(compositorScope(), QString("hyprendfield-low-power"), 3000);
        settings.set("lowPowerVisualMode", false); QTRY_COMPARE_WITH_TIMEOUT(compositorScope(), QString("hyprendfield"), 3000);
        QVERIFY(window.isVisible()); window.hide();
    }
    void hardwareTemperature() {
        QTemporaryDir dir; const auto path = dir.filePath("class/hwmon/hwmon0/"); QVERIFY(QDir().mkpath(path));
        for (const auto &[name, data] : QList<QPair<QString, QByteArray>>{{"name", "coretemp"}, {"temp1_input", "101000"}, {"temp1_max", "100000"}}) {
            QFile file(path + name); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(data);
        }
        const auto reading = FacilityController::readTemperature(dir.path()); QCOMPARE(reading.value("temperature").toDouble(), 101.);
        FacilityController facility(nullptr, false); facility.setEnabled(true); facility.accept({{"heat", reading}}, 0);
        QCOMPARE(facility.alerts().size(), 1); QCOMPARE(facility.alerts().first().toMap().value("kind").toString(), QString("heat"));
        auto cool = reading; cool["temperature"] = 94.; facility.accept({{"heat", cool}}, 30); facility.accept({{"heat", cool}}, 150);
        QVERIFY(facility.alerts().isEmpty());
        QVERIFY(QFile::remove(path + "temp1_max")); QVERIFY(FacilityController::readTemperature(dir.path()).isEmpty());
    }
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
        QCOMPARE(QCoreApplication::translate("test", "已创建视频便笺"), QString("Video note created"));
        QCOMPARE(QCoreApplication::translate("test", "%1 · 硬件温度上限 %2°C"), QString("%1 · Hardware temperature limit %2°C"));
        for (const auto &[index, expected] : QList<QPair<int, QString>>{{3,"裝置溫度過高"}, {4,"デバイスの温度が高すぎます"}, {5,"장치 온도가 너무 높습니다"}}) {
            settings.set("languageIndex", index); QCOMPARE(QCoreApplication::translate("test", "设备温度过高"), expected);
        }
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
    void moduleHeaderNumbering() {
        const QList<QPair<QString, QString>> pages{
            {"NotesPage", "notes"}, {"FileShelfPage", "fileShelf"}, {"ClipboardPage", "clipboard"},
            {"ArchivePage", "archive"}, {"VolumePage", "volume"}, {"ReaderPage", "reader"},
            {"CalendarPage", "calendar"}, {"ProjectionPage", "projection"}, {"MediaAssemblyPage", "mediaAssembly"},
            {"OrbiPomPage", "minigame"}, {"EventLogPage", "eventLog"}, {"AccountPage", "account"},
            {"AppShortcutsPage", "addApp"}, {"ProfilePage", "profile"}};
        QString source = "import QtQuick\nimport \"ModuleNumbering.js\" as ModuleNumbering\nQtObject {\n";
        QStringList defaultOrder, consecutive;
        for (const auto &[page, key] : pages) {
            QFile file(QString(SETTINGS_QML_DIR) + '/' + page + ".qml"); QVERIFY(file.open(QIODevice::ReadOnly));
            const QString pageSource = QString::fromUtf8(file.readAll());
            const QString binding = "ModuleNumbering.code(\"" + key + "\")";
            QVERIFY2(pageSource.contains("import \"ModuleNumbering.js\" as ModuleNumbering"), qPrintable(page));
            QVERIFY2(pageSource.contains("text: " + binding) || pageSource.contains("code: " + binding), qPrintable(page));
            source += "property string " + key + ": " + binding + "\n";
            defaultOrder.append(key);
            consecutive.append(QString("MOD-%1").arg(defaultOrder.size(), 2, 10, QChar('0')));
        }
        source += "property string playback: ModuleNumbering.code(\"nowPlaying\")\n"
                  "property string focus: ModuleNumbering.code(\"workMode\")\n"
                  "property string unknown: ModuleNumbering.code(\"missing\")\n}";
        for (const bool configured : {false, true}) {
            QTemporaryDir dir;
            SettingsController settings(nullptr, dir.filePath("s.ini"), dir.filePath("startup"), dir.path());
            QQmlEngine engine; QSignalSpy warnings(&engine, &QQmlEngine::warnings);
            if (configured) engine.rootContext()->setContextProperty("settingsController", &settings);
            QQmlComponent component(&engine);
            component.setData(source.toUtf8(), QUrl::fromLocalFile(QString(SETTINGS_QML_DIR) + "/NumberingTest.qml"));
            std::unique_ptr<QObject> object(component.create()); QVERIFY2(object, qPrintable(component.errorString()));
            const auto codes = [&object](const QStringList &order) {
                QStringList values;
                for (const auto &key : order) values.append(object->property(key.toUtf8().constData()).toString());
                return values;
            };
            QCOMPARE(codes(defaultOrder), consecutive);
            QCOMPARE(codes({"playback", "focus", "unknown"}), QStringList({"", "", ""}));
            if (configured) {
                auto order = defaultOrder;
                settings.moveModule("mediaAssembly", 0); order.move(order.indexOf("mediaAssembly"), 0);
                QCOMPARE(codes(order), consecutive);
                // Headerless pages do not create gaps in the displayed numbers.
                settings.moveModule("nowPlaying", 0); QCOMPARE(codes(order), consecutive);
                settings.showModule("notes", false); order.removeAll("notes"); order.append("notes");
                QCOMPARE(codes(order), consecutive);
                settings.showModule("account", false); order.removeAll("account"); order.append("account");
                QCOMPARE(codes(order), consecutive);
                // A hidden account page remains reachable from the profile, with a unique number.
                QCOMPARE(object->property("account").toString(), QString("MOD-14"));
                SettingsController restored(nullptr, dir.filePath("s.ini"), dir.filePath("startup"), dir.path());
                engine.rootContext()->setContextProperty("settingsController", &restored);
                QCOMPARE(codes(order), consecutive);
                restored.resetModules(); QCOMPARE(codes(defaultOrder), consecutive);
                engine.rootContext()->setContextProperty("settingsController", &settings);
                settings.showModule("notes", true); settings.showModule("account", true);
                order = defaultOrder; order.move(order.indexOf("mediaAssembly"), 0);
                QCOMPARE(codes(order), consecutive);
                settings.resetModules(); QCOMPARE(codes(defaultOrder), consecutive);
            }
            QVERIFY(warnings.isEmpty());
        }
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
        shell.replace("WatchShell {", "Item { property var displaySettings; property var projectionWorkspace; objectName: 'shellBinding';");
        QQmlEngine engine; QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        engine.rootContext()->setContextProperty("settingsController", &settings);
        QQmlComponent component(&engine);
        component.setData(("import QtQuick\nItem { id: root; width: 1600; height: 900; readonly property var displaySettings: settingsController; QtObject { id: fullProjection; property bool opened: false }\n" + shell + "\n}").toUtf8(), QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> object(component.create()); QVERIFY2(object, qPrintable(component.errorString()));
        auto *item = object->findChild<QQuickItem *>("shellBinding"); QVERIFY(item);
        QCOMPARE(item->property("displaySettings").value<QObject *>(), &settings);
        settings.set("uiScale", 1.18);
        QCOMPARE(item->scale(), std::min({1600. / 1100, 900. / 740, 1.75}) * 1.18);
        QVERIFY(warnings.isEmpty());
    }
    void longMenuTitleFits() {
        QFile file(QString(SETTINGS_QML_DIR) + "/MenuTile.qml"); QVERIFY(file.open(QIODevice::ReadOnly));
        const QString source = QString::fromUtf8(file.readAll());
        const auto start = source.indexOf("        Text {"); QVERIFY(start >= 0);
        const auto end = source.indexOf("\n        }", start); QVERIFY(end > start);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("harmonyMedium", QVariantMap{{"name", "HarmonyOS Sans"}});
        engine.rootContext()->setContextProperty("tile", QVariantMap{{"entry", QVariantMap{{"label", "可露希尔的小游戏"}}}});
        QQmlComponent component(&engine);
        component.setData(("import QtQuick\n" + source.mid(start, end - start + 10)).toUtf8(), QUrl());
        std::unique_ptr<QObject> object(component.create()); QVERIFY2(object, qPrintable(component.errorString()));
        QQuickWindow window; window.resize(120, 110); auto *item = qobject_cast<QQuickItem *>(object.get());
        QVERIFY(item); item->setParentItem(window.contentItem()); window.show(); QTest::qWait(40);
        QCOMPARE(item->property("text").toString(), QString("可露希尔的小游戏"));
        QVERIFY(!item->property("truncated").toBool());
        QVERIFY(item->property("contentWidth").toDouble() <= item->width());
        item->setParentItem(nullptr);
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
