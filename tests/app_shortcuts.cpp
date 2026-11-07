#include "appshortcutscontroller.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
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
class ShortcutTests : public QObject {
    Q_OBJECT
    static bool write(const QString &path, const QByteArray &bytes) { QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
    static QString application(const QString &directory, const QString &name = "test") {
        const QString path = directory + '/' + name + ".desktop";
        if (!write(path, "[Desktop Entry]\nType=Application\nName=Test Application\nExec=/usr/bin/true\nIcon=folder\nTerminal=false\n")) return {};
        return path;
    }
    static QString firstId(const AppShortcutsController &controller) { return controller.state()["items"].toList().value(0).toMap()["id"].toString(); }
private slots:
    void editingAndPersistence() {
        QTemporaryDir dir; const auto file = application(dir.path()); QVERIFY(!file.isEmpty()); QString id;
        {
            AppShortcutsController controller(nullptr, dir.path() + "/data");
            QVERIFY(controller.add(QUrl::fromLocalFile(file))); id = firstId(controller); QVERIFY(!id.isEmpty());
            QVERIFY(!controller.add(QUrl::fromLocalFile(file))); QCOMPARE(controller.state()["count"].toInt(), 1);
            QVERIFY(controller.rename(id, "  Dolphin  ")); QCOMPARE(controller.state()["items"].toList().first().toMap()["name"].toString(), "Dolphin");
            QVERIFY(!controller.rename(id, "\n\t"));
            QVERIFY(controller.setIcon(id, "terminal")); QVERIFY(!controller.setIcon(id, "../invalid"));
        }
        AppShortcutsController restored(nullptr, dir.path() + "/data");
        QCOMPARE(firstId(restored), id); QCOMPARE(restored.state()["items"].toList().first().toMap()["iconPreset"].toString(), "terminal");
        QVERIFY(restored.remove(id)); QVERIFY(QFile::exists(file)); QCOMPARE(restored.state()["count"].toInt(), 0);
    }
    void deferredLaunchAndChangedTarget() {
        QTemporaryDir dir; const auto file = application(dir.path()); int launched = 0;
        AppShortcutsController controller(nullptr, dir.path() + "/data", [&](const QString &path) { if (path != file) return QString("Unexpected application"); ++launched; return QString(); });
        QVERIFY(controller.add(QUrl::fromLocalFile(file))); const auto id = firstId(controller);
        QSignalSpy requested(&controller, &AppShortcutsController::launchRequested), opened(&controller, &AppShortcutsController::opened), failed(&controller, &AppShortcutsController::launchFailed);
        QVERIFY(controller.requestOpen(id)); QCOMPARE(requested.size(), 1); QCOMPARE(launched, 0);
        controller.launchPending(); QCOMPARE(launched, 1); QCOMPARE(opened.size(), 1);
        QVERIFY(write(file, "[Desktop Entry]\nType=Application\nName=Changed\nExec=/usr/bin/false\n"));
        QVERIFY(controller.requestOpen(id)); controller.launchPending(); QCOMPARE(launched, 1); QCOMPARE(failed.size(), 1);
        QVERIFY(!controller.add(QUrl("https://example.com/app.desktop")));
    }
    void preserveFilesAndMissingTarget() {
        QTemporaryDir dir; const auto file = application(dir.path());
        AppShortcutsController controller(nullptr, dir.path() + "/data"); QVERIFY(controller.add(QUrl::fromLocalFile(file)));
        const QString id = firstId(controller); QVERIFY(QFile::remove(file)); controller.refresh();
        QVERIFY(!controller.state()["items"].toList().first().toMap()["available"].toBool()); QVERIFY(controller.rename(id, "Missing app")); QVERIFY(controller.remove(id));
        const auto path = dir.path() + "/data/shortcuts.json"; QVERIFY(write(path, "broken"));
        AppShortcutsController broken(nullptr, dir.path() + "/data"); QVERIFY(!broken.state()["available"].toBool());
        const auto other = application(dir.path(), "other"); QVERIFY(!broken.add(QUrl::fromLocalFile(other)));
        QFile saved(path); QVERIFY(saved.open(QIODevice::ReadOnly)); QCOMPARE(saved.readAll(), QByteArray("broken"));
    }
    void artworkAndPage() {
        QTemporaryDir dir; const auto file = application(dir.path());
        AppShortcutsController controller(nullptr, dir.path() + "/data"); QVERIFY(controller.add(QUrl::fromLocalFile(file))); const auto id = firstId(controller);
        QCOMPARE(controller.presets().size(), 14);
        for (const auto &preset : controller.presets()) {
            const auto image = controller.artwork(id, preset.toMap()["id"].toString(), QColor("#FFFA00"), {120, 120}); QVERIFY(!image.isNull());
        }
        QQmlEngine engine; engine.rootContext()->setContextProperty("appShortcutsController", &controller); engine.addImageProvider("shortcut-icons", new ShortcutIconProvider(&controller));
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int index = QFontDatabase::addApplicationFont(QString(SHORTCUT_SOURCE_DIR) + "/assets/fonts/HarmonyOS_Sans_" + weight + ".ttf"); QVERIFY(index >= 0);
            engine.rootContext()->setContextProperty(QString("harmony") + weight, QVariantMap{{"name", QFontDatabase::applicationFontFamilies(index).first()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QString(SHORTCUT_SOURCE_DIR) + "/qml/AppShortcutsPage.qml"));
        std::unique_ptr<QObject> page(component.create()); QVERIFY2(page, qPrintable(component.errorString()));
        QQuickWindow window; window.setColor(QColor("#181818")); window.resize(690, 600); auto *item = qobject_cast<QQuickItem *>(page.get()); item->setParentItem(window.contentItem()); item->setPosition({50, 40});
        window.show(); page->setProperty("presented", true); QTest::qWait(80);
        page->setProperty("menuId", id); page->setProperty("menuY", 134); QTest::qWait(30);
        if (qEnvironmentVariableIsSet("HYPR_SHORTCUT_RENDER")) QVERIFY(window.grabWindow().save(QString(SHORTCUT_PREVIEW_DIR) + "/app-shortcuts-page.png"));
        QMetaObject::invokeMethod(page.get(), "chooseIcon", Q_ARG(QVariant, 3)); QCOMPARE(controller.state()["items"].toList().first().toMap()["iconPreset"].toString(), "terminal");
        for (const auto &warning : warnings) qWarning() << warning; QVERIFY(warnings.isEmpty()); item->setParentItem(nullptr);
    }
};
int main(int argc, char **argv) { QApplication app(argc, argv); ShortcutTests tests; return QTest::qExec(&tests, argc, argv); }
#include "app_shortcuts.moc"
