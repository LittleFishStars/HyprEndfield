#pragma once

#include <QObject>
#include <QDate>
#include <QDateTime>
#include <QTimer>
#include <QVariantMap>

class CalendarController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit CalendarController(QObject *parent = nullptr, QString directory = {}, bool notifications = true);

    QVariantMap state() const;
    bool active() const { return m_active; }
    void setActive(bool value);

    Q_INVOKABLE bool saveEvent(const QString &title, const QString &details,
                               const QString &day, const QString &id = {});
    Q_INVOKABLE bool removeEvent(const QString &id);
    Q_INVOKABLE bool refreshReminders();

signals:
    void eventOccurred(const QString &action);
    void stateChanged();
    void activeChanged();

private:
    struct Event {
        QString id;
        QString title;
        QString details;
        QDate day;
        QDateTime created;
        QDateTime modified;
        bool catchUpPending = false;
        QDateTime catchUpAt;
        QString beforeDeliveredFor;
        QString dayDeliveredFor;
    };
    struct Reminder {
        int event = -1;
        QDateTime at;
        bool previousDay = false;
        bool catchUp = false;
    };

    void load();
    bool persist(const QList<Event> &events);
    void setError(const QString &value);
    void scheduleNext();
    QList<Reminder> reminders(const QDateTime &now) const;
    void deliverDue();
    void notify(const Event &event, bool previousDay);
    static QString dayString(const QDate &day);

    QString m_directory;
    QString m_path;
    QByteArray m_persisted;
    QList<Event> m_events;
    QTimer m_reminderTimer;
    QTimer m_clockTimer;
    QDate m_today;
    QString m_error;
    bool m_active = false;
    bool m_notifications = true;
    bool m_notificationAvailable = false;
};
