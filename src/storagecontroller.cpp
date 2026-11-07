#include "storagecontroller.hpp"
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QStorageInfo>
#include <QSet>
#include <QtConcurrentRun>
#include <algorithm>

StorageController::StorageController(QObject *parent) : QObject(parent) {
    m_selection = QSettings().value("storage/volume").toString();
    m_timer.setInterval(60'000);
    connect(&m_timer, &QTimer::timeout, this, &StorageController::refresh);
    connect(&m_reader, &QFutureWatcherBase::finished, this, [this] { m_volumes = m_reader.result(); emit changed(); });
}
StorageController::~StorageController() { m_reader.waitForFinished(); }
QVariantList StorageController::readVolumes() {
    // Select local block-backed mounts before statfs; a stalled network share
    // must not prevent a reading. Btrfs subvolumes share the same capacity.
    QFile mounts("/proc/self/mountinfo"); if (!mounts.open(QIODevice::ReadOnly)) return {};
    QList<QPair<QString, QString>> paths{{"/", ""}};
    for (const auto &line : mounts.readAll().split('\n')) {
        const auto halves = line.split(' '); const int separator = halves.indexOf("-");
        if (separator < 6 || separator + 2 >= halves.size()) continue;
        const QByteArray device = halves[separator + 2], type = halves[separator + 1];
        if (!device.startsWith("/dev/") || type == "squashfs" || type == "iso9660") continue;
        QByteArray path = halves[4]; path.replace("\\040", " ").replace("\\011", "\t").replace("\\134", "\\");
        if (path != "/") paths.append({QString::fromUtf8(path), QString::fromUtf8(device)});
    }
    QVariantList result; QSet<QByteArray> seen;
    for (const auto &[path, ignored] : paths) {
        Q_UNUSED(ignored)
        QStorageInfo disk(path);
        if (!disk.isValid() || !disk.isReady() || disk.bytesTotal() <= 0 || disk.bytesAvailable() < 0) continue;
        const QByteArray identity = disk.device().isEmpty() ? path.toUtf8() : disk.device();
        if (seen.contains(identity)) continue;
        seen.insert(identity);
        const qint64 total = disk.bytesTotal(), available = std::clamp(disk.bytesAvailable(), qint64(0), total);
        const QString name = disk.name().isEmpty() ? path : disk.name();
        result.append(QVariantMap{{"id", QString::fromUtf8(identity)}, {"path", path}, {"name", name},
            {"total", total}, {"available", available}, {"used", total - available}, {"fraction", double(total - available) / total}});
    }
    return result;
}
QVariantMap StorageController::state() const {
    QVariantMap selected;
    for (const auto &value : m_volumes) if (value.toMap().value("id").toString() == m_selection) { selected = value.toMap(); break; }
    if (selected.isEmpty() && !m_volumes.isEmpty()) selected = m_volumes.first().toMap();
    return {{"volumes", m_volumes}, {"volume", selected}, {"loading", m_reader.isRunning()}};
}
void StorageController::setActive(bool active) {
    if (m_active == active) return;
    m_active = active;
    if (active) { refresh(); m_timer.start(); } else m_timer.stop();
    emit activeChanged();
}
void StorageController::refresh() {
    if (m_reader.isRunning()) return;
    m_reader.setFuture(QtConcurrent::run(readVolumes)); emit changed();
}
void StorageController::selectVolume(const QString &id) {
    for (const auto &value : m_volumes) if (value.toMap().value("id").toString() == id) {
        m_selection = id; QSettings().setValue("storage/volume", id); emit changed(); return;
    }
}
bool StorageController::openDolphin() {
    return QProcess::startDetached("dolphin", {state().value("volume").toMap().value("path", "/").toString()});
}
