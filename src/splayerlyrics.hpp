#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QWebSocket>

class NowPlayingController;
class QJsonObject;

// SPlayer 3.x's optional local WebSocket feed; playback remains on MPRIS.
class SPlayerLyrics final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged)
public:
    explicit SPlayerLyrics(NowPlayingController *player, QObject *parent = nullptr,
                           const QUrl &endpoint = {});
    QVariantList lines() const { return m_lines; }
    static QVariantList parseLines(const QJsonObject &song);
signals:
    void linesChanged();
private:
    void reconcile();
    void open();
    void requestSong();
    void receive(const QString &text);
    void clear();
    NowPlayingController *m_player;
    QWebSocket m_socket;
    QTimer m_retry;
    QUrl m_endpoint;
    QString m_trackKey;
    QVariantList m_lines;
};
