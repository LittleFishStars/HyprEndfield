#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <pulse/pulseaudio.h>

// PipeWire's PulseAudio interface supplies device selection and native stream
// gain. No microphone recording, audio interception, or persistent service.
class AudioController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
public:
    explicit AudioController(QObject *parent = nullptr, const QString &server = {});
    ~AudioController() override;
    bool active() const { return m_active; }
    void setActive(bool active);
    QVariantMap state() const { return m_state; }
    Q_INVOKABLE bool selectDevice(const QString &key, bool input);
    Q_INVOKABLE bool setVolume(double percent, const QString &outputKey);
    Q_INVOKABLE bool setBalance(double percent, const QString &outputKey);
    Q_INVOKABLE bool setMuted(bool muted, const QString &outputKey);
    Q_INVOKABLE bool setApplicationVolume(double percent, const QString &applicationKey);
signals:
    void activeChanged();
    void stateChanged();
    void commandFailed(const QString &message);
private:
    void connectServer();
    void disconnectServer();
    void connected();
    void failed(const QString &message);
    void scheduleRefresh();
    void refresh();
    void finishPart(bool success);
    void publish();
    bool accept(pa_operation *operation);
    QVariantMap currentOutput(const QString &key) const;
    static void contextState(pa_context *, void *);
    static void subscribed(pa_context *, pa_subscription_event_type_t, uint32_t, void *);
    static void serverInfo(pa_context *, const pa_server_info *, void *);
    static void sinkInfo(pa_context *, const pa_sink_info *, int, void *);
    static void sourceInfo(pa_context *, const pa_source_info *, int, void *);
    static void streamInfo(pa_context *, const pa_sink_input_info *, int, void *);
    static void commandDone(pa_context *, int, void *);
    template<typename F> void deliver(F fn) {
        const auto epoch = m_epoch; // Changed only after the Pulse thread joins.
        QMetaObject::invokeMethod(this, [this, epoch, fn] {
            if (epoch == m_epoch && m_active) fn();
        }, Qt::QueuedConnection);
    }
    QString m_server;
    bool m_active = false;
    bool m_ready = false;
    bool m_refreshAgain = false;
    bool m_batchOk = true;
    quint64 m_epoch = 0;
    int m_pending = 0;
    pa_threaded_mainloop *m_loop = nullptr;
    pa_context *m_context = nullptr;
    QTimer m_refreshTimer;
    QTimer m_retryTimer;
    QVariantMap m_state;
    QVariantList m_outputs, m_inputs, m_streams;
    QVariantList m_nextOutputs, m_nextInputs, m_nextStreams;
    QString m_defaultOutput, m_defaultInput;
};
