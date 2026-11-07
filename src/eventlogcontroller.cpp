#include "eventlogcontroller.hpp"
#include "eventlogtext.hpp"
#include "endfield/core/data/data_store.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QStandardPaths>
#include <QTextBoundaryFinder>
#include <QtConcurrentRun>

namespace model = endfield::modules;
EventLogController::EventLogController(QObject *parent, QString appRoot)
    : QObject(parent), m_store(std::filesystem::path((appRoot.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : appRoot).toStdString()), 500, compactName) {
    m_clock.start();
    m_actionTimer.setSingleShot(true); m_actionTimer.setInterval(600);
    connect(&m_actionTimer, &QTimer::timeout, this, &EventLogController::publishPending);
    m_worker.setMaxThreadCount(1);
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(350);
    connect(&m_saveTimer, &QTimer::timeout, this, &EventLogController::save);
    connect(&m_save, &QFutureWatcherBase::finished, this, [this] {
        if (m_store.complete(m_save.result())) emit stateChanged();
        if (m_store.saveDeadline()) m_saveTimer.start();
    });
    m_dateTimer.setInterval(60000);
    connect(&m_dateTimer, &QTimer::timeout, this, &EventLogController::stateChanged);
}
EventLogController::~EventLogController() { flush(); }

std::string EventLogController::compactName(std::string_view value) {
    QString clean;
    const QString input = QString::fromUtf8(value.data(), value.size());
    for (const auto c : input) {
        const bool blank = c.isSpace() || c.category() == QChar::Other_Control || c.category() == QChar::Other_Format;
        if (blank) { if (!clean.isEmpty() && !clean.endsWith(' ')) clean += ' '; }
        else clean += c;
    }
    clean = clean.trimmed();
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, clean);
    QString result;
    int previous = 0;
    for (int end = finder.toNextBoundary(), count = 0; end >= 0 && count < 512; end = finder.toNextBoundary(), ++count) {
        const QString next = clean.mid(previous, end - previous);
        if (result.toUtf8().size() + next.toUtf8().size() > 160) break;
        result += next; previous = end;
    }
    return result.trimmed().toStdString();
}

QVariantMap EventLogController::state() const {
    QVariantList entries;
    for (const auto &event : m_store.events()) {
        if (event.kind == model::EventKind::overlayOpened || event.kind == model::EventKind::moduleOpened
            || event.kind == model::EventKind::mapPinStyleChanged || event.kind == model::EventKind::mapRecentered) continue;
        const auto date = QDateTime::fromMSecsSinceEpoch(qRound64((event.createdAt + 978307200.) * 1000.));
        const auto filter = model::eventLogFilterOf(event.kind);
        entries.append(QVariantMap{{"id", QString::fromStdString(event.id)},
            {"kind", QString::fromUtf8(model::eventKindKey(event.kind))}, {"filter", int(filter)},
            {"category", QString::fromStdString(model::eventLogFilterTitle(filter, endfield::core::Language::system))},
            {"text", QString::fromStdString(model::eventLogLine(event, endfield::core::Language::system))},
            {"time", date.toString("HH:mm")}, {"day", date.date() == QDate::currentDate() ? QString() : date.toString("MM.dd")}});
    }
    QString status;
    if (const auto message = m_store.statusMessage()) {
        if (message->starts_with("Saved log")) status = tr("保存的日志无法读取；原文件已保留。");
        else if (message->starts_with("This log")) status = tr("此日志使用了更新的格式；原文件已保留。");
        else status = tr("日志无法保存。新事件会保留在本次会话中。");
    }
    return {{"entries", entries}, {"count", entries.size()}, {"capacity", 500}, {"status", status}};
}
void EventLogController::setActive(bool value) {
    if (m_active == value) return;
    m_active = value;
    if (value) { m_dateTimer.start(); emit stateChanged(); } else m_dateTimer.stop();
    emit activeChanged();
}
void EventLogController::observeWork(const QVariantMap &state) {
    if (!m_workBaseline.isEmpty()) {
        const auto phase = state.value("phase").toString(), previous = m_workBaseline.value("phase").toString();
        QString kind;
        if (phase != previous) {
            if (phase == "running") kind = previous == "paused" ? "workResumed" : "workStarted";
            else if (phase == "paused") kind = "workPaused";
            else if (phase == "completed") kind = "workCompleted";
            else if (phase == "idle" && previous != "idle") kind = "workReset";
        }
        if (!kind.isEmpty()) record(kind, {{"kind", state.value("kind")}, {"seconds", state.value("duration")}});
    }
    m_workBaseline = state;
}
void EventLogController::observeBattery(const QVariantMap &state) {
    if (!state.value("available").toBool()) { m_batteryBaseline.clear(); return; }
    const QVariantMap fields{{"state", state.value("stateKey", "unavailable")}, {"percentage", state.value("percentage")}};
    if (!m_batteryBaseline.isEmpty()) {
        if (state.value("plugged") != m_batteryBaseline.value("plugged"))
            record(state.value("plugged").toBool() ? "powerConnected" : "powerDisconnected", fields);
        else if (state.value("stateKey") != m_batteryBaseline.value("stateKey")) record("batteryStateChanged", fields);
    }
    m_batteryBaseline = state;
}
void EventLogController::observeAudio(const QVariantMap &state) {
    if (!state.value("ready").toBool()) { m_audioReady = false; m_audioBaseline.clear(); return; }
    QMap<QString, QString> devices;
    for (const auto key : {"outputs", "inputs"}) for (const auto &value : state.value(key).toList()) {
        const auto item = value.toMap(); devices[QString(key) + '/' + item.value("name").toString()] = item.value("label").toString();
    }
    if (m_audioReady) {
        for (auto it = devices.begin(); it != devices.end(); ++it) if (!m_audioBaseline.contains(it.key())) record("audioDeviceConnected", {{"device", it.value()}});
        for (auto it = m_audioBaseline.begin(); it != m_audioBaseline.end(); ++it) if (!devices.contains(it.key())) record("audioDeviceDisconnected", {{"device", it.value()}});
    }
    m_audioBaseline = std::move(devices); m_audioReady = true;
}
void EventLogController::record(const QString &kind, const QVariantMap &metadata) {
    if (kind == "mapPinStyleChanged" || kind == "mapRecentered") return;
    for (unsigned n = 0; n <= unsigned(model::EventKind::accountAction); ++n) {
        const auto type = static_cast<model::EventKind>(n);
        if (QString::fromUtf8(model::eventKindKey(type)) != kind) continue;
        model::EventMetadata fields;
        for (auto it = metadata.begin(); it != metadata.end(); ++it) fields.emplace(it.key().toStdString(), it.value().toString().toStdString());
        try {
            m_store.record({ehud::data::makeUUID(), type, ehud::data::foundationNow(), std::move(fields)}, now());
            m_saveTimer.start(); emit stateChanged();
        } catch (const std::exception &) { /* Reject malformed metadata without copying it into diagnostic logs. */ }
        return;
    }
}
void EventLogController::recordAction(const QString &kind, const QString &action, bool coalesce) {
    if (coalesce) { m_pendingActions[kind + '/' + action] = action; m_actionTimer.start(); }
    else record(kind, {{"action", action}});
}
void EventLogController::publishPending() {
    const auto pending = std::exchange(m_pendingActions, {});
    m_actionTimer.stop();
    for (auto it = pending.begin(); it != pending.end(); ++it) record(it.key().section('/', 0, 0), {{"action", it.value()}});
}
void EventLogController::clear() { m_actionTimer.stop(); m_pendingActions.clear(); m_store.clear(now()); m_saveTimer.start(); emit stateChanged(); }
void EventLogController::save() {
    if (m_save.isRunning()) return;
    if (auto snapshot = m_store.takeSave(now())) m_save.setFuture(QtConcurrent::run(&m_worker, [copy = std::move(*snapshot)] { return copy.execute(); }));
    else if (m_store.saveDeadline()) m_saveTimer.start();
}
void EventLogController::flush() {
    publishPending();
    m_saveTimer.stop();
    m_worker.waitForDone();
    if (m_save.future().isValid()) m_store.complete(m_save.result());
    if (auto snapshot = m_store.takeSave(now(), true)) m_store.complete(snapshot->execute());
}
