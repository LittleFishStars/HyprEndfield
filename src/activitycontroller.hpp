#pragma once

#include "activitycollector.hpp"
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <memory>

class ActivityController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY demandChanged)
    Q_PROPERTY(bool applications READ applications WRITE setApplications NOTIFY demandChanged)
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
public:
    explicit ActivityController(QObject *parent = nullptr, Activity::Paths paths = {}, bool automatic = true);
    ~ActivityController() override;
    bool active() const { return m_active; }
    bool applications() const { return m_applications; }
    void setActive(bool active);
    void setApplications(bool applications);
    QVariantMap state() const;
    Q_INVOKABLE void setSort(const QString &key, bool descending);
    void accept(const Activity::Reading &reading);
signals:
    void demandChanged();
    void stateChanged();
private:
    void request();
    bool m_active = false, m_applications = false, m_automatic = true;
    bool m_appsReady = false, m_pending = false, m_busy = false;
    QString m_sortKey = "cpu";
    bool m_descending = true;
    std::optional<Activity::Reading> m_previous;
    QVariantMap m_latest;
    QVariantList m_history, m_apps;
    QTimer m_timer;
    QFutureWatcher<Activity::Reading> m_watcher;
    std::shared_ptr<Activity::Collector> m_collector;
};
