#pragma once
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <optional>

// EndfieldHUD FacilityAlertMonitor: edge-triggered alerts and 120 s recovery.
class FacilityController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList alerts READ alerts NOTIFY alertsChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit FacilityController(QObject *parent = nullptr, bool automatic = true);
    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    QVariantList alerts() const;
    Q_INVOKABLE void dismiss(const QString &kind);
    Q_INVOKABLE void markPresented(const QString &kind);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap bootFacts() const;
    void accept(const QVariantMap &sample, double now);
signals:
    void alertsChanged();
    void enabledChanged();
private:
    struct Condition { bool active = false, dismissed = false, isNew = true; std::optional<double> recovering; QVariantMap reading; };
    bool m_enabled = false, m_automatic = true, m_pending = false;
    quint64 m_generation = 0;
    QMap<QString, Condition> m_conditions;
    QVariantMap m_readings;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QFutureWatcher<QVariantMap> m_watcher;
};
