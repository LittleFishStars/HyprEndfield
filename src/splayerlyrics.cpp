#include "splayerlyrics.hpp"
#include "nowplayingcontroller.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

SPlayerLyrics::SPlayerLyrics(NowPlayingController *player, QObject *parent, const QUrl &endpoint)
    : QObject(parent), m_player(player), m_endpoint(endpoint)
{
    m_socket.setMaxAllowedIncomingMessageSize(8 * 1024 * 1024);
    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, &SPlayerLyrics::open);
    connect(player, &NowPlayingController::activeChanged, this, &SPlayerLyrics::reconcile);
    connect(player, &NowPlayingController::stateChanged, this, &SPlayerLyrics::reconcile);
    connect(&m_socket, &QWebSocket::connected, this, [this] { m_retry.stop(); requestSong(); });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &SPlayerLyrics::receive);
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        clear();
        if (m_player->active() && !m_trackKey.isEmpty()) m_retry.start();
    });
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this] {
        if (m_player->active() && !m_trackKey.isEmpty()) m_retry.start();
    });
}

void SPlayerLyrics::clear()
{
    if (m_lines.isEmpty()) return;
    m_lines.clear(); emit linesChanged();
}

void SPlayerLyrics::reconcile()
{
    const QString next = m_player->state().value("trackKey").toString();
    if (!m_player->active() || next.isEmpty()) {
        m_trackKey.clear(); m_retry.stop(); m_socket.abort(); clear(); return;
    }
    if (next != m_trackKey) { m_trackKey = next; clear(); requestSong(); }
    if (m_socket.state() == QAbstractSocket::UnconnectedState && !m_retry.isActive()) open();
}

void SPlayerLyrics::open()
{
    if (!m_player->active() || m_trackKey.isEmpty() || m_socket.state() != QAbstractSocket::UnconnectedState) return;
    QUrl url = m_endpoint;
    if (url.isEmpty()) {
        // Read only the explicit WebSocket settings; no account/cache access.
        QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                   + QStringLiteral("/SPlayer/config.json"));
        if (!file.open(QIODevice::ReadOnly)) { m_retry.start(); return; }
        const auto config = QJsonDocument::fromJson(file.readAll()).object().value("websocket").toObject();
        if (!config.value("enabled").toBool()) { m_retry.start(); return; }
        const int port = config.value("port").toInt(25885);
        if (port < 1 || port > 65535) return;
        url = QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port));
    }
    m_socket.open(url);
}

void SPlayerLyrics::requestSong()
{
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_socket.sendTextMessage(QStringLiteral("{\"type\":\"get-song-info\"}"));
}

void SPlayerLyrics::receive(const QString &text)
{
    if (!m_player->active() || m_trackKey.isEmpty()) return;
    const auto message = QJsonDocument::fromJson(text.toUtf8()).object();
    const auto type = message.value("type").toString();
    if (type == "song-change" || type == "lyric-change") {
        // Broadcast lyrics omit songId in SPlayer. Request an identified snapshot.
        if (type == "song-change") clear();
        requestSong(); return;
    }
    if (type != "song-info") return;
    const auto song = message.value("data").toObject();
    const auto state = m_player->state();
    const auto id = song.value("id").toVariant().toString();
    const QString trackId = state.value("trackId").toString();
    if (song.value("playName").toString() != state.value("title").toString()
        || (!id.isEmpty() && !trackId.endsWith('/' + id))) return;
    const auto lines = song.value("lyricLoading").toBool() ? QVariantList() : parseLines(song);
    if (lines != m_lines) { m_lines = lines; emit linesChanged(); }
}

QVariantList SPlayerLyrics::parseLines(const QJsonObject &song)
{
    // SPlayer's AMLL line format uses milliseconds; the source HUD uses seconds.
    auto input = song.value("lrcData").toArray();
    if (input.isEmpty()) input = song.value("yrcData").toArray();
    QVariantList result;
    for (const auto &value : input) {
        const auto line = value.toObject();
        const auto start = line.value("startTime");
        if (!start.isDouble() || !std::isfinite(start.toDouble()) || start.toDouble() < 0) continue;
        QString text;
        for (const auto &word : line.value("words").toArray()) text += word.toObject().value("word").toString();
        text = text.trimmed();
        if (text.isEmpty()) continue;
        result.append(QVariantMap{{"time", start.toDouble() / 1000.0}, {"text", text}});
    }
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.toMap().value("time").toDouble() < b.toMap().value("time").toDouble();
    });
    return result;
}
