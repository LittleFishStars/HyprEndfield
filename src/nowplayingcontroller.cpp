#include "nowplayingcontroller.hpp"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QUrl>
#include <algorithm>
#include <cmath>

namespace {
const QString path = QStringLiteral("/org/mpris/MediaPlayer2");
const QString player = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString propertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
bool isSPlayer(const QString &name)
{
    const QString prefix = QStringLiteral("org.mpris.MediaPlayer2.splayer");
    return name.compare(prefix, Qt::CaseInsensitive) == 0
        || name.startsWith(prefix + '.', Qt::CaseInsensitive);
}
QVariantMap asMap(const QVariant &value)
{
    return value.metaType() == QMetaType::fromType<QDBusArgument>()
        ? qdbus_cast<QVariantMap>(value.value<QDBusArgument>()) : value.toMap();
}
QString objectPath(const QVariant &value)
{
    return value.metaType() == QMetaType::fromType<QDBusObjectPath>()
        ? value.value<QDBusObjectPath>().path() : value.toString();
}
}

NowPlayingController::NowPlayingController(QObject *parent, QDBusConnection bus)
    : QObject(parent), m_bus(std::move(bus))
{
    m_clock.start();
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &NowPlayingController::positionChanged);
}

void NowPlayingController::setActive(bool value)
{
    if (m_active == value) return;
    m_active = value;
    if (value) {
        m_bus.connect(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
                      this, SLOT(ownerChanged(QString,QString,QString)));
        discover();
    } else {
        m_bus.disconnect(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                         QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameOwnerChanged"),
                         this, SLOT(ownerChanged(QString,QString,QString)));
        detach();
    }
    emit activeChanged();
}

void NowPlayingController::discover()
{
    if (!m_active || !m_bus.interface()) return;
    const auto generation = m_generation;
    auto *pending = new QDBusPendingCallWatcher(m_bus.interface()->asyncCall(QStringLiteral("ListNames")), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *watcher) {
        const QDBusPendingReply<QStringList> reply = *watcher;
        watcher->deleteLater();
        if (!m_active || generation != m_generation || !m_owner.isEmpty() || reply.isError()) return;
        QStringList names = reply.value();
        names.sort();
        for (const auto &name : names) {
            if (!isSPlayer(name)) continue;
            const QDBusReply<QString> owner = m_bus.interface()->serviceOwner(name);
            if (owner.isValid() && !owner.value().isEmpty()) {
                attach(name, owner.value());
                break;
            }
        }
    });
}

void NowPlayingController::ownerChanged(const QString &name, const QString &, const QString &owner)
{
    if (!m_active || !isSPlayer(name)) return;
    if (name == m_service) {
        detach();
        if (!owner.isEmpty()) attach(name, owner);
        else discover();
    } else if (m_owner.isEmpty() && !owner.isEmpty()) attach(name, owner);
}

void NowPlayingController::attach(const QString &name, const QString &owner)
{
    m_service = name;
    m_owner = owner;
    ++m_generation;
    m_bus.connect(owner, path, propertiesInterface, QStringLiteral("PropertiesChanged"),
                  this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
    m_bus.connect(owner, path, player, QStringLiteral("Seeked"), this, SLOT(seeked(qlonglong)));
    refresh();
}

void NowPlayingController::detach()
{
    if (!m_owner.isEmpty()) {
        m_bus.disconnect(m_owner, path, propertiesInterface, QStringLiteral("PropertiesChanged"),
                         this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
        m_bus.disconnect(m_owner, path, player, QStringLiteral("Seeked"), this, SLOT(seeked(qlonglong)));
    }
    ++m_generation;
    m_service.clear(); m_owner.clear(); m_properties.clear(); m_metadata.clear();
    m_tick.stop(); anchor(-1);
    emit stateChanged();
    emit positionChanged();
}

void NowPlayingController::refresh()
{
    if (!m_active || m_owner.isEmpty()) return;
    const auto generation = m_generation, revision = m_revision;
    auto message = QDBusMessage::createMethodCall(m_owner, path, propertiesInterface, QStringLiteral("GetAll"));
    message << player;
    auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 2000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, generation, revision](auto *watcher) {
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (!m_active || generation != m_generation) return;
        // An older snapshot must not overwrite a track/seek signal received meanwhile.
        if (revision != m_revision) { refresh(); return; }
        if (reply.isError()) { emit commandFailed(reply.error().message()); return; }
        apply(reply.value());
    });
}

void NowPlayingController::propertiesChanged(const QString &interface, const QVariantMap &changed,
                                           const QStringList &invalidated)
{
    if (!m_active || interface != player) return;
    ++m_revision;
    apply(changed);
    for (const auto &key : invalidated) m_properties.remove(key);
    if (!invalidated.isEmpty()) refresh();
    // MPRIS does not emit normal Position changes. Resample only on a state/track change.
    if ((changed.contains("PlaybackStatus") || changed.contains("Metadata")) && !changed.contains("Position")) refresh();
}

void NowPlayingController::apply(const QVariantMap &properties)
{
    const double elapsed = position();
    const QString previousTrack = trackKey();
    for (auto it = properties.begin(); it != properties.end(); ++it) m_properties.insert(it.key(), it.value());
    m_metadata = asMap(m_properties.value("Metadata"));
    const bool changedTrack = previousTrack != trackKey();
    anchor(properties.contains("Position") ? properties.value("Position").toLongLong() / 1e6
                                           : changedTrack ? -1 : elapsed);
    if (m_active && m_properties.value("PlaybackStatus").toString() == "Playing" && m_position >= 0) m_tick.start();
    else m_tick.stop();
    emit stateChanged();
    emit positionChanged();
}

void NowPlayingController::anchor(double seconds) { m_position = seconds; m_clock.restart(); }

double NowPlayingController::position() const
{
    if (m_position < 0) return -1;
    double seconds = m_position;
    if (m_active && m_properties.value("PlaybackStatus").toString() == "Playing")
        seconds += m_clock.elapsed() / 1000.0 * m_properties.value("Rate", 1.0).toDouble();
    const double duration = m_metadata.value("mpris:length").toLongLong() / 1e6;
    return duration > 0 ? std::clamp(seconds, 0.0, duration) : std::max(0.0, seconds);
}

QString NowPlayingController::trackKey() const
{
    const auto id = objectPath(m_metadata.value("mpris:trackid"));
    return m_owner.isEmpty() || id.isEmpty() || id == "/org/mpris/MediaPlayer2/TrackList/NoTrack"
        ? QString() : m_owner + '|' + id + '|' + m_metadata.value("xesam:title").toString();
}

bool NowPlayingController::permitted(const char *capability) const
{
    return m_active && !trackKey().isEmpty() && m_properties.value("CanControl").toBool()
        && m_properties.value(QLatin1String(capability)).toBool();
}

QVariantMap NowPlayingController::state() const
{
    const bool playing = m_properties.value("PlaybackStatus").toString() == "Playing";
    const QUrl art(m_metadata.value("mpris:artUrl").toString());
    const bool validArt = art.isLocalFile() || art.scheme() == "http" || art.scheme() == "https";
    const double duration = std::max(0.0, m_metadata.value("mpris:length").toLongLong() / 1e6);
    return {{"connected", !m_owner.isEmpty()}, {"hasTrack", !trackKey().isEmpty()},
            {"trackKey", trackKey()}, {"trackId", objectPath(m_metadata.value("mpris:trackid"))},
            {"title", m_metadata.value("xesam:title").toString()},
            {"artist", m_metadata.value("xesam:artist").toStringList().join(QStringLiteral(" / "))},
            {"artUrl", validArt ? art.toString() : QString()}, {"duration", duration},
            {"playing", playing}, {"rate", m_properties.value("Rate", 1.0)},
            {"volume", std::clamp(m_properties.value("Volume", 0.0).toDouble(), 0.0, 1.0)},
            {"canVolume", m_active && !trackKey().isEmpty() && m_properties.value("CanControl").toBool() && m_properties.contains("Volume")},
            {"canToggle", permitted(playing ? "CanPause" : "CanPlay")},
            {"canPrevious", permitted("CanGoPrevious") || (permitted("CanSeek") && position() > 3)},
            {"canNext", permitted("CanGoNext")},
            {"canSeek", permitted("CanSeek") && duration > 0 && position() >= 0}};
}

void NowPlayingController::call(const QString &method, const QList<QVariant> &args, const QString &interface)
{
    if (!m_active || m_owner.isEmpty()) return;
    const auto generation = m_generation;
    auto message = QDBusMessage::createMethodCall(m_owner, path, interface, method);
    message.setArguments(args);
    auto *pending = new QDBusPendingCallWatcher(m_bus.asyncCall(message, 2000), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, generation](auto *watcher) {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (!m_active || generation != m_generation) return;
        if (reply.isError()) { emit commandFailed(reply.error().message()); return; }
        refresh();
    });
}

void NowPlayingController::playPause() { if (state().value("canToggle").toBool()) call(QStringLiteral("PlayPause")); }
void NowPlayingController::previous()
{
    if (permitted("CanSeek") && position() > 3) seek(0, trackKey());
    else if (permitted("CanGoPrevious")) call(QStringLiteral("Previous"));
}
void NowPlayingController::next() { if (permitted("CanGoNext")) call(QStringLiteral("Next")); }
void NowPlayingController::seek(double seconds, const QString &key)
{
    if (key != trackKey() || !state().value("canSeek").toBool() || !std::isfinite(seconds)) return;
    const auto micros = qlonglong(std::clamp(seconds, 0.0, m_metadata.value("mpris:length").toLongLong() / 1e6) * 1e6);
    call(QStringLiteral("SetPosition"), {QVariant::fromValue(QDBusObjectPath(objectPath(m_metadata.value("mpris:trackid")))), micros});
}
void NowPlayingController::setVolume(double volume, const QString &key)
{
    if (key != trackKey() || !state().value("canVolume").toBool() || !std::isfinite(volume)) return;
    call(QStringLiteral("Set"), {player, QStringLiteral("Volume"), QVariant::fromValue(QDBusVariant(std::clamp(volume, 0.0, 1.0)))}, propertiesInterface);
}
void NowPlayingController::seeked(qlonglong microseconds)
{
    if (!m_active) return;
    ++m_revision; anchor(std::max(0.0, microseconds / 1e6));
    emit positionChanged();
}
