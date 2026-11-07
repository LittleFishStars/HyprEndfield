#include "activitycontroller.hpp"
#include <QtConcurrent/QtConcurrentRun>

ActivityController::ActivityController(QObject *parent, Activity::Paths paths, bool automatic)
    : QObject(parent), m_automatic(automatic), m_collector(std::make_shared<Activity::Collector>(std::move(paths)))
{
    m_latest = Activity::derive(Activity::Reading{}, nullptr);
    m_timer.setInterval(5000);
    connect(&m_timer, &QTimer::timeout, this, &ActivityController::request);
    connect(&m_watcher, &QFutureWatcher<Activity::Reading>::finished, this, [this] {
        m_busy = false;
        accept(m_watcher.result());
        if (m_pending) { m_pending = false; request(); }
    });
    if (m_automatic) { m_timer.start(); QTimer::singleShot(0, this, &ActivityController::request); }
}
ActivityController::~ActivityController() { m_timer.stop(); m_watcher.waitForFinished(); }
void ActivityController::setActive(bool active) {
    if (active == m_active) return;
    m_active = active; m_timer.setInterval(active ? 1000 : 5000);
    emit demandChanged();
    if (active) request();
}
void ActivityController::setApplications(bool applications) {
    if (applications == m_applications) return;
    m_applications = applications; emit demandChanged();
    if (m_active && applications) request();
}
void ActivityController::request() {
    if (!m_automatic) return;
    if (m_busy) { m_pending = true; return; }
    m_busy = true;
    const bool applications = m_active && m_applications;
    m_watcher.setFuture(QtConcurrent::run([collector = m_collector, applications] { return collector->sample(applications); }));
}
void ActivityController::accept(const Activity::Reading &reading) {
    m_latest = Activity::derive(reading, m_previous ? &*m_previous : nullptr);
    m_history.append(m_latest); if (m_history.size() > 60) m_history.removeFirst();
    if (reading.applications && m_active && m_applications) {
        m_apps = Activity::deriveApps(reading, m_previous ? &*m_previous : nullptr); m_appsReady = true;
    }
    auto next = reading;
    // DRM counters may temporarily go backwards; retain their high-water mark.
    if (m_previous) for (auto it = next.processes.begin(); it != next.processes.end(); ++it) {
        const auto old = m_previous->processes.constFind(it.key());
        if (old == m_previous->processes.cend() || old->start != it->start) continue;
        for (auto engine = it->gpuEngines.begin(); engine != it->gpuEngines.end(); ++engine)
            engine.value() = std::max(engine.value(), old->gpuEngines.value(engine.key()));
    }
    m_previous = std::move(next); emit stateChanged();
}
void ActivityController::setSort(const QString &key, bool descending) {
    if (!QStringList{"name", "gpu", "cpu", "memory", "threads"}.contains(key)) return;
    m_sortKey = key; m_descending = descending; emit stateChanged();
}
QVariantMap ActivityController::state() const {
    return {{"latest", m_latest}, {"history", m_history}, {"apps", Activity::sortedApps(m_apps, m_sortKey, m_descending)},
        {"appsReady", m_appsReady}, {"sortKey", m_sortKey}, {"descending", m_descending}};
}
