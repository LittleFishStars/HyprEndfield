#include <QCoreApplication>
#include "audiocontroller.hpp"

#include <QMap>
#include <algorithm>
#include <cmath>

namespace {
QString string(const char *value) { return QString::fromUtf8(value ? value : ""); }
QString pulseProperty(pa_proplist *list, const char *name) { return string(pa_proplist_gets(list, name)); }
QVariantMap volumeInfo(uint32_t id, const pa_cvolume &volume, const pa_channel_map &map)
{
    QVariantList levels, positions;
    for (int i = 0; i < volume.channels; ++i) levels.append(volume.values[i]);
    for (int i = 0; i < map.channels; ++i) positions.append(int(map.map[i]));
    return {{"id", id}, {"levels", levels}, {"positions", positions},
            {"volume", 100.0 * pa_cvolume_max(&volume) / PA_VOLUME_NORM},
            {"balance", 100.0 * pa_cvolume_get_balance(&volume, &map)},
            {"canBalance", bool(pa_channel_map_can_balance(&map))}};
}
pa_cvolume levels(const QVariantMap &item)
{
    pa_cvolume result{};
    const auto values = item.value("levels").toList();
    result.channels = std::min(qsizetype(PA_CHANNELS_MAX), values.size());
    for (int i = 0; i < result.channels; ++i) result.values[i] = values[i].toUInt();
    return result;
}
pa_channel_map channelMap(const QVariantMap &item)
{
    pa_channel_map result{};
    const auto values = item.value("positions").toList();
    result.channels = std::min(qsizetype(PA_CHANNELS_MAX), values.size());
    for (int i = 0; i < result.channels; ++i) result.map[i] = pa_channel_position_t(values[i].toInt());
    return result;
}
QVariantMap emptyState()
{
    return {{"ready", false}, {"outputs", QVariantList{}}, {"inputs", QVariantList{}},
            {"applications", QVariantList{}}, {"outputKey", ""}, {"inputKey", ""},
            {"outputName", "—"}, {"inputName", "—"}, {"volume", 0.0}, {"balance", 0.0},
            {"muted", false}, {"canBalance", false}, {"status", ""}};
}
}

AudioController::AudioController(QObject *parent, const QString &server)
    : QObject(parent), m_server(server), m_state(emptyState())
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(60);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AudioController::refresh);
    m_retryTimer.setSingleShot(true);
    m_retryTimer.setInterval(3000);
    connect(&m_retryTimer, &QTimer::timeout, this, &AudioController::connectServer);
}
AudioController::~AudioController() { disconnectServer(); }
void AudioController::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    if (active) connectServer();
    else {
        disconnectServer();
        m_state = emptyState();
        emit stateChanged();
    }
    emit activeChanged();
}
void AudioController::connectServer()
{
    if (!m_active || m_context) return;
    m_loop = pa_threaded_mainloop_new();
    if (!m_loop) { failed(QCoreApplication::translate("HyprEndfield", "无法创建音频连接")); return; }
    m_context = pa_context_new(pa_threaded_mainloop_get_api(m_loop), "HyprEndfield");
    if (!m_context || pa_threaded_mainloop_start(m_loop) < 0) {
        failed(QCoreApplication::translate("HyprEndfield", "无法启动音频连接")); return;
    }
    pa_threaded_mainloop_lock(m_loop);
    pa_context_set_state_callback(m_context, contextState, this);
    const auto server = m_server.toUtf8();
    const int result = pa_context_connect(m_context, server.isEmpty() ? nullptr : server.constData(), PA_CONTEXT_NOAUTOSPAWN, nullptr);
    pa_threaded_mainloop_unlock(m_loop);
    if (result < 0) failed(QCoreApplication::translate("HyprEndfield", "无法连接音频服务"));
}
void AudioController::disconnectServer()
{
    m_refreshTimer.stop(); m_retryTimer.stop();
    // stop() joins the callback thread; never call it with its lock held.
    if (m_loop) pa_threaded_mainloop_stop(m_loop);
    if (m_context) {
        pa_context_set_state_callback(m_context, nullptr, nullptr);
        pa_context_disconnect(m_context);
        pa_context_unref(m_context);
        m_context = nullptr;
    }
    if (m_loop) { pa_threaded_mainloop_free(m_loop); m_loop = nullptr; }
    ++m_epoch;
    m_ready = false; m_pending = 0; m_refreshAgain = false;
    m_outputs.clear(); m_inputs.clear(); m_streams.clear();
}
void AudioController::contextState(pa_context *context, void *data)
{
    auto *self = static_cast<AudioController *>(data);
    switch (pa_context_get_state(context)) {
    case PA_CONTEXT_READY: self->deliver([self] { self->connected(); }); break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED: {
        const QString error = string(pa_strerror(pa_context_errno(context)));
        self->deliver([self, error] { self->failed(QCoreApplication::translate("HyprEndfield", "音频连接已断开：") + error); });
        break;
    }
    default: break;
    }
}
void AudioController::connected()
{
    m_ready = true;
    pa_threaded_mainloop_lock(m_loop);
    pa_context_set_subscribe_callback(m_context, subscribed, this);
    accept(pa_context_subscribe(m_context, pa_subscription_mask_t(PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SOURCE
        | PA_SUBSCRIPTION_MASK_SINK_INPUT | PA_SUBSCRIPTION_MASK_SERVER), commandDone, this));
    pa_threaded_mainloop_unlock(m_loop);
    refresh();
}
void AudioController::failed(const QString &message)
{
    disconnectServer();
    m_state = emptyState(); m_state["status"] = message;
    emit stateChanged(); emit commandFailed(message);
    if (m_active) m_retryTimer.start();
}
void AudioController::subscribed(pa_context *, pa_subscription_event_type_t, uint32_t, void *data)
{
    auto *self = static_cast<AudioController *>(data);
    self->deliver([self] { self->scheduleRefresh(); });
}
void AudioController::scheduleRefresh()
{
    if (m_pending) m_refreshAgain = true;
    else if (!m_refreshTimer.isActive()) m_refreshTimer.start(60);
}
void AudioController::refresh()
{
    if (!m_ready) return;
    if (m_pending) { m_refreshAgain = true; return; }
    m_pending = 4; m_batchOk = true; m_refreshAgain = false;
    m_nextOutputs.clear(); m_nextInputs.clear(); m_nextStreams.clear();
    pa_threaded_mainloop_lock(m_loop);
    auto request = [this](pa_operation *operation) {
        if (operation) pa_operation_unref(operation);
        else deliver([this] { finishPart(false); });
    };
    request(pa_context_get_server_info(m_context, serverInfo, this));
    request(pa_context_get_sink_info_list(m_context, sinkInfo, this));
    request(pa_context_get_source_info_list(m_context, sourceInfo, this));
    request(pa_context_get_sink_input_info_list(m_context, streamInfo, this));
    pa_threaded_mainloop_unlock(m_loop);
}
void AudioController::serverInfo(pa_context *, const pa_server_info *info, void *data)
{
    auto *self = static_cast<AudioController *>(data);
    const bool ok = info;
    const auto output = info ? string(info->default_sink_name) : QString();
    const auto input = info ? string(info->default_source_name) : QString();
    self->deliver([self, ok, output, input] {
        self->m_defaultOutput = output; self->m_defaultInput = input; self->finishPart(ok);
    });
}
void AudioController::sinkInfo(pa_context *, const pa_sink_info *info, int end, void *data)
{
    auto *self = static_cast<AudioController *>(data);
    if (end) { self->deliver([self, end] { self->finishPart(end > 0); }); return; }
    if (!info) return;
    auto item = volumeInfo(info->index, info->volume, info->channel_map);
    item["name"] = string(info->name); item["label"] = string(info->description);
    item["muted"] = bool(info->mute);
    self->deliver([self, item] { self->m_nextOutputs.append(item); });
}
void AudioController::sourceInfo(pa_context *, const pa_source_info *info, int end, void *data)
{
    auto *self = static_cast<AudioController *>(data);
    if (end) { self->deliver([self, end] { self->finishPart(end > 0); }); return; }
    if (!info || info->monitor_of_sink != PA_INVALID_INDEX) return;
    const QVariantMap item{{"id", info->index}, {"name", string(info->name)}, {"label", string(info->description)}};
    self->deliver([self, item] { self->m_nextInputs.append(item); });
}
void AudioController::streamInfo(pa_context *, const pa_sink_input_info *info, int end, void *data)
{
    auto *self = static_cast<AudioController *>(data);
    if (end) { self->deliver([self, end] { self->finishPart(end > 0); }); return; }
    if (!info || !info->has_volume || !info->volume_writable) return;
    auto item = volumeInfo(info->index, info->volume, info->channel_map);
    const QString pid = pulseProperty(info->proplist, PA_PROP_APPLICATION_PROCESS_ID);
    QString name = pulseProperty(info->proplist, PA_PROP_APPLICATION_NAME);
    if (name.isEmpty()) name = pulseProperty(info->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);
    if (name.isEmpty()) name = string(info->name);
    item["pid"] = pid;
    item["label"] = name;
    item["icon"] = pulseProperty(info->proplist, PA_PROP_APPLICATION_ICON_NAME);
    item["group"] = pid.isEmpty() ? QString("client:%1").arg(info->client == PA_INVALID_INDEX ? info->index : info->client) : "pid:" + pid;
    self->deliver([self, item] { self->m_nextStreams.append(item); });
}
void AudioController::finishPart(bool success)
{
    if (!m_pending) return;
    m_batchOk &= success;
    if (--m_pending) return;
    if (m_batchOk) {
        m_outputs = m_nextOutputs; m_inputs = m_nextInputs; m_streams = m_nextStreams;
        publish();
    } else {
        m_state["ready"] = false;
        m_state["status"] = QCoreApplication::translate("HyprEndfield", "暂时无法读取音频设备");
        emit stateChanged();
        m_refreshAgain = true;
    }
    if (m_refreshAgain) m_refreshTimer.start(m_batchOk ? 60 : 1000);
}
void AudioController::publish()
{
    auto next = emptyState();
    next["ready"] = true;
    auto devices = [this, &next](QVariantList &list, const QString &name, bool input) {
        for (auto &value : list) {
            auto item = value.toMap();
            item["key"] = QString("%1:%2:%3").arg(m_epoch).arg(item["id"].toUInt()).arg(item["name"].toString());
            value = item;
            if (item["name"].toString() != name) continue;
            next[input ? "inputKey" : "outputKey"] = item["key"];
            next[input ? "inputName" : "outputName"] = item["label"];
            if (!input) for (const char *key : {"volume", "balance", "canBalance", "muted"}) next[key] = item[key];
        }
    };
    devices(m_outputs, m_defaultOutput, false); devices(m_inputs, m_defaultInput, true);
    next["outputs"] = m_outputs; next["inputs"] = m_inputs;
    QMap<QString, QVariantMap> groups;
    for (const auto &value : m_streams) {
        const auto stream = value.toMap();
        const QString group = stream["group"].toString();
        auto &app = groups[group];
        if (app.isEmpty()) {
            app = stream;
            app["key"] = QString::number(m_epoch) + ':' + group;
            app["members"] = QVariantList{};
        }
        auto members = app["members"].toList(); members.append(stream["id"]); app["members"] = members;
        app["key"] = app["key"].toString() + ':' + stream["id"].toString();
        app["volume"] = std::max(app["volume"].toDouble(), stream["volume"].toDouble());
    }
    QVariantList apps;
    for (const auto &app : groups) apps.append(app);
    std::sort(apps.begin(), apps.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["label"].toString().localeAwareCompare(b.toMap()["label"].toString()) < 0;
    });
    next["applications"] = apps;
    if (m_state != next) { m_state = next; emit stateChanged(); }
}
QVariantMap AudioController::currentOutput(const QString &key) const
{
    if (!m_active || !m_ready || !m_state["ready"].toBool() || key.isEmpty() || key != m_state["outputKey"].toString()) return {};
    for (const auto &value : m_outputs) if (value.toMap()["key"].toString() == key) return value.toMap();
    return {};
}
bool AudioController::accept(pa_operation *operation)
{
    if (!operation) return false;
    pa_operation_unref(operation);
    return true;
}
void AudioController::commandDone(pa_context *context, int success, void *data)
{
    auto *self = static_cast<AudioController *>(data);
    const QString error = success ? QString() : string(pa_strerror(pa_context_errno(context)));
    self->deliver([self, success, error] {
        if (!success) {
            const QString message = QCoreApplication::translate("HyprEndfield", "音频设置未生效：") + error;
            self->m_state["status"] = message;
            emit self->stateChanged(); emit self->commandFailed(message);
        }
        self->scheduleRefresh();
    });
}
bool AudioController::selectDevice(const QString &key, bool input)
{
    if (!m_ready || !m_state["ready"].toBool()) return false;
    for (const auto &value : input ? m_inputs : m_outputs) {
        const auto item = value.toMap();
        if (item["key"].toString() != key) continue;
        const auto name = item["name"].toString().toUtf8();
        pa_threaded_mainloop_lock(m_loop);
        const bool ok = accept(input ? pa_context_set_default_source(m_context, name.constData(), commandDone, this)
                                     : pa_context_set_default_sink(m_context, name.constData(), commandDone, this));
        pa_threaded_mainloop_unlock(m_loop);
        return ok;
    }
    return false;
}
bool AudioController::setVolume(double percent, const QString &key)
{
    const auto item = currentOutput(key);
    if (item.isEmpty() || !std::isfinite(percent)) return false;
    auto volume = levels(item);
    if (!pa_cvolume_valid(&volume)) return false;
    pa_cvolume_scale(&volume, pa_volume_t(std::lround(std::clamp(percent, 0.0, 100.0) * PA_VOLUME_NORM / 100.0)));
    pa_threaded_mainloop_lock(m_loop);
    const bool ok = accept(pa_context_set_sink_volume_by_index(m_context, item["id"].toUInt(), &volume, commandDone, this));
    pa_threaded_mainloop_unlock(m_loop);
    return ok;
}
bool AudioController::setBalance(double percent, const QString &key)
{
    const auto item = currentOutput(key);
    if (item.isEmpty() || !item["canBalance"].toBool() || !std::isfinite(percent)) return false;
    auto volume = levels(item); const auto map = channelMap(item);
    if (!pa_cvolume_set_balance(&volume, &map, std::clamp(percent / 100.0, -1.0, 1.0))) return false;
    pa_threaded_mainloop_lock(m_loop);
    const bool ok = accept(pa_context_set_sink_volume_by_index(m_context, item["id"].toUInt(), &volume, commandDone, this));
    pa_threaded_mainloop_unlock(m_loop);
    return ok;
}
bool AudioController::setMuted(bool muted, const QString &key)
{
    const auto item = currentOutput(key);
    if (item.isEmpty()) return false;
    pa_threaded_mainloop_lock(m_loop);
    const bool ok = accept(pa_context_set_sink_mute_by_index(m_context, item["id"].toUInt(), muted, commandDone, this));
    pa_threaded_mainloop_unlock(m_loop);
    return ok;
}
bool AudioController::setApplicationVolume(double percent, const QString &key)
{
    if (!m_ready || !m_state["ready"].toBool() || !std::isfinite(percent)) return false;
    QVariantList members;
    for (const auto &value : m_state["applications"].toList())
        if (value.toMap()["key"].toString() == key) members = value.toMap()["members"].toList();
    if (members.isEmpty()) return false;
    bool ok = true;
    pa_threaded_mainloop_lock(m_loop);
    for (const auto &value : m_streams) {
        const auto stream = value.toMap();
        if (!members.contains(stream["id"])) continue;
        auto volume = levels(stream);
        pa_cvolume_scale(&volume, pa_volume_t(std::lround(std::clamp(percent, 0.0, 100.0) * PA_VOLUME_NORM / 100.0)));
        ok &= accept(pa_context_set_sink_input_volume(m_context, stream["id"].toUInt(), &volume, commandDone, this));
    }
    pa_threaded_mainloop_unlock(m_loop);
    return ok;
}
