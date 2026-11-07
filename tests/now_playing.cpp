#include "nowplayingcontroller.hpp"
#include "splayerlyrics.hpp"
#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocketServer>
#include <cmath>

class MockPlayer final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus MEMBER status)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(qlonglong Position MEMBER position)
    Q_PROPERTY(double Rate MEMBER rate)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(bool CanControl MEMBER canControl)
    Q_PROPERTY(bool CanPlay MEMBER canPlay)
    Q_PROPERTY(bool CanPause MEMBER canPause)
    Q_PROPERTY(bool CanGoNext MEMBER canNext)
    Q_PROPERTY(bool CanGoPrevious MEMBER canPrevious)
    Q_PROPERTY(bool CanSeek MEMBER canSeek)
public:
    explicit MockPlayer(QDBusConnection bus) : bus(bus) {}
    QString status = "Paused", id = "1", title = "Test track";
    qlonglong position = 12'000'000, duration = 180'000'000;
    double rate = 1, gain = .7;
    bool canControl = true, canPlay = true, canPause = true, canNext = true, canPrevious = true, canSeek = true;
    int nextCalls = 0, previousCalls = 0, toggleCalls = 0, seekCalls = 0, volumeCalls = 0;
    QDBusConnection bus;
    QVariantMap metadata() const {
        return {{"mpris:trackid", QVariant::fromValue(QDBusObjectPath("/com/splayer/track/" + id))},
                {"mpris:length", duration}, {"xesam:title", title},
                {"xesam:artist", QStringList{"Test artist"}}, {"mpris:artUrl", "file:///tmp/test-cover.png"}};
    }
    double volume() const { return gain; }
    void setVolume(double value) { ++volumeCalls; gain = value; changed({{"Volume", gain}}); }
    void changed(const QVariantMap &values) {
        auto signal = QDBusMessage::createSignal("/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
        signal << QString("org.mpris.MediaPlayer2.Player") << values << QStringList();
        bus.send(signal);
    }
public slots:
    void Next() { ++nextCalls; }
    void Previous() { ++previousCalls; }
    void PlayPause() { ++toggleCalls; status = status == "Playing" ? "Paused" : "Playing"; changed({{"PlaybackStatus", status}}); }
    void SetPosition(const QDBusObjectPath &track, qlonglong value) {
        if (track.path() != "/com/splayer/track/" + id) return;
        ++seekCalls; position = value; emit Seeked(position);
    }
signals:
    void Seeked(qlonglong Position);
};

class PlayerTests final : public QObject
{
    Q_OBJECT
private slots:
    void integration()
    {
        auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "mock-player");
        QVERIFY(bus.isConnected());
        MockPlayer mock(bus);
        QVERIFY(bus.registerObject("/org/mpris/MediaPlayer2", &mock, QDBusConnection::ExportAllProperties
                                   | QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals));
        QVERIFY(bus.registerService("org.mpris.MediaPlayer2.other"));
        NowPlayingController player;
        QSignalSpy failures(&player, &NowPlayingController::commandFailed);
        player.setActive(true);
        QTest::qWait(100);
        QVERIFY(!player.state().value("hasTrack").toBool());
        player.next(); player.playPause();
        QTest::qWait(20);
        QCOMPARE(mock.nextCalls + mock.toggleCalls, 0);

        const QString service = "org.mpris.MediaPlayer2.splayer.instance123";
        QVERIFY(bus.registerService(service));
        QTRY_VERIFY(player.state().value("hasTrack").toBool());
        QCOMPARE(player.state().value("artist").toString(), QString("Test artist"));
        QCOMPARE(player.state().value("duration").toDouble(), 180.0);
        QCOMPARE(player.position(), 12.0);
        const QString key = player.state().value("trackKey").toString();
        player.seek(37.25, key);
        QTRY_COMPARE(mock.seekCalls, 1);
        QTRY_COMPARE(player.position(), 37.25);
        player.seek(-100, key);
        QTRY_COMPARE(mock.position, qlonglong(0));
        player.seek(999, key);
        QTRY_COMPARE(mock.position, mock.duration);
        player.seek(NAN, key);
        player.setVolume(NAN, key);
        QTest::qWait(20);
        QCOMPARE(mock.seekCalls, 3);
        player.setVolume(.42, key);
        QTRY_COMPARE(mock.volumeCalls, 1);
        QTRY_COMPARE(player.state().value("volume").toDouble(), .42);
        player.setVolume(5, key);
        QTRY_COMPARE(mock.gain, 1.0);

        player.previous(); // Source rule: restart after three seconds, otherwise Previous.
        QTRY_COMPARE(mock.seekCalls, 4);
        QTRY_COMPARE(player.position(), 0.0);
        player.previous();
        QTRY_COMPARE(mock.previousCalls, 1);
        player.next();
        QTRY_COMPARE(mock.nextCalls, 1);
        player.playPause();
        QTRY_VERIFY(player.state().value("playing").toBool());
        QTest::qWait(120);
        QVERIFY(player.position() > .05);
        player.playPause();
        QTRY_VERIFY(!player.state().value("playing").toBool());
        const double frozen = player.position();
        QTest::qWait(100);
        QCOMPARE(player.position(), frozen);

        mock.canControl = false;
        mock.changed({{"CanControl", false}});
        QTRY_VERIFY(!player.state().value("canToggle").toBool());
        player.next(); player.previous(); player.playPause(); player.seek(60, key); player.setVolume(.1, key);
        QTest::qWait(30);
        QCOMPARE(mock.toggleCalls, 2); QCOMPARE(mock.nextCalls, 1); QCOMPARE(mock.previousCalls, 1);
        QCOMPARE(mock.seekCalls, 4); QCOMPARE(mock.volumeCalls, 2);
        mock.canControl = true; mock.canSeek = false;
        mock.changed({{"CanControl", true}, {"CanSeek", false}});
        QTRY_VERIFY(!player.state().value("canSeek").toBool());
        player.seek(20, key);
        QTest::qWait(20); QCOMPARE(mock.seekCalls, 4);

        mock.id = "2"; mock.title = "Second track"; mock.canSeek = true;
        mock.changed({{"Metadata", mock.metadata()}, {"Position", qlonglong(0)}, {"CanSeek", true}});
        QTRY_COMPARE(player.state().value("title").toString(), mock.title);
        player.seek(60, key); player.setVolume(.9, key);
        QTest::qWait(30); QCOMPARE(mock.seekCalls, 4); QCOMPARE(mock.volumeCalls, 2);

        // Same identified song snapshots as SPlayer, using an isolated local server.
        QWebSocketServer server("test", QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        SPlayerLyrics lyrics(&player, nullptr, QUrl(QString("ws://127.0.0.1:%1").arg(server.serverPort())));
        QWebSocket *client = nullptr;
        QJsonObject song{{"id", 2}, {"playName", mock.title}, {"lrcData", QJsonArray{
            QJsonObject{{"startTime", 1250}, {"words", QJsonArray{QJsonObject{{"word", "Test "}}, QJsonObject{{"word", "line"}}}}},
            QJsonObject{{"startTime", 2750}, {"words", QJsonArray{QJsonObject{{"word", "Second line"}}}}}}}};
        const auto sendSong = [&] {
            client->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"type", "song-info"}, {"data", song}}).toJson(QJsonDocument::Compact)));
        };
        connect(&server, &QWebSocketServer::newConnection, this, [&] {
            client = server.nextPendingConnection();
            connect(client, &QWebSocket::textMessageReceived, this, [&](const QString &message) {
                QCOMPARE(QJsonDocument::fromJson(message.toUtf8()).object().value("type").toString(), QString("get-song-info"));
                sendSong();
            });
        });
        mock.changed({{"Rate", 1.0}}); // Activates the just-created lyric observer.
        QTRY_COMPARE(lyrics.lines().size(), 2);
        QCOMPARE(lyrics.lines().first().toMap().value("time").toDouble(), 1.25);
        QCOMPARE(lyrics.lines().first().toMap().value("text").toString(), QString("Test line"));
        song["id"] = 1; song["lrcData"] = QJsonArray(); sendSong();
        QTest::qWait(40); QCOMPARE(lyrics.lines().size(), 2); // Old-song response ignored.
        song["id"] = 2; sendSong();
        QTRY_VERIFY(lyrics.lines().isEmpty()); // Instrumental/no-lyric song clears text.

        player.setActive(false);
        QTRY_VERIFY(!player.state().value("hasTrack").toBool());
        QVERIFY(lyrics.lines().isEmpty());
        player.next(); player.seek(5, key);
        QTest::qWait(30); QCOMPARE(mock.nextCalls, 1); QCOMPARE(mock.seekCalls, 4);
        player.setActive(true);
        QTRY_VERIFY(player.state().value("hasTrack").toBool());
        QVERIFY(bus.unregisterService(service));
        QTRY_VERIFY(!player.state().value("hasTrack").toBool());
        QVERIFY(bus.registerService(service));
        QTRY_VERIFY(player.state().value("hasTrack").toBool());
        QCOMPARE(failures.count(), 0);
        player.setActive(false);
        bus.unregisterService(service);
        bus.unregisterService("org.mpris.MediaPlayer2.other");
    }

    void lyricsParsing()
    {
        const auto lines = SPlayerLyrics::parseLines(QJsonDocument::fromJson(R"({"yrcData":[
            {"startTime":3000,"words":[{"word":"later"}]},
            {"startTime":1000,"words":[{"word":"earlier"}]},
            {"startTime":-1,"words":[{"word":"invalid"}]},
            {"words":[{"word":"missing time"}]}]})").object());
        QCOMPARE(lines.size(), 2);
        QCOMPARE(lines.first().toMap().value("text").toString(), QString("earlier"));
        QVERIFY(SPlayerLyrics::parseLines(QJsonObject()).isEmpty());
    }
};

QTEST_GUILESS_MAIN(PlayerTests)
#include "now_playing.moc"
