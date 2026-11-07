#pragma once
#include "endfield/core/data/event_log_store.hpp"
#include <QObject>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QTimer>
#include <QVariantMap>

class EventLogController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    explicit EventLogController(QObject *parent = nullptr, QString appRoot = {});
    ~EventLogController() override;
    QVariantMap state() const;
    bool active() const { return m_active; }
    void setActive(bool value);
    Q_INVOKABLE void record(const QString &kind, const QVariantMap &metadata = {});
    Q_INVOKABLE void recordAction(const QString &kind, const QString &action, bool coalesce = false);
    Q_INVOKABLE void clear();
    void flush();
    void observeWork(const QVariantMap &state);
    void observeBattery(const QVariantMap &state);
    void observeAudio(const QVariantMap &state);
signals:
    void stateChanged();
    void activeChanged();
private:
    void save();
    void publishPending();
    double now() const { return m_clock.elapsed() / 1000.; }
    static std::string compactName(std::string_view value);
    ehud::data::EventLogStore m_store;
    QElapsedTimer m_clock;
    QThreadPool m_worker;
    QFutureWatcher<ehud::data::EventLogSaveResult> m_save;
    QTimer m_saveTimer, m_dateTimer, m_actionTimer;
    QMap<QString, QString> m_pendingActions;
    QVariantMap m_workBaseline, m_batteryBaseline;
    QMap<QString, QString> m_audioBaseline;
    bool m_audioReady = false;
    bool m_active = false;
};
