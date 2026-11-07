#pragma once

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

// The SPlayer MPRIS adapter. Never sends transport commands to other players.
class NowPlayingController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
public:
    explicit NowPlayingController(QObject *parent = nullptr,
                                  QDBusConnection bus = QDBusConnection::sessionBus());
    bool active() const { return m_active; }
    void setActive(bool value);
    QVariantMap state() const;
    double position() const;
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void next();
    Q_INVOKABLE void seek(double seconds, const QString &trackKey);
    Q_INVOKABLE void setVolume(double volume, const QString &trackKey);
signals:
    void activeChanged();
    void stateChanged();
    void positionChanged();
    void commandFailed(const QString &message);
private slots:
    void ownerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void propertiesChanged(const QString &interface, const QVariantMap &changed,
                           const QStringList &invalidated);
    void seeked(qlonglong microseconds);
private:
    void discover();
    void attach(const QString &name, const QString &owner);
    void detach();
    void refresh();
    void apply(const QVariantMap &properties);
    void call(const QString &method, const QList<QVariant> &args = {},
              const QString &interface = QStringLiteral("org.mpris.MediaPlayer2.Player"));
    void anchor(double seconds);
    bool permitted(const char *capability) const;
    QString trackKey() const;
    QDBusConnection m_bus;
    bool m_active = false;
    QString m_service, m_owner;
    QVariantMap m_properties, m_metadata;
    QElapsedTimer m_clock;
    QTimer m_tick;
    double m_position = -1;
    quint64 m_generation = 0, m_revision = 0;
};
