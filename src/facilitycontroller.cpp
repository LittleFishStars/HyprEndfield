#include "facilitycontroller.hpp"
#include <QFile>
#include <QDir>
#include <QStorageInfo>
#include <QtConcurrentRun>
#include <algorithm>

namespace {
QVariantMap readSystem() {
    QVariantMap values;
    const auto temperature = FacilityController::readTemperature();
    if (!temperature.isEmpty()) values["heat"] = temperature;
    const auto disk = QStorageInfo::root();
    if (disk.isValid() && disk.isReady() && disk.bytesTotal() > 0 && disk.bytesAvailable() >= 0)
        values["disk"] = QVariantMap{{"total", disk.bytesTotal()}, {"available", disk.bytesAvailable()}, {"name", disk.rootPath()}};
    QFile memory("/proc/meminfo");
    if (memory.open(QIODevice::ReadOnly)) {
        QHash<QByteArray, double> fields;
        for (const auto &line : memory.readAll().split('\n')) {
            const auto pair = line.simplified().split(' ');
            if (pair.size() >= 2) fields[pair[0]] = pair[1].toDouble() * 1024;
        }
        const double total = fields.value("MemTotal:"), available = fields.value("MemAvailable:", -1);
        if (total > 0 && available >= 0 && available <= total) values["mem"] = QVariantMap{{"total", total}, {"used", total - available}};
    }
    return values;
}
}
QVariantMap FacilityController::readTemperature(const QString &sys) {
    const auto text = [](const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString(); };
    const auto number = [&](const QString &path) { bool ok; double value = text(path).toDouble(&ok); return ok ? value / 1000. : -999.; };
    QVariantMap result; double margin = -1000;
    for (const auto &device : QDir(sys + "/class/hwmon").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString base = sys + "/class/hwmon/" + device + '/';
        for (const auto &sensor : QDir(base).entryList({"temp*_input"}, QDir::Files)) {
            const QString stem = sensor.left(sensor.size() - 6);
            if (text(base + stem + "_fault") == "1" || text(base + stem + "_enable") == "0") continue;
            const double current = number(base + sensor), maximum = number(base + stem + "_max"), critical = number(base + stem + "_crit");
            const double limit = maximum > 0 && maximum < 200 ? maximum : critical;
            if (current < -20 || current > 200 || limit <= 0 || limit > 200) continue;
            // Hardware-defined limits only; do not invent one limit for CPU, SSD and battery.
            if (current - limit <= margin) continue;
            margin = current - limit;
            result = {{"temperature", current}, {"limit", limit}, {"name", text(base + "name") + " " + text(base + stem + "_label")}};
        }
    }
    return result;
}
FacilityController::FacilityController(QObject *parent, bool automatic) : QObject(parent), m_automatic(automatic) {
    m_clock.start(); m_timer.setInterval(30000);
    connect(&m_timer, &QTimer::timeout, this, &FacilityController::refresh);
}
void FacilityController::setEnabled(bool on) {
    if (on == m_enabled) return;
    m_enabled = on; ++m_generation; m_conditions.clear();
    if (on && m_automatic) { m_timer.start(); refresh(); } else m_timer.stop();
    emit enabledChanged(); emit alertsChanged();
}
void FacilityController::refresh() {
    if (!m_enabled || !m_automatic) return;
    if (m_watcher.isRunning()) { m_pending = true; return; }
    const auto generation = m_generation;
    m_watcher.disconnect(this);
    connect(&m_watcher, &QFutureWatcherBase::finished, this, [this, generation] {
        if (m_enabled && generation == m_generation) accept(m_watcher.result(), m_clock.elapsed() / 1000.);
        if (m_pending) { m_pending = false; refresh(); }
    });
    m_watcher.setFuture(QtConcurrent::run(readSystem));
}
void FacilityController::accept(const QVariantMap &sample, double now) {
    if (!m_enabled) return;
    for (const auto &kind : {QString("disk"), QString("mem"), QString("heat")}) {
        if (!sample.contains(kind)) continue; // An unavailable source preserves its last reading.
        const auto reading = sample.value(kind).toMap();
        const bool heat = kind == "heat";
        const double total = reading.value(heat ? "limit" : "total").toDouble();
        const double quantity = reading.value(heat ? "temperature" : kind == "disk" ? "available" : "used", -1).toDouble();
        if (total <= 0 || quantity < 0 || (!heat && quantity > total)) continue;
        m_readings[kind] = reading;
        const double fraction = quantity / total;
        const bool raise = heat ? quantity >= total : kind == "disk" ? fraction < .10 : fraction >= .90;
        const bool recover = heat ? quantity <= total - 5 : kind == "disk" ? fraction >= .12 : fraction < .90;
        auto &condition = m_conditions[kind]; condition.reading = reading;
        if (raise) {
            condition.recovering.reset();
            if (!condition.active) { condition.active = true; condition.dismissed = false; condition.isNew = true; }
        } else if (recover && condition.active) {
            if (!condition.recovering) condition.recovering = now;
            if (now - *condition.recovering >= 120) condition = {};
        } else condition.recovering.reset();
    }
    emit alertsChanged();
}
QVariantList FacilityController::alerts() const {
    QVariantList out;
    for (auto it = m_conditions.cbegin(); it != m_conditions.cend(); ++it) {
        const auto &condition = it.value(); if (!condition.active || condition.dismissed) continue;
        if (it.key() == "heat") {
            const auto &r = condition.reading;
            out.append(QVariantMap{{"kind", "heat"}, {"module", "activity"}, {"isNew", condition.isNew},
                {"title", tr("设备温度过高")}, {"english", "HIGH TEMPERATURE"}, {"tag", tr("温度")},
                {"value", QString::number(qRound(r.value("temperature").toDouble())) + "°C"}, {"unit", tr("温度")},
                {"detail", tr("%1 · 硬件温度上限 %2°C").arg(r.value("name").toString()).arg(r.value("limit").toDouble(), 0, 'f', 0)}});
            continue;
        }
        const bool disk = it.key() == "disk"; const auto &r = condition.reading;
        const double total = r.value("total").toDouble(), amount = r.value(disk ? "available" : "used").toDouble();
        out.append(QVariantMap{{"kind", it.key()}, {"module", disk ? "storage" : "activity"}, {"isNew", condition.isNew},
            {"title", disk ? tr("存储空间不足") : tr("内存压力过高")}, {"english", disk ? "LOW DISK SPACE" : "HIGH MEMORY PRESSURE"},
            {"tag", disk ? tr("磁盘") : QString("RAM")}, {"value", QString::number(qRound(100 * amount / total)) + '%'},
            {"unit", disk ? tr("可用") : tr("压力")},
            {"detail", disk ? tr("%1  剩余 %2 GB").arg(r.value("name").toString()).arg(amount / 1e9, 0, 'f', 1)
                            : tr("已用 %1 / %2 GB").arg(amount / 1e9, 0, 'f', 1).arg(total / 1e9, 0, 'f', 1)}});
    }
    return out;
}
void FacilityController::dismiss(const QString &kind) {
    if (!m_conditions.contains(kind)) return;
    m_conditions[kind].dismissed = true; emit alertsChanged();
}
void FacilityController::markPresented(const QString &kind) {
    if (!m_conditions.contains(kind) || !m_conditions[kind].isNew) return;
    m_conditions[kind].isNew = false; emit alertsChanged();
}
QVariantMap FacilityController::bootFacts() const {
    QVariantMap result = m_readings;
    QFile uptime("/proc/uptime");
    if (uptime.open(QIODevice::ReadOnly)) { bool ok; const auto value = uptime.readAll().split(' ').first().toDouble(&ok); if (ok) result["uptime"] = value; }
    return result;
}
