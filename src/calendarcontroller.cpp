#include <QCoreApplication>
#include "calendarcontroller.hpp"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
#include <limits>

namespace {
constexpr qsizetype maximumBytes = 3 * 1024 * 1024;
constexpr qsizetype maximumEvents = 256;
constexpr qsizetype maximumUpcoming = 30;

QByteArray readFile(const QString &path, bool *exists = nullptr, bool *ok = nullptr)
{
    QFile file(path);
    if (exists) *exists = file.exists();
    if (ok) *ok = true;
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) { if (ok) *ok = false; return {}; }
    return file.read(maximumBytes + 1);
}

QDateTime atNine(const QDate &day)
{
    return QDateTime(day, QTime(9, 0), QTimeZone::systemTimeZone());
}
}

CalendarController::CalendarController(QObject *parent, QString directory, bool notifications)
    : QObject(parent), m_notifications(notifications)
{
    if (directory.isEmpty())
        directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/HyprEndfield/Calendar";
    m_directory = std::move(directory);
    m_path = m_directory + "/calendar.json";
    m_today = QDate::currentDate();

    m_reminderTimer.setSingleShot(true);
    m_reminderTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_reminderTimer, &QTimer::timeout, this, &CalendarController::deliverDue);
    m_clockTimer.setInterval(60'000);
    connect(&m_clockTimer, &QTimer::timeout, this, [this] {
        const QDate current = QDate::currentDate();
        if (current != m_today) { m_today = current; emit stateChanged(); }
        scheduleNext();
    });
    m_clockTimer.start();

    if (m_notifications) {
        QDBusInterface notificationsInterface(QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"), QDBusConnection::sessionBus());
        m_notificationAvailable = notificationsInterface.isValid();
    }
    load();
    scheduleNext();
}

QString CalendarController::dayString(const QDate &day)
{
    return day.toString(QStringLiteral("yyyy-MM-dd"));
}

void CalendarController::load()
{
    bool exists = false, readable = true;
    const QByteArray bytes = readFile(m_path, &exists, &readable);
    if (!exists) { m_persisted.clear(); return; }
    if (!readable) { setError(QCoreApplication::translate("HyprEndfield", "无法读取日历数据，原文件已保留。")); return; }
    if (bytes.size() > maximumBytes) { setError(QCoreApplication::translate("HyprEndfield", "日历最多可保存 256 个事项。")); return; }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || document.object().value("version").toInt() != 1
        || !document.object().value("events").isArray()) {
        setError(QCoreApplication::translate("HyprEndfield", "无法读取日历数据，原文件已保留。"));
        return;
    }

    QList<Event> values;
    QSet<QString> ids;
    for (const auto &value : document.object().value("events").toArray()) {
        const auto object = value.toObject();
        Event event;
        event.id = object.value("id").toString().toUpper();
        event.title = object.value("title").toString();
        event.details = object.value("details").toString();
        event.day = QDate::fromString(object.value("day").toString(), Qt::ISODate);
        event.created = QDateTime::fromString(object.value("created").toString(), Qt::ISODateWithMs);
        event.modified = QDateTime::fromString(object.value("modified").toString(), Qt::ISODateWithMs);
        event.catchUpPending = object.value("catchUpPending").toBool();
        event.catchUpAt = QDateTime::fromString(object.value("catchUpAt").toString(), Qt::ISODateWithMs);
        event.beforeDeliveredFor = object.value("beforeDeliveredFor").toString();
        event.dayDeliveredFor = object.value("dayDeliveredFor").toString();
        const bool valid = QUuid(event.id).isNull() == false && !ids.contains(event.id)
            && !event.title.trimmed().isEmpty() && event.title.size() <= 120
            && event.details.size() <= 2000 && event.day.isValid()
            && event.day.year() >= 1900 && event.day.year() <= 9999
            && event.created.isValid() && event.modified.isValid();
        if (!valid) { setError(QCoreApplication::translate("HyprEndfield", "无法读取日历数据，原文件已保留。")); return; }
        ids.insert(event.id); values.append(std::move(event));
    }
    if (values.size() > maximumEvents) { setError(QCoreApplication::translate("HyprEndfield", "无法读取日历数据，原文件已保留。")); return; }
    m_events = std::move(values);
    m_persisted = bytes;
}

bool CalendarController::persist(const QList<Event> &events)
{
    bool exists = false, readable = true;
    const QByteArray current = readFile(m_path, &exists, &readable);
    if (!readable) { setError(QCoreApplication::translate("HyprEndfield", "无法读取日历数据，原文件已保留。")); return false; }
    if ((exists ? current : QByteArray()) != m_persisted) {
        setError(QCoreApplication::translate("HyprEndfield", "日历文件已被其他程序修改，请重启后再保存。"));
        return false;
    }

    QJsonArray list;
    for (const auto &event : events) {
        QJsonObject object{{"id", event.id}, {"title", event.title}, {"details", event.details},
            {"day", dayString(event.day)}, {"created", event.created.toString(Qt::ISODateWithMs)},
            {"modified", event.modified.toString(Qt::ISODateWithMs)},
            {"catchUpPending", event.catchUpPending}, {"beforeDeliveredFor", event.beforeDeliveredFor},
            {"dayDeliveredFor", event.dayDeliveredFor}};
        if (event.catchUpAt.isValid()) object["catchUpAt"] = event.catchUpAt.toString(Qt::ISODateWithMs);
        list.append(object);
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{{"version", 1}, {"events", list}}).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumBytes) { setError(QCoreApplication::translate("HyprEndfield", "日历最多可保存 256 个事项。")); return false; }
    if (!QDir().mkpath(m_directory)) { setError(QCoreApplication::translate("HyprEndfield", "无法保存日历数据。")); return false; }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setError(QCoreApplication::translate("HyprEndfield", "无法保存日历数据。")); return false;
    }
    m_events = events;
    m_persisted = bytes;
    m_error.clear();
    emit stateChanged();
    scheduleNext();
    return true;
}

void CalendarController::setError(const QString &value)
{
    m_error = value;
    emit stateChanged();
}

QVariantMap CalendarController::state() const
{
    QVariantList values;
    QList<Event> ordered = m_events;
    std::sort(ordered.begin(), ordered.end(), [](const Event &a, const Event &b) {
        if (a.day != b.day) return a.day < b.day;
        if (a.created != b.created) return a.created < b.created;
        return a.id < b.id;
    });
    for (const auto &event : ordered) {
        values.append(QVariantMap{{"id", event.id}, {"title", event.title}, {"details", event.details},
            {"day", dayString(event.day)}, {"created", event.created.toMSecsSinceEpoch()}});
    }
    const QString reminderStatus = m_notificationAvailable
        ? QCoreApplication::translate("HyprEndfield", "提醒：前一天及当天 09:00")
        : QCoreApplication::translate("HyprEndfield", "桌面通知暂不可用");
    return {{"events", values}, {"total", values.size()}, {"today", dayString(m_today)},
        {"error", m_error}, {"reminderStatus", reminderStatus},
        {"notificationsAvailable", m_notificationAvailable}};
}

void CalendarController::setActive(bool value)
{
    if (m_active == value) return;
    m_active = value;
    emit activeChanged();
    if (value) { m_today = QDate::currentDate(); emit stateChanged(); }
}

bool CalendarController::saveEvent(const QString &rawTitle, const QString &details,
                                   const QString &dayText, const QString &id)
{
    QString title = rawTitle.trimmed().left(120);
    const QDate day = QDate::fromString(dayText, Qt::ISODate);
    if (title.isEmpty() || !day.isValid() || day.year() < 1900 || day.year() > 9999) return false;
    QList<Event> next = m_events;
    const QDateTime timestamp = QDateTime::currentDateTime();
    auto found = std::find_if(next.begin(), next.end(), [&](const Event &event) { return event.id == id.toUpper(); });
    if (!id.isEmpty() && found == next.end()) { setError(QCoreApplication::translate("HyprEndfield", "此事项已不存在。")); return false; }
    const bool creating = found == next.end();
    if (creating) {
        if (next.size() >= maximumEvents) { setError(QCoreApplication::translate("HyprEndfield", "日历最多可保存 256 个事项。")); return false; }
        Event event;
        event.id = QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
        event.created = timestamp;
        next.append(event);
        found = std::prev(next.end());
    }
    const bool changedDay = creating || found->day != day;
    found->title = title;
    found->details = details.left(2000);
    found->day = day;
    found->modified = timestamp;
    if (changedDay) {
        found->beforeDeliveredFor.clear();
        found->dayDeliveredFor.clear();
        found->catchUpPending = day == timestamp.date() && timestamp >= atNine(day);
        found->catchUpAt = found->catchUpPending ? timestamp.addSecs(3) : QDateTime();
    }
    const qsizetype upcoming = std::count_if(next.cbegin(), next.cend(), [&](const Event &event) { return event.day >= timestamp.date(); });
    if (upcoming > maximumUpcoming) { setError(QCoreApplication::translate("HyprEndfield", "最多保留 30 个待办日期事项，以确保每条提醒都能安排。")); return false; }
    if (!persist(next)) return false;
    emit eventOccurred(creating ? "created" : "edited");
    return true;
}

bool CalendarController::removeEvent(const QString &id)
{
    QList<Event> next = m_events;
    const auto size = next.size();
    next.removeIf([&](const Event &event) { return event.id == id.toUpper(); });
    if (next.size() == size || !persist(next)) return false;
    emit eventOccurred("deleted");
    return true;
}

bool CalendarController::refreshReminders()
{
    m_notificationAvailable = m_notifications && QDBusInterface(QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"), QStringLiteral("org.freedesktop.Notifications"),
        QDBusConnection::sessionBus()).isValid();
    scheduleNext();
    emit stateChanged();
    return m_notificationAvailable;
}

QList<CalendarController::Reminder> CalendarController::reminders(const QDateTime &now) const
{
    QList<Reminder> result;
    for (int index = 0; index < m_events.size(); ++index) {
        const auto &event = m_events[index];
        const QString key = dayString(event.day);
        const QDateTime onDate = atNine(event.day);
        const QDateTime prior = onDate.addDays(-1);
        if (event.beforeDeliveredFor != key && prior > now)
            result.append({index, prior, true, false});
        if (event.dayDeliveredFor != key && onDate > now)
            result.append({index, onDate, false, false});
        if (event.catchUpPending && event.day == now.date())
            result.append({index, event.catchUpAt.isValid() ? event.catchUpAt : now.addSecs(1), false, true});
    }
    std::sort(result.begin(), result.end(), [](const Reminder &a, const Reminder &b) { return a.at < b.at; });
    return result;
}

void CalendarController::scheduleNext()
{
    m_reminderTimer.stop();
    if (!m_notificationAvailable) return;
    const auto plan = reminders(QDateTime::currentDateTime());
    if (plan.isEmpty()) return;
    const qint64 remaining = QDateTime::currentDateTime().msecsTo(plan.first().at);
    const qint64 maximum = std::numeric_limits<int>::max() - 1;
    m_reminderTimer.start(int(std::clamp<qint64>(remaining, 1, maximum)));
}

void CalendarController::deliverDue()
{
    const QDateTime now = QDateTime::currentDateTime();
    QList<Event> next = m_events;
    bool changed = false;
    for (auto &event : next) {
        const QString key = dayString(event.day);
        const QDateTime onDate = atNine(event.day), prior = onDate.addDays(-1);
        if (event.beforeDeliveredFor != key && prior <= now && prior > now.addSecs(-300)) {
            notify(event, true); event.beforeDeliveredFor = key; changed = true;
        }
        if (event.dayDeliveredFor != key && onDate <= now && onDate > now.addSecs(-300)) {
            notify(event, false); event.dayDeliveredFor = key; changed = true;
        }
        if (event.catchUpPending && event.day == now.date() && (!event.catchUpAt.isValid() || event.catchUpAt <= now)) {
            notify(event, false); event.dayDeliveredFor = key;
            event.catchUpPending = false; event.catchUpAt = {}; changed = true;
        }
    }
    if (changed) persist(next); else scheduleNext();
}

void CalendarController::notify(const Event &event, bool previousDay)
{
    if (!m_notificationAvailable) return;
    QDBusInterface interface(QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"), QDBusConnection::sessionBus());
    interface.asyncCall(QStringLiteral("Notify"), QStringLiteral("HyprEndfield"), uint(0),
        QStringLiteral("calendar"), event.title,
        (previousDay ? QCoreApplication::translate("HyprEndfield", "明天 ") : QCoreApplication::translate("HyprEndfield", "今天 ")) + dayString(event.day),
        QStringList(), QVariantMap{{"urgency", uchar(1)}}, 10'000);
}
