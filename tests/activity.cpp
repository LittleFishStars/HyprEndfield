#include "activitycollector.hpp"
#include "activitycontroller.hpp"
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJSEngine>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QtTest>
#include <cmath>

class ActivityTests final : public QObject
{
    Q_OBJECT
    static QQuickItem *findItem(QQuickItem *root, const QString &name) {
        if (root->objectName() == name) return root;
        for (auto *child : root->childItems()) if (auto *found = findItem(child, name)) return found;
        return nullptr;
    }
    static void file(const QString &path, const QByteArray &text) {
        QVERIFY(QDir().mkpath(QFileInfo(path).path()));
        QFile output(path); QVERIFY(output.open(QIODevice::WriteOnly)); QCOMPARE(output.write(text), text.size());
    }
    static QByteArray processStat(int pid, int parent, quint64 ticks, const QByteArray &name = "demo") {
        QList<QByteArray> fields(22, "0"); fields[0] = "S"; fields[1] = QByteArray::number(parent);
        fields[11] = QByteArray::number(ticks); fields[12] = "5"; fields[17] = "7"; fields[19] = "500"; fields[21] = "200";
        return QByteArray::number(pid) + " (" + name + ") " + fields.join(' ');
    }
    static Activity::Reading reading(double seconds) {
        Activity::Reading result; result.seconds = seconds; result.ticksPerSecond = 100;
        result.cpu = Activity::Counters{quint64(seconds * 1000), quint64(seconds * 600)};
        result.memoryUsed = 8'000'000'000; result.memoryTotal = 16'000'000'000; result.gpu = 24;
        result.network["wlan0"] = {quint64(seconds * 1000), quint64(seconds * 45000)};
        result.disks["nvme0n1"] = {quint64(seconds * 3000), quint64(seconds * 8000)};
        return result;
    }
    static void apps(Activity::Reading &reading, int count) {
        reading.applications = true;
        for (int i = 0; i < count; ++i) {
            const QString id = QString::number(i);
            reading.apps[id] = {id, QString("测试应用 %1").arg(i + 1), {}, {100 + i}};
            reading.processes[100 + i] = {100 + i, 1, 7 + i, 500, quint64(reading.seconds * (i + 1) * 20), quint64(i + 1) * 240'000'000, "test", "/usr/bin/test"};
        }
    }
private slots:
    void perApplicationGpu() {
        auto before = reading(1), now = reading(2); apps(before, 1); apps(now, 1);
        Activity::parseDrm("drm-driver: i915\ndrm-pdev: 0000:00:02.0\ndrm-client-id: 5\ndrm-engine-render: 100000000 ns\n", before.processes[100]);
        const QByteArray descriptor = "drm-driver: i915\ndrm-pdev: 0000:00:02.0\ndrm-client-id: 5\ndrm-engine-render: 600000000 ns\n";
        Activity::parseDrm(descriptor, now.processes[100]); Activity::parseDrm(descriptor, now.processes[100]);
        QCOMPARE(Activity::deriveApps(now, &before)[0].toMap()["gpu"].toDouble(), 50.);
        now.processes[100].gpuCapacities.begin().value() = 2;
        QCOMPARE(Activity::deriveApps(now, &before)[0].toMap()["gpu"].toDouble(), 25.);
        now.processes[100].start++;
        QVERIFY(!Activity::deriveApps(now, &before)[0].toMap()["gpu"].isValid());
        const auto usage = Activity::parseNvidiaProcesses("# gpu pid type sm mem enc dec command\n# Idx # C/G % % % % name\n0 100 G 24 0 0 0 app\n0 101 G - - - - app\n");
        QCOMPARE(usage.value(100), 24.); QVERIFY(!usage.contains(101));
        now.processes[100].gpuPercent = 24.;
        QCOMPARE(Activity::deriveApps(now, nullptr)[0].toMap()["gpu"].toDouble(), 24.);
    }
    void countersAndMissing() {
        const auto cpu = Activity::parseCpu("cpu 100 10 20 50 5 6 7 2 200 10\ncpu0 1 2 3");
        QVERIFY(cpu); QCOMPARE(cpu->first, quint64(200)); QCOMPARE(cpu->second, quint64(55));
        QVERIFY(!Activity::parseCpu("cpu malformed"));
        auto before = reading(1), now = reading(2);
        auto state = Activity::derive(now, &before);
        QCOMPARE(state["cpu"].toDouble(), 40.0); QCOMPARE(state["upload"].toDouble(), 1000.0);
        QCOMPARE(state["download"].toDouble(), 45000.0); QCOMPARE(state["read"].toDouble(), 3000.0);
        QVERIFY(!Activity::derive(now, nullptr)["cpu"].isValid());
        now.cpu->second = 1; QVERIFY(!Activity::derive(now, &before)["cpu"].isValid());
        now = reading(20); QVERIFY(!Activity::derive(now, &before)["cpu"].isValid());
        QVERIFY(!Activity::derive(now, &before)["read"].isValid());
        now = reading(2); now.network["wlan0"].first = 0;
        QVERIFY(!Activity::derive(now, &before)["upload"].isValid());
        now = reading(2); now.network["eth0"] = {1, 1};
        QVERIFY(!Activity::derive(now, &before)["download"].isValid());
    }
    void processIdentityAndSorting() {
        const auto process = Activity::parseProcess(processStat(100, 5, 30, "name ) with spaces"), 4096);
        QVERIFY(process); QCOMPARE(process->name, QString("name ) with spaces"));
        QCOMPARE(process->parent, 5); QCOMPARE(process->ticks, quint64(35)); QCOMPARE(process->memory, quint64(819200));
        QVERIFY(!Activity::parseProcess("bad", 4096));
        auto before = reading(1), now = reading(2); apps(before, 3); apps(now, 3);
        auto list = Activity::deriveApps(now, &before);
        QCOMPARE(list[2].toMap()["cpu"].toDouble(), 60.0);
        now.processes[102].ticks += 300;
        list = Activity::deriveApps(now, &before); QCOMPARE(list[2].toMap()["cpu"].toDouble(), 360.0);
        now.processes[102].start = 501;
        list = Activity::deriveApps(now, &before); QVERIFY(!list[2].toMap()["cpu"].isValid());
        QCOMPARE(Activity::sortedApps(list, "cpu", true)[0].toMap()["id"].toString(), QString("1"));
        QCOMPARE(Activity::sortedApps(list, "cpu", false)[2].toMap()["id"].toString(), QString("2"));
        QCOMPARE(Activity::sortedApps(list, "name", false)[0].toMap()["name"].toString(), QString("测试应用 1"));
        QCOMPARE(Activity::sortedApps(list, "name", true)[0].toMap()["name"].toString(), QString("测试应用 3"));
        now.apps["0"].pids.append(999); list = Activity::deriveApps(now, &before);
        QVERIFY(!list[0].toMap()["memory"].isValid());

        auto many = reading(2); apps(many, 20);
        const QLocale previousLocale;
        QLocale::setDefault(QLocale::c());
        const auto ascending = Activity::sortedApps(Activity::deriveApps(many, nullptr), "name", false);
        const auto descending = Activity::sortedApps(Activity::deriveApps(many, nullptr), "name", true);
        QLocale::setDefault(previousLocale);
        QCOMPARE(ascending[2].toMap()["name"].toString(), QString("测试应用 3"));
        QCOMPARE(descending[0].toMap()["name"].toString(), QString("测试应用 20"));
    }
    void linuxCollectorFixture() {
        QTemporaryDir directory; const QString root = directory.path();
        file(root + "/proc/stat", "cpu 10 0 10 80 0 0 0 0 0 0\n");
        file(root + "/proc/meminfo", "MemTotal: 16000 kB\nMemAvailable: 6000 kB\n");
        file(root + "/proc/net/dev", " wlan0: 100 0 0 0 0 0 0 0 200 0 0 0 0 0 0 0\n vpn0: 100 0 0 0 0 0 0 0 200 0 0 0 0 0 0 0\n lo: 90 0 0 0 0 0 0 0 90 0 0 0 0 0 0 0\n");
        QVERIFY(QDir().mkpath(root + "/sys/class/net/wlan0/device"));
        QVERIFY(QDir().mkpath(root + "/sys/block/nvme0n1/device"));
        file(root + "/sys/block/nvme0n1/stat", "1 2 8 4 5 6 16 8 9 10 11");
        file(root + "/sys/block/zram0/stat", "1 2 999 4 5 6 999 8 9 10 11");
        file(root + "/sys/class/drm/card0/device/gpu_busy_percent", "52\n");
        file(root + "/applications/demo.desktop", "[Desktop Entry]\nType=Application\nName=Demo\nExec=/test/demo\nIcon=demo-icon\n");
        file(root + "/bin/demo", ""); file(root + "/bin/helper", "");
        file(root + "/proc/100/stat", processStat(100, 1, 10));
        file(root + "/proc/101/stat", processStat(101, 100, 20));
        QVERIFY(QFile::link(root + "/bin/demo", root + "/proc/100/exe"));
        QVERIFY(QFile::link(root + "/bin/helper", root + "/proc/101/exe"));
        Activity::Collector collector({root + "/proc", root + "/sys", {root + "/applications"}, false});
        const auto value = collector.sample(true);
        QCOMPARE(value.memoryTotal.value(), quint64(16'384'000)); QCOMPARE(value.memoryUsed.value(), quint64(10'240'000));
        QCOMPARE(value.network.size(), 1); QCOMPARE(value.network["wlan0"].first, quint64(200));
        QCOMPARE(value.disks.size(), 1); QCOMPARE(value.disks["nvme0n1"].first, quint64(4096));
        QCOMPARE(value.gpu.value(), 52.0); QCOMPARE(value.apps.size(), 1);
        QCOMPARE(value.apps["demo.desktop"].pids, QList<int>({100, 101}));
        QVERIFY(collector.sample(false).processes.isEmpty());
    }
    void historyAndFormat() {
        ActivityController controller(nullptr, {}, false);
        for (int i = 0; i < 70; ++i) controller.accept(reading(i + 1));
        QCOMPARE(controller.state()["history"].toList().size(), 60);
        QFile file(QStringLiteral(ACTIVITY_QML_DIR) + "/ActivityFormat.js"); QVERIFY(file.open(QIODevice::ReadOnly));
        QString script = QString::fromUtf8(file.readAll()); script.remove(".pragma library");
        QJSEngine engine; QVERIFY(!engine.evaluate(script).isError());
        QCOMPARE(engine.evaluate("percent(null, true)").toString(), QString("—"));
        QCOMPARE(engine.evaluate("percent(360, false)").toString(), QString("360.0%"));
        QCOMPARE(engine.evaluate("bytes(8192000000)").toString(), QString("8.2 GB"));
        QCOMPARE(engine.evaluate("rate(250000)").toString(), QString("250 KB/s"));
        QCOMPARE(engine.evaluate("memory(240000000)").toString(), QString("240 MB"));
        QCOMPARE(engine.evaluate("paths([null, 50]).line").toString(), QString());
        QCOMPARE(engine.evaluate("paths([0, 100]).line.endsWith('L296 2')").toBool(), true);
        QCOMPARE(engine.evaluate("paths([1, 2, null, 3, 4]).line.split('M').length").toInt(), 3);
        QCOMPARE(engine.evaluate("paths([-1, 0]).line.endsWith('L296 44')").toBool(), true);
        QCOMPARE(engine.evaluate("series([{upload: null, download: 0}], 3)[0][0] === null").toBool(), true);
    }
    void page() {
        ActivityController controller(nullptr, {}, false);
        controller.setActive(true); controller.setApplications(true);
        for (int i = 0; i < 48; ++i) { auto value = reading(i + 1); value.gpu = 24 + std::sin(i * 0.5) * 12; apps(value, 20); controller.accept(value); }
        controller.setActive(false); controller.setApplications(false);
        QQmlEngine engine; engine.rootContext()->setContextProperty("activityController", &controller);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int font = QFontDatabase::addApplicationFont(QStringLiteral(ACTIVITY_QML_DIR) + "/../assets/fonts/HarmonyOS_Sans_" + weight + ".ttf");
            engine.rootContext()->setContextProperty(QString("harmony%1").arg(weight), QVariantMap{{"name", QFontDatabase::applicationFontFamilies(font).first()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(ACTIVITY_QML_DIR) + "/ActivityPage.qml"));
        QQuickWindow window; window.resize(960, 760); window.setColor(QColor("#222222"));
        std::unique_ptr<QObject> root(component.create()); QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(root.get()); QVERIFY(item); QCOMPARE(item->size(), QSizeF(590, 510));
        item->setParentItem(window.contentItem()); item->setPosition({50, 20}); item->setTransformOrigin(QQuickItem::TopLeft); item->setScale(1.4);
        window.show(); window.requestActivate(); root->setProperty("reduceMotion", true); root->setProperty("presented", true);
        QVERIFY(controller.active()); QVERIFY(!controller.applications());
        auto *gpu = findItem(item, "metric0"); QVERIFY(gpu); QCOMPARE(gpu->size(), QSizeF(590, 64));
        QCOMPARE(gpu->property("lines").value<QJSValue>().toVariant().toList().size(), 2);
        QTest::qWait(120);
        if (qEnvironmentVariableIsSet("HYPR_ACTIVITY_RENDER")) QVERIFY(window.grabWindow().save("/tmp/hyprendfield-activity-overview.png"));
        QTest::keyClick(&window, Qt::Key_E);
        QTRY_COMPARE(root->property("tab").toInt(), 1); QVERIFY(controller.applications());
        auto *table = root->findChild<QQuickItem *>("activityTable"); QVERIFY(table); QCOMPARE(table->position(), QPointF(0, 155)); QCOMPARE(table->size(), QSizeF(590, 330));
        QMetaObject::invokeMethod(root.get(), "sort", Q_ARG(QVariant, "name"));
        QCOMPARE(controller.state()["sortKey"].toString(), QString("name")); QVERIFY(!controller.state()["descending"].toBool());
        QMetaObject::invokeMethod(root.get(), "sort", Q_ARG(QVariant, "name")); QVERIFY(controller.state()["descending"].toBool());
        QTest::wheelEvent(&window, table->mapToScene(QPointF(300, 120)), QPoint(0, -120));
        QTRY_COMPARE(root->property("appOffset").toDouble(), 40.0);
        auto *scrollStrip = findItem(item, "activityScrollStrip"); QVERIFY(scrollStrip); QVERIFY(scrollStrip->isVisible());
        auto *scrollThumb = findItem(item, "activityScrollThumb"); QVERIFY(scrollThumb);
        const QPoint dragStart = scrollThumb->mapToScene(QPointF(scrollThumb->width() / 2, scrollThumb->height() / 2)).toPoint();
        const QPoint dragEnd = scrollStrip->mapToScene(QPointF(6, scrollStrip->height() - 8)).toPoint();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, dragStart);
        QTest::mouseMove(&window, dragEnd, 30);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, dragEnd);
        QTRY_VERIFY(root->property("appOffset").toDouble() > root->property("maxAppOffset").toDouble() * 0.8);
        QMetaObject::invokeMethod(root.get(), "scroll", Q_ARG(QVariant, -root->property("appOffset").toDouble() + 138), Q_ARG(QVariant, false));
        QCOMPARE(root->property("appOffset").toDouble(), 138.0);
        QTest::qWait(120);
        if (qEnvironmentVariableIsSet("HYPR_ACTIVITY_RENDER")) QVERIFY(window.grabWindow().save("/tmp/hyprendfield-activity-apps.png"));
        root->setProperty("reduceMotion", false);
        QTest::keyClick(&window, Qt::Key_Q); QCOMPARE(root->property("motionPhase").toString(), QString("leave"));
        QTRY_COMPARE(root->property("tab").toInt(), 0); QTRY_COMPARE(root->property("motionPhase").toString(), QString("idle"));
        QVERIFY(!controller.applications());
        root->setProperty("presented", false); QVERIFY(!controller.active()); QVERIFY(!controller.applications());
        if (!warnings.isEmpty()) for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty()); item->setParentItem(nullptr);
    }
};

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsSet("HYPR_ACTIVITY_RENDER")) {
        qputenv("QT_QUICK_BACKEND", "rhi"); QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        QSurfaceFormat format; format.setSamples(4); QSurfaceFormat::setDefaultFormat(format);
    }
    QGuiApplication app(argc, argv);
    if (app.arguments().contains("--live")) {
        Activity::Collector collector;
        const auto first = collector.sample(true); QTest::qWait(1100); const auto second = collector.sample(true);
        qInfo() << "System:" << Activity::derive(second, &first);
        const auto apps = Activity::deriveApps(second, &first);
        qInfo() << "Apps:" << apps.size();
        for (const auto &entry : apps) qInfo() << entry.toMap();
        return second.cpu && second.memoryTotal && !second.network.isEmpty() && !second.disks.isEmpty() ? 0 : 1;
    }
    ActivityTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "activity.moc"
