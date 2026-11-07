#include "audiocontroller.hpp"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

// Separate runtime/socket and WirePlumber's policy-only profile (no hardware
// monitors): tests cannot enumerate physical devices or change real settings.
class AudioTest final : public QObject
{
    Q_OBJECT
    QTemporaryDir runtime;
    QProcessEnvironment environment;
    QProcess pipewire, pulse, policy, playback;
    std::unique_ptr<AudioController> audio;
    QString endpoint;

    QByteArray pactl(const QStringList &arguments, bool *success = nullptr) {
        QProcess process;
        process.setProcessEnvironment(environment);
        process.start("pactl", arguments);
        const bool ok = process.waitForFinished(5000) && process.exitCode() == 0;
        if (success) *success = ok;
        if (!ok) qWarning().noquote() << process.readAllStandardError();
        return process.readAllStandardOutput().trimmed();
    }
    bool load(const QStringList &arguments) { bool ok; pactl(QStringList{"load-module"} + arguments, &ok); return ok; }
    QVariantMap state() const { return audio->state(); }
    QString outputKey() const { return state()["outputKey"].toString(); }
    static void stop(QProcess &process) {
        if (process.state() == QProcess::NotRunning) return;
        process.terminate();
        if (!process.waitForFinished(3000)) { process.kill(); process.waitForFinished(3000); }
    }
private slots:
    void initTestCase() {
        QVERIFY(runtime.isValid());
        endpoint = "unix:" + runtime.path() + "/pulse/native";
        environment = QProcessEnvironment::systemEnvironment();
        environment.insert("XDG_RUNTIME_DIR", runtime.path());
        environment.insert("PIPEWIRE_RUNTIME_DIR", runtime.path());
        environment.insert("PULSE_RUNTIME_PATH", runtime.path() + "/pulse");
        environment.insert("PULSE_SERVER", endpoint);
        environment.insert("PIPEWIRE_REMOTE", "pipewire-0");
        environment.insert("XDG_STATE_HOME", runtime.path() + "/state");
        environment.insert("XDG_CONFIG_HOME", runtime.path() + "/config");
        environment.insert("XDG_CACHE_HOME", runtime.path() + "/cache");
        pipewire.setProcessEnvironment(environment);
        pulse.setProcessEnvironment(environment);
        playback.setProcessEnvironment(environment);
        policy.setProcessEnvironment(environment);
        pipewire.start("pipewire", QStringList{});
        QVERIFY(pipewire.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(runtime.path() + "/pipewire-0"), 5000);
        pulse.start("pipewire-pulse", QStringList{});
        QVERIFY(pulse.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(runtime.path() + "/pulse/native"), 5000);
        QVERIFY(load({"module-null-sink", "sink_name=hud_a", "sink_properties=device.description=HUD_A", "channels=2", "channel_map=front-left,front-right"}));
        QVERIFY(load({"module-null-sink", "sink_name=hud_b", "sink_properties=device.description=HUD_B", "channels=2", "channel_map=front-left,front-right"}));
        QVERIFY(load({"module-remap-source", "source_name=hud_input", "master=hud_a.monitor"}));
        policy.start("wireplumber", {"--profile=policy"});
        QVERIFY(policy.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(pactl({"get-default-sink"}).startsWith("hud_"), 5000);
        pactl({"set-default-sink", "hud_a"});
        pactl({"set-default-source", "hud_input"});
        audio = std::make_unique<AudioController>(nullptr, endpoint);
        QVERIFY(!audio->active());
        QVERIFY(!audio->setVolume(50, ""));
        audio->setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(state()["ready"].toBool(), 5000);
    }
    void devicesAndLevels() {
        QCOMPARE(state()["outputs"].toList().size(), 2);
        QCOMPARE(state()["inputs"].toList().size(), 1); // Excludes sink monitors.
        QCOMPARE(state()["outputName"].toString(), "HUD_A");
        QVERIFY(state()["canBalance"].toBool());
        const QString key = outputKey();
        QVERIFY(audio->setVolume(40, key));
        QTRY_VERIFY(std::abs(state()["volume"].toDouble() - 40) < 0.1);
        QVERIFY(audio->setBalance(50, key));
        QTRY_VERIFY(std::abs(state()["balance"].toDouble() - 50) < 0.1);
        QVERIFY(audio->setVolume(60, key));
        QTRY_VERIFY(std::abs(state()["volume"].toDouble() - 60) < 0.1);
        QVERIFY(std::abs(state()["balance"].toDouble() - 50) < 0.1);
        QVERIFY(audio->setMuted(true, key));
        QTRY_VERIFY(state()["muted"].toBool());
        QVERIFY(audio->setMuted(false, key));
        QTRY_VERIFY(!state()["muted"].toBool());
        QVERIFY(!audio->setVolume(std::numeric_limits<double>::quiet_NaN(), key));
        QVERIFY(audio->setVolume(150, key));
        QTRY_VERIFY(std::abs(state()["volume"].toDouble() - 100) < 0.1);
        pactl({"set-sink-volume", "hud_a", "30%"});
        QTRY_VERIFY(std::abs(state()["volume"].toDouble() - 30) < 0.1);
    }
    void selectionAndStaleGestures() {
        const QString oldKey = outputKey();
        QString other;
        for (const auto &item : state()["outputs"].toList())
            if (item.toMap()["name"].toString() == "hud_b") other = item.toMap()["key"].toString();
        QVERIFY(audio->selectDevice(other, false));
        QTRY_COMPARE(outputKey(), other);
        QVERIFY(!audio->setVolume(5, oldKey));
        QVERIFY(!audio->setBalance(50, oldKey));
        QVERIFY(!audio->setMuted(true, oldKey));
        QVERIFY(!audio->selectDevice("missing", false));
        QVERIFY(audio->selectDevice(state()["inputKey"].toString(), true));
        QTRY_COMPARE(pactl({"get-default-source"}), QByteArray("hud_input"));
    }
    void applicationVolume() {
        playback.start("paplay", {"--raw", "--rate=48000", "--channels=2", "--device=hud_b",
                                  "--property=application.name=HUD Fixture", "--property=application.icon_name=", "/dev/zero"});
        QVERIFY(playback.waitForStarted());
        QTRY_COMPARE_WITH_TIMEOUT(state()["applications"].toList().size(), 1, 5000);
        const auto app = state()["applications"].toList().first().toMap();
        QCOMPARE(app["label"].toString(), "HUD Fixture");
        QVERIFY(audio->setApplicationVolume(23, app["key"].toString()));
        QTRY_VERIFY(std::abs(state()["applications"].toList().first().toMap()["volume"].toDouble() - 23) < 0.1);
        QVERIFY(!audio->setApplicationVolume(50, "missing"));
        stop(playback);
        QTRY_VERIFY(state()["applications"].toList().isEmpty());
        QVERIFY(!audio->setApplicationVolume(50, app["key"].toString()));
    }
    void reconnectAndInactive() {
        const QString previous = outputKey();
        audio->setActive(false);
        QVERIFY(!state()["ready"].toBool());
        QVERIFY(!audio->setVolume(80, previous));
        audio->setActive(true);
        QTRY_VERIFY(state()["ready"].toBool());
        QVERIFY(outputKey() != previous);
        QVERIFY(!audio->setVolume(80, previous));
        stop(pulse);
        QTRY_VERIFY(!state()["ready"].toBool());
        pulse.start("pipewire-pulse", QStringList{});
        QVERIFY(pulse.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(state()["ready"].toBool(), 8000);
    }
    void cleanupTestCase() {
        audio.reset(); stop(playback); stop(policy); stop(pulse); stop(pipewire);
    }
};
QTEST_GUILESS_MAIN(AudioTest)
#include "audio.moc"
