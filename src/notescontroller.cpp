#include <QCoreApplication>
#include "notescontroller.hpp"
#include "richtext.hpp"
#include <QMimeDatabase>
#include <QtConcurrentRun>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr qsizetype maximumFileBytes = 64 * 1024 * 1024;
constexpr qsizetype maximumImageBytes = 128 * 1024 * 1024;
constexpr qsizetype maximumNotes = 256;
constexpr qsizetype maximumTasks = 256;
constexpr qsizetype maximumText = 100'000;
constexpr qsizetype maximumStrokes = 2'000;
constexpr qsizetype maximumPointsPerStroke = 4'096;
constexpr qsizetype maximumPoints = 100'000;
constexpr qint64 maximumPixels = 100'000'000;
constexpr double workspaceWidth = 1920;
constexpr double workspaceHeight = 1080;

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
}

bool validId(const QString &id)
{
    return !id.isEmpty() && !QUuid(id).isNull();
}

QByteArray readBytes(const QString &path, bool *exists = nullptr, bool *ok = nullptr)
{
    QFile file(path);
    if (exists) *exists = file.exists();
    if (ok) *ok = true;
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return {};
    }
    return file.read(maximumFileBytes + 1);
}

QJsonObject styleObject(const NotesController::Style &style)
{
    return {{QStringLiteral("family"), style.family}, {QStringLiteral("size"), style.size},
            {QStringLiteral("color"), style.color}, {QStringLiteral("bold"), style.bold},
            {QStringLiteral("italic"), style.italic}, {QStringLiteral("underline"), style.underline},
            {QStringLiteral("strikeout"), style.strikeout}};
}

QJsonObject taskObject(const NotesController::Task &task)
{
    return {{QStringLiteral("id"), task.id}, {QStringLiteral("text"), task.text},
            {QStringLiteral("checked"), task.checked}, {QStringLiteral("flagged"), task.flagged}};
}

QJsonArray taskArray(const QList<NotesController::Task> &tasks)
{
    QJsonArray values;
    for (const auto &task : tasks) values.append(taskObject(task));
    return values;
}

bool parseStyle(const QJsonObject &object, NotesController::Style &style)
{
    style.family = object.value(QStringLiteral("family")).toString(QStringLiteral("HarmonyOS Sans"));
    style.size = object.value(QStringLiteral("size")).toDouble(14);
    style.color = object.value(QStringLiteral("color")).toString(QStringLiteral("#EEEEEE")).toUpper();
    style.bold = object.value(QStringLiteral("bold")).toBool();
    style.italic = object.value(QStringLiteral("italic")).toBool();
    style.underline = object.value(QStringLiteral("underline")).toBool();
    style.strikeout = object.value(QStringLiteral("strikeout")).toBool();
    return !style.family.isEmpty() && style.family.size() <= 128
        && std::isfinite(style.size) && style.size >= 6 && style.size <= 144
        && NotesController::validColor(style.color);
}

bool parseTasks(const QJsonArray &array, QList<NotesController::Task> &tasks, qsizetype textLimit)
{
    if (array.size() > maximumTasks) return false;
    QSet<QString> ids;
    for (const auto &value : array) {
        if (!value.isObject()) return false;
        const auto object = value.toObject();
        NotesController::Task task;
        task.id = object.value(QStringLiteral("id")).toString().toUpper();
        task.text = object.value(QStringLiteral("text")).toString();
        task.checked = object.value(QStringLiteral("checked")).toBool();
        task.flagged = object.value(QStringLiteral("flagged")).toBool();
        if (!validId(task.id) || ids.contains(task.id) || task.text.size() > textLimit) return false;
        ids.insert(task.id);
        tasks.append(std::move(task));
    }
    return true;
}

double segmentDistance(const QPointF &point, const QPointF &a, const QPointF &b)
{
    const QPointF delta = b - a;
    const double length = QPointF::dotProduct(delta, delta);
    const double amount = length <= 0 ? 0
        : std::clamp(QPointF::dotProduct(point - a, delta) / length, 0.0, 1.0);
    const QPointF nearest = a + delta * amount;
    return std::hypot(point.x() - nearest.x(), point.y() - nearest.y());
}
}

NotesController::NotesController(QObject *parent, QString directory)
    : QObject(parent)
{
    if (directory.isEmpty())
        directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/HyprEndfield/Notes");
    m_directory = std::move(directory);
    m_imagesDirectory = m_directory + QStringLiteral("/Images");
    m_path = m_directory + QStringLiteral("/notes.json");
    load();
}
NotesController::~NotesController() {
    m_cancelImport = true;
    m_mediaImport.waitForFinished();
    // An import whose completion callback never ran has no persisted note.
    if (!m_pendingMediaPath.isEmpty()) QFile::remove(m_pendingMediaPath);
}

bool NotesController::validColor(const QString &color)
{
    static const QRegularExpression expression(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return expression.match(color).hasMatch();
}

void NotesController::load()
{
    bool exists = false;
    bool readable = true;
    const QByteArray bytes = readBytes(m_path, &exists, &readable);
    if (!exists) {
        m_persisted.clear();
        return;
    }
    auto reject = [this] {
        m_available = false;
        m_error = QCoreApplication::translate("HyprEndfield", "无法读取已保存的便笺，原文件已保留。");
    };
    if (!readable || bytes.size() > maximumFileBytes) {
        reject();
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        reject();
        return;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1
        || !root.value(QStringLiteral("centre")).isObject()
        || !root.value(QStringLiteral("notes")).isArray()) {
        reject();
        return;
    }

    Centre centre;
    const auto centreObject = root.value(QStringLiteral("centre")).toObject();
    centre.tab = centreObject.value(QStringLiteral("tab")).toString(QStringLiteral("todo"));
    centre.folded = centreObject.value(QStringLiteral("folded")).toBool();
    centre.text = centreObject.value(QStringLiteral("text")).toString();
    if ((centre.tab != QStringLiteral("text") && centre.tab != QStringLiteral("todo"))
        || centre.text.size() > maximumText
        || !parseStyle(centreObject.value(QStringLiteral("style")).toObject(), centre.style)
        || !parseTasks(centreObject.value(QStringLiteral("tasks")).toArray(), centre.tasks, 60)) {
        reject();
        return;
    }

    const QJsonArray noteArray = root.value(QStringLiteral("notes")).toArray();
    if (noteArray.size() > maximumNotes) {
        reject();
        return;
    }
    QList<Note> notes;
    QSet<QString> ids;
    for (const auto &value : noteArray) {
        if (!value.isObject()) {
            reject();
            return;
        }
        const auto object = value.toObject();
        Note note;
        note.id = object.value(QStringLiteral("id")).toString().toUpper();
        note.kind = object.value(QStringLiteral("kind")).toString();
        note.text = object.value(QStringLiteral("text")).toString();
        note.imageName = object.value(QStringLiteral("imageName")).toString();
        note.mediaName = object.value("mediaName").toString();
        note.richText = object.value("richText").toString();
        note.x = object.value(QStringLiteral("x")).toDouble(std::numeric_limits<double>::quiet_NaN());
        note.y = object.value(QStringLiteral("y")).toDouble(std::numeric_limits<double>::quiet_NaN());
        note.width = object.value(QStringLiteral("width")).toDouble(std::numeric_limits<double>::quiet_NaN());
        note.height = object.value(QStringLiteral("height")).toDouble(std::numeric_limits<double>::quiet_NaN());
        note.z = object.value(QStringLiteral("z")).toInt();
        note.pinned = object.value(QStringLiteral("pinned")).toBool();
        note.created = QDateTime::fromString(object.value(QStringLiteral("created")).toString(), Qt::ISODateWithMs);
        const bool kindValid = note.kind == QStringLiteral("text") || note.kind == QStringLiteral("todo")
            || note.kind == QStringLiteral("image") || note.kind == QStringLiteral("drawing") || note.kind == "video" || note.kind == "gif";
        static const QRegularExpression imageName(QStringLiteral("^[0-9A-Fa-f-]{36}\\.png$"));
        static const QRegularExpression mediaName(QStringLiteral("^[0-9A-Fa-f-]{36}\\.(mp4|mkv|webm|mov|avi|m4v|ogv|gif)$"));
        if (!validId(note.id) || ids.contains(note.id) || !kindValid || note.text.size() > maximumText
            || note.richText.size() > 2'000'000
            || (note.kind == "video" || note.kind == "gif" ? !mediaName.match(note.mediaName).hasMatch() : !note.mediaName.isEmpty())
            || !note.created.isValid() || !std::isfinite(note.x) || !std::isfinite(note.y)
            || !std::isfinite(note.width) || !std::isfinite(note.height)
            || (note.kind == QStringLiteral("image") && !imageName.match(note.imageName).hasMatch())
            || (note.kind != QStringLiteral("image") && !note.imageName.isEmpty())
            || !parseTasks(object.value(QStringLiteral("tasks")).toArray(), note.tasks, 1'000)) {
            reject();
            return;
        }
        qsizetype pointCount = 0;
        const QJsonArray strokes = object.value(QStringLiteral("strokes")).toArray();
        if (strokes.size() > maximumStrokes) {
            reject();
            return;
        }
        for (const auto &strokeValue : strokes) {
            const auto strokeObject = strokeValue.toObject();
            Stroke stroke;
            stroke.width = strokeObject.value(QStringLiteral("width")).toDouble();
            stroke.color = strokeObject.value(QStringLiteral("color")).toString().toUpper();
            const QJsonArray points = strokeObject.value(QStringLiteral("points")).toArray();
            if (points.isEmpty() || points.size() > maximumPointsPerStroke
                || !std::isfinite(stroke.width) || stroke.width < 1 || stroke.width > 80
                || !validColor(stroke.color)) {
                reject();
                return;
            }
            for (const auto &pointValue : points) {
                const QJsonArray pair = pointValue.toArray();
                const double x = pair.size() == 2 ? pair[0].toDouble(-1) : -1;
                const double y = pair.size() == 2 ? pair[1].toDouble(-1) : -1;
                if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 || y > 1) {
                    reject();
                    return;
                }
                stroke.points.append({x, y});
            }
            pointCount += stroke.points.size();
            if (pointCount > maximumPoints) {
                reject();
                return;
            }
            note.strokes.append(std::move(stroke));
        }
        if (note.kind != QStringLiteral("drawing") && !note.strokes.isEmpty()) {
            reject();
            return;
        }
        constrain(note);
        ids.insert(note.id);
        notes.append(std::move(note));
    }
    m_centre = std::move(centre);
    m_notes = std::move(notes);
    m_persisted = bytes;
}

QJsonObject noteObject(const NotesController::Note &note)
{
    QJsonArray strokes;
    for (const auto &stroke : note.strokes) {
        QJsonArray points;
        for (const auto &point : stroke.points) points.append(QJsonArray{point.x(), point.y()});
        strokes.append(QJsonObject{{QStringLiteral("width"), stroke.width},
                                   {QStringLiteral("color"), stroke.color},
                                   {QStringLiteral("points"), points}});
    }
    return {{QStringLiteral("id"), note.id}, {QStringLiteral("kind"), note.kind},
            {QStringLiteral("mediaName"), note.mediaName}, {QStringLiteral("richText"), note.richText},
            {QStringLiteral("text"), note.text}, {QStringLiteral("tasks"), taskArray(note.tasks)},
            {QStringLiteral("imageName"), note.imageName}, {QStringLiteral("x"), note.x},
            {QStringLiteral("y"), note.y}, {QStringLiteral("width"), note.width},
            {QStringLiteral("height"), note.height}, {QStringLiteral("z"), note.z},
            {QStringLiteral("pinned"), note.pinned},
            {QStringLiteral("created"), note.created.toString(Qt::ISODateWithMs)},
            {QStringLiteral("strokes"), strokes}};
}

bool NotesController::persist(const Centre &centre, const QList<Note> &notes)
{
    if (!m_available) {
        fail(QCoreApplication::translate("HyprEndfield", "便笺存储不可用。"));
        return false;
    }
    bool exists = false;
    bool readable = true;
    const QByteArray current = readBytes(m_path, &exists, &readable);
    if (!readable) {
        fail(QCoreApplication::translate("HyprEndfield", "无法读取便笺数据，原文件已保留。"));
        return false;
    }
    if ((exists ? current : QByteArray()) != m_persisted) {
        fail(QCoreApplication::translate("HyprEndfield", "便笺文件已被其他程序修改，请重启后再保存。"));
        return false;
    }
    QJsonArray noteValues;
    for (const auto &note : notes) noteValues.append(noteObject(note));
    const QJsonObject centreValue{{QStringLiteral("tab"), centre.tab},
                                  {QStringLiteral("folded"), centre.folded},
                                  {QStringLiteral("text"), centre.text},
                                  {QStringLiteral("style"), styleObject(centre.style)},
                                  {QStringLiteral("tasks"), taskArray(centre.tasks)}};
    const QByteArray bytes = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
        {QStringLiteral("centre"), centreValue}, {QStringLiteral("notes"), noteValues}})
        .toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumFileBytes || !QDir().mkpath(m_directory)) {
        fail(QCoreApplication::translate("HyprEndfield", "无法保存便笺数据。"));
        return false;
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        fail(QCoreApplication::translate("HyprEndfield", "无法保存便笺数据。"));
        return false;
    }
    m_centre = centre;
    m_notes = notes;
    m_persisted = bytes;
    m_error.clear();
    emit stateChanged();
    return true;
}

void NotesController::fail(const QString &message, const QString &action)
{
    m_error = message;
    emit stateChanged();
    if (!action.isEmpty()) emit actionFinished(action, false, message);
}

QVariantMap NotesController::taskState(const Task &task)
{
    return {{QStringLiteral("id"), task.id}, {QStringLiteral("text"), task.text},
            {QStringLiteral("checked"), task.checked}, {QStringLiteral("flagged"), task.flagged}};
}

QVariantMap NotesController::state() const
{
    QVariantList centreTasks;
    int done = 0;
    bool hasDot = false;
    for (const auto &task : m_centre.tasks) {
        centreTasks.append(taskState(task));
        done += task.checked ? 1 : 0;
        hasDot = hasDot || (task.flagged && !task.checked);
    }
    QVariantMap style{{QStringLiteral("family"), m_centre.style.family},
                      {QStringLiteral("size"), m_centre.style.size},
                      {QStringLiteral("color"), m_centre.style.color},
                      {QStringLiteral("bold"), m_centre.style.bold},
                      {QStringLiteral("italic"), m_centre.style.italic},
                      {QStringLiteral("underline"), m_centre.style.underline},
                      {QStringLiteral("strikeout"), m_centre.style.strikeout}};
    QVariantMap centre{{QStringLiteral("tab"), m_centre.tab},
                       {QStringLiteral("folded"), m_centre.folded},
                       {QStringLiteral("text"), m_centre.text},
                       {QStringLiteral("style"), style},
                       {QStringLiteral("tasks"), centreTasks},
                       {QStringLiteral("done"), done},
                       {QStringLiteral("hasDot"), hasDot}};
    QVariantList notes;
    QList<Note> ordered = m_notes;
    std::sort(ordered.begin(), ordered.end(), [](const Note &left, const Note &right) {
        if (left.z != right.z) return left.z < right.z;
        return left.created < right.created;
    });
    for (const auto &note : ordered) {
        QVariantList tasks;
        for (const auto &task : note.tasks) tasks.append(taskState(task));
        QVariantList strokes;
        for (const auto &stroke : note.strokes) {
            QVariantList points;
            for (const auto &point : stroke.points)
                points.append(QVariantMap{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}});
            strokes.append(QVariantMap{{QStringLiteral("points"), points},
                                       {QStringLiteral("width"), stroke.width},
                                       {QStringLiteral("color"), stroke.color}});
        }
        const QString imagePath = note.imageName.isEmpty() ? QString() : m_imagesDirectory + QLatin1Char('/') + note.imageName;
        notes.append(QVariantMap{{QStringLiteral("id"), note.id}, {QStringLiteral("kind"), note.kind},
            {QStringLiteral("richText"), note.richText},
            {QStringLiteral("mediaUrl"), note.mediaName.isEmpty() ? QString() : QUrl::fromLocalFile(m_directory + "/Media/" + note.mediaName).toString()},
            {QStringLiteral("text"), note.text}, {QStringLiteral("tasks"), tasks},
            {QStringLiteral("imageUrl"), imagePath.isEmpty() ? QString() : QUrl::fromLocalFile(imagePath).toString()},
            {QStringLiteral("imageAvailable"), imagePath.isEmpty() ? false : QFileInfo::exists(imagePath)},
            {QStringLiteral("x"), note.x}, {QStringLiteral("y"), note.y},
            {QStringLiteral("width"), note.width}, {QStringLiteral("height"), note.height},
            {QStringLiteral("z"), note.z}, {QStringLiteral("pinned"), note.pinned},
            {QStringLiteral("createdAt"), note.created.toMSecsSinceEpoch()},
            {QStringLiteral("strokes"), strokes}});
    }
    return {{QStringLiteral("ready"), m_available}, {QStringLiteral("error"), m_error}, {QStringLiteral("importing"), m_mediaImport.isRunning()},
            {QStringLiteral("centre"), centre}, {QStringLiteral("notes"), notes},
            {QStringLiteral("total"), notes.size()}};
}

int NotesController::noteIndex(const QList<Note> &notes, const QString &id) const
{
    const QString normalized = id.toUpper();
    for (int index = 0; index < notes.size(); ++index)
        if (notes[index].id == normalized) return index;
    return -1;
}

int NotesController::nextZ(const QList<Note> &notes) const
{
    int value = 0;
    for (const auto &note : notes) value = std::max(value, note.z);
    return value == std::numeric_limits<int>::max() ? value : value + 1;
}

void NotesController::constrain(Note &note)
{
    const double minimumWidth = note.kind == QStringLiteral("todo") ? 180 : 110;
    const double minimumHeight = note.kind == QStringLiteral("todo") ? 105 : 70;
    note.width = std::clamp(std::isfinite(note.width) ? note.width : 210.0, minimumWidth, workspaceWidth);
    note.height = std::clamp(std::isfinite(note.height) ? note.height : 140.0, minimumHeight, workspaceHeight);
    note.x = std::clamp(std::isfinite(note.x) ? note.x : 24.0, 0.0, workspaceWidth - note.width);
    note.y = std::clamp(std::isfinite(note.y) ? note.y : 50.0, 0.0, workspaceHeight - note.height);
}

bool NotesController::setCentreTab(const QString &tab)
{
    if (tab != QStringLiteral("text") && tab != QStringLiteral("todo")) return false;
    Centre next = m_centre;
    next.tab = tab;
    return persist(next, m_notes);
}

bool NotesController::setCentreFolded(bool folded)
{
    Centre next = m_centre;
    next.folded = folded;
    return persist(next, m_notes);
}

bool NotesController::setCentreText(const QString &text)
{
    Centre next = m_centre;
    next.text = text.left(maximumText);
    return persist(next, m_notes);
}

bool NotesController::setCentreStyle(const QString &key, const QVariant &value)
{
    Centre next = m_centre;
    if (key == QStringLiteral("family")) {
        const QString family = value.toString().left(128);
        if (family.isEmpty()) return false;
        next.style.family = family;
    } else if (key == QStringLiteral("size")) {
        const double size = value.toDouble();
        if (!std::isfinite(size) || size < 6 || size > 144) return false;
        next.style.size = size;
    } else if (key == QStringLiteral("color")) {
        const QString color = value.toString().toUpper();
        if (!validColor(color)) return false;
        next.style.color = color;
    } else if (key == QStringLiteral("bold")) next.style.bold = value.toBool();
    else if (key == QStringLiteral("italic")) next.style.italic = value.toBool();
    else if (key == QStringLiteral("underline")) next.style.underline = value.toBool();
    else if (key == QStringLiteral("strikeout")) next.style.strikeout = value.toBool();
    else return false;
    return persist(next, m_notes);
}

bool NotesController::addCentreTask(const QString &rawText)
{
    const QString text = rawText.trimmed().left(60);
    if (text.isEmpty() || m_centre.tasks.size() >= maximumTasks) return false;
    Centre next = m_centre;
    next.tasks.append(Task{newId(), text});
    return persist(next, m_notes);
}

bool NotesController::updateCentreTask(const QString &id, const QString &rawText)
{
    const QString text = rawText.trimmed().left(60);
    if (text.isEmpty()) return false;
    Centre next = m_centre;
    auto found = std::find_if(next.tasks.begin(), next.tasks.end(), [&](const Task &task) { return task.id == id.toUpper(); });
    if (found == next.tasks.end()) return false;
    found->text = text;
    return persist(next, m_notes);
}

bool NotesController::toggleCentreTask(const QString &id)
{
    Centre next = m_centre;
    auto found = std::find_if(next.tasks.begin(), next.tasks.end(), [&](const Task &task) { return task.id == id.toUpper(); });
    if (found == next.tasks.end()) return false;
    found->checked = !found->checked;
    return persist(next, m_notes);
}

bool NotesController::toggleCentreFlag(const QString &id)
{
    Centre next = m_centre;
    auto found = std::find_if(next.tasks.begin(), next.tasks.end(), [&](const Task &task) { return task.id == id.toUpper(); });
    if (found == next.tasks.end()) return false;
    found->flagged = !found->flagged;
    return persist(next, m_notes);
}

bool NotesController::moveCentreTask(const QString &id, int offset)
{
    Centre next = m_centre;
    const auto found = std::find_if(next.tasks.begin(), next.tasks.end(), [&](const Task &task) { return task.id == id.toUpper(); });
    if (found == next.tasks.end()) return false;
    const int index = int(std::distance(next.tasks.begin(), found));
    const int destination = std::clamp(index + offset, 0, int(next.tasks.size()) - 1);
    if (index == destination) return false;
    next.tasks.move(index, destination);
    return persist(next, m_notes);
}

bool NotesController::removeCentreTask(const QString &id)
{
    Centre next = m_centre;
    const qsizetype removed = next.tasks.removeIf([&](const Task &task) { return task.id == id.toUpper(); });
    return removed > 0 && persist(next, m_notes);
}

QString NotesController::createNote(const QString &kind, double x, double y)
{
    if (kind != QStringLiteral("text") && kind != QStringLiteral("todo") && kind != QStringLiteral("drawing")) return {};
    if (m_notes.size() >= maximumNotes) {
        fail(QCoreApplication::translate("HyprEndfield", "便笺最多可保存 256 张。"), QStringLiteral("create"));
        return {};
    }
    QList<Note> next = m_notes;
    Note note;
    note.id = newId();
    note.kind = kind;
    note.x = x;
    note.y = y;
    note.width = kind == QStringLiteral("drawing") ? 300 : kind == QStringLiteral("todo") ? 228 : 210;
    note.height = kind == QStringLiteral("drawing") ? 240 : kind == QStringLiteral("todo") ? 154 : 140;
    note.z = nextZ(next);
    note.created = QDateTime::currentDateTime();
    if (kind == QStringLiteral("todo")) note.tasks.append(Task{newId(), QString()});
    constrain(note);
    next.append(note);
    if (!persist(m_centre, next)) return {};
    emit actionFinished(QStringLiteral("create"), true,
                        kind == QStringLiteral("drawing") ? QCoreApplication::translate("HyprEndfield", "已创建画画便笺") : QCoreApplication::translate("HyprEndfield", "已创建便笺"));
    return note.id;
}

QString NotesController::importImage(const QUrl &url, double x, double y)
{
    if (m_notes.size() >= maximumNotes || !url.isLocalFile()) {
        fail(QCoreApplication::translate("HyprEndfield", "无法将这个文件作为图片打开。"), QStringLiteral("import"));
        return {};
    }
    const QString sourcePath = url.toLocalFile();
    const QFileInfo info(sourcePath);
    if (!info.isFile() || !info.isReadable() || info.size() > maximumImageBytes) {
        fail(info.size() > maximumImageBytes ? QCoreApplication::translate("HyprEndfield", "请选择小于 128 MB 的图片。")
                                             : QCoreApplication::translate("HyprEndfield", "无法将这个文件作为图片打开。"),
             QStringLiteral("import"));
        return {};
    }
    QImageReader reader(sourcePath);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (!sourceSize.isValid() || qint64(sourceSize.width()) * sourceSize.height() > maximumPixels) {
        fail(QCoreApplication::translate("HyprEndfield", "图片尺寸过大。"), QStringLiteral("import"));
        return {};
    }
    if (std::max(sourceSize.width(), sourceSize.height()) > 1600)
        reader.setScaledSize(sourceSize.scaled(1600, 1600, Qt::KeepAspectRatio));
    const QImage image = reader.read();
    if (image.isNull()) {
        fail(QCoreApplication::translate("HyprEndfield", "无法将这个文件作为图片打开。"), QStringLiteral("import"));
        return {};
    }
    if (!QDir().mkpath(m_imagesDirectory)) {
        fail(QCoreApplication::translate("HyprEndfield", "无法保存图片便笺。"), QStringLiteral("import"));
        return {};
    }
    const QString imageName = newId().toLower() + QStringLiteral(".png");
    const QString destination = m_imagesDirectory + QLatin1Char('/') + imageName;
    QSaveFile output(destination);
    if (!output.open(QIODevice::WriteOnly) || !image.save(&output, "PNG") || !output.commit()) {
        fail(QCoreApplication::translate("HyprEndfield", "无法保存图片便笺。"), QStringLiteral("import"));
        return {};
    }
    QList<Note> next = m_notes;
    Note note;
    note.id = newId();
    note.kind = QStringLiteral("image");
    note.imageName = imageName;
    note.x = x;
    note.y = y;
    const double ratio = double(image.height()) / std::max(1, image.width());
    note.width = std::min(210.0, 160.0 / std::max(0.1, ratio));
    note.height = note.width * ratio + 24;
    note.z = nextZ(next);
    note.created = QDateTime::currentDateTime();
    constrain(note);
    next.append(note);
    if (!persist(m_centre, next)) {
        QFile::remove(destination);
        return {};
    }
    emit actionFinished(QStringLiteral("import"), true, QCoreApplication::translate("HyprEndfield", "已创建图片便笺"));
    return note.id;
}

void NotesController::importMedia(const QUrl &url, double x, double y) {
    if (!m_pendingMediaPath.isEmpty()) { fail(tr("正在导入媒体，请稍候。")); return; }
    if (!url.isLocalFile() || !QFileInfo(url.toLocalFile()).isFile()) { fail(tr("只支持本机图片和视频。")); return; }
    const auto mime = QMimeDatabase().mimeTypeForFile(url.toLocalFile());
    const bool gif = mime.name() == "image/gif";
    if (mime.name().startsWith("image/") && !gif) { importImage(url, x, y); return; }
    if (gif) {
        QImageReader reader(url.toLocalFile());
        const auto size = reader.size();
        if (!reader.canRead() || !size.isValid() || qint64(size.width()) * size.height() > maximumPixels
            || reader.imageCount() > 2000 || QFileInfo(url.toLocalFile()).size() > maximumImageBytes) {
            fail(tr("无法将这个文件作为图片打开。")); return;
        }
    }
    const QString extension = gif ? QString("gif") : QFileInfo(url.toLocalFile()).suffix().toLower();
    if ((!gif && (!mime.name().startsWith("video/") || !QStringList{"mp4", "mkv", "webm", "mov", "avi", "m4v", "ogv"}.contains(extension)))
        || m_notes.size() >= maximumNotes) { fail(tr("只支持图片、GIF 和视频。")); return; }
    const QString name = newId().toLower() + '.' + extension, directory = m_directory + "/Media", destination = directory + '/' + name;
    if (!QDir().mkpath(directory)) { fail(tr("无法保存媒体便笺。")); return; }
    m_pendingMediaPath = destination;
    m_mediaImport.disconnect(this);
    connect(&m_mediaImport, &QFutureWatcherBase::finished, this, [this, name, destination, x, y, gif] {
        const auto error = m_mediaImport.result();
        m_pendingMediaPath.clear();
        if (!error.isEmpty()) { fail(error); emit stateChanged(); return; }
        if (m_notes.size() >= maximumNotes) { QFile::remove(destination); fail(tr("便笺最多可保存 256 张。")); return; }
        auto next = m_notes; Note note; note.id = newId(); note.kind = gif ? "gif" : "video"; note.mediaName = name;
        note.x = x; note.y = y; note.width = 320; note.height = 232; note.z = nextZ(next); note.created = QDateTime::currentDateTime(); constrain(note);
        next.append(note);
        if (!persist(m_centre, next)) QFile::remove(destination);
        else emit actionFinished("import", true, gif ? tr("已创建图片便笺") : tr("已创建视频便笺"));
        emit stateChanged();
    });
    m_cancelImport = false;
    m_mediaImport.setFuture(QtConcurrent::run([this, source = url.toLocalFile(), destination] {
        QFile input(source); QSaveFile output(destination);
        if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) return tr("无法保存媒体便笺。");
        while (!input.atEnd()) {
            if (m_cancelImport) return QString();
            const auto bytes = input.read(1024 * 1024);
            if (input.error() != QFile::NoError || output.write(bytes) != bytes.size()) return tr("无法保存媒体便笺。");
        }
        return m_cancelImport || output.commit() ? QString() : tr("无法保存媒体便笺。");
    }));
    emit stateChanged();
}

QVariantMap NotesController::formatState(QObject *document, int position) const { return richTextStyle(document, position); }
bool NotesController::applyFormat(QObject *document, int start, int end, const QString &key, const QVariant &value) {
    return formatRichText(document, start, end, key, value);
}
bool NotesController::updateNoteRichText(const QString &id, const QString &html) {
    if (html.size() > 2'000'000) return false;
    auto next = m_notes; const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != "text") return false;
    QTextDocument document; document.setHtml(html);
    if (document.toPlainText().size() > maximumText) return false;
    next[index].text = document.toPlainText(); next[index].richText = document.toHtml();
    return persist(m_centre, next);
}

bool NotesController::updateNoteGeometry(const QString &id, double x, double y, double width, double height)
{
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0) return false;
    next[index].x = x;
    next[index].y = y;
    next[index].width = width;
    next[index].height = height;
    next[index].z = nextZ(next);
    constrain(next[index]);
    return persist(m_centre, next);
}

bool NotesController::updateNoteText(const QString &id, const QString &text)
{
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("text")) return false;
    next[index].text = text.left(maximumText);
    next[index].richText.clear();
    return persist(m_centre, next);
}

bool NotesController::toggleNotePin(const QString &id)
{
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0) return false;
    next[index].pinned = !next[index].pinned;
    next[index].z = nextZ(next);
    return persist(m_centre, next);
}

bool NotesController::removeNote(const QString &id)
{
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0) return false;
    const QString imageName = next[index].imageName;
    const QString mediaName = next[index].mediaName;
    next.removeAt(index);
    if (!persist(m_centre, next)) return false;
    if (!imageName.isEmpty()) QFile::remove(m_imagesDirectory + QLatin1Char('/') + imageName);
    if (!mediaName.isEmpty()) QFile::remove(m_directory + "/Media/" + mediaName);
    emit actionFinished(QStringLiteral("delete"), true, QCoreApplication::translate("HyprEndfield", "便笺已删除"));
    return true;
}

bool NotesController::addNoteTask(const QString &id, const QString &rawText)
{
    const QString text = rawText.trimmed().left(1'000);
    if (text.isEmpty()) return false;
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("todo") || next[index].tasks.size() >= maximumTasks) return false;
    if (next[index].tasks.size() == 1 && next[index].tasks[0].text.isEmpty()) next[index].tasks[0].text = text;
    else next[index].tasks.append(Task{newId(), text});
    return persist(m_centre, next);
}

bool NotesController::updateNoteTask(const QString &id, const QString &taskId, const QString &rawText)
{
    const QString text = rawText.trimmed().left(1'000);
    if (text.isEmpty()) return false;
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("todo")) return false;
    auto found = std::find_if(next[index].tasks.begin(), next[index].tasks.end(), [&](const Task &task) { return task.id == taskId.toUpper(); });
    if (found == next[index].tasks.end()) return false;
    found->text = text;
    return persist(m_centre, next);
}

bool NotesController::toggleNoteTask(const QString &id, const QString &taskId)
{
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("todo")) return false;
    auto found = std::find_if(next[index].tasks.begin(), next[index].tasks.end(), [&](const Task &task) { return task.id == taskId.toUpper(); });
    if (found == next[index].tasks.end()) return false;
    found->checked = !found->checked;
    return persist(m_centre, next);
}

bool NotesController::moveNoteTask(const QString &id, const QString &taskId, int offset)
{
    QList<Note> next = m_notes;
    const int noteAt = noteIndex(next, id);
    if (noteAt < 0 || next[noteAt].kind != QStringLiteral("todo")) return false;
    auto &tasks = next[noteAt].tasks;
    const auto found = std::find_if(tasks.begin(), tasks.end(), [&](const Task &task) { return task.id == taskId.toUpper(); });
    if (found == tasks.end()) return false;
    const int index = int(std::distance(tasks.begin(), found));
    const int destination = std::clamp(index + offset, 0, int(tasks.size()) - 1);
    if (index == destination) return false;
    tasks.move(index, destination);
    return persist(m_centre, next);
}

bool NotesController::removeNoteTask(const QString &id, const QString &taskId)
{
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("todo")) return false;
    const qsizetype removed = next[index].tasks.removeIf([&](const Task &task) { return task.id == taskId.toUpper(); });
    return removed > 0 && persist(m_centre, next);
}

bool NotesController::appendStroke(const QString &id, const QVariantList &values,
                                   double width, const QString &rawColor)
{
    if (values.isEmpty() || values.size() > maximumPointsPerStroke || !std::isfinite(width) || width < 1 || width > 80) return false;
    const QString color = rawColor.toUpper();
    if (!validColor(color)) return false;
    Stroke stroke;
    stroke.width = width;
    stroke.color = color;
    for (const auto &value : values) {
        const QVariantMap point = value.toMap();
        const double x = point.value(QStringLiteral("x")).toDouble();
        const double y = point.value(QStringLiteral("y")).toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 || y > 1) return false;
        stroke.points.append({x, y});
    }
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("drawing") || next[index].strokes.size() >= maximumStrokes) return false;
    qsizetype total = stroke.points.size();
    for (const auto &item : next[index].strokes) total += item.points.size();
    if (total > maximumPoints) {
        fail(QCoreApplication::translate("HyprEndfield", "画画容量已满，请新建画画便笺。"), QStringLiteral("drawing"));
        return false;
    }
    next[index].strokes.append(std::move(stroke));
    return persist(m_centre, next);
}

bool NotesController::eraseDrawing(const QString &id, double x, double y, double radius)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) || radius <= 0) return false;
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("drawing")) return false;
    const QPointF point(x, y);
    const qsizetype before = next[index].strokes.size();
    next[index].strokes.removeIf([&](const Stroke &stroke) {
        if (stroke.points.size() == 1)
            return std::hypot(stroke.points[0].x() - x, stroke.points[0].y() - y) <= radius;
        for (int at = 1; at < stroke.points.size(); ++at)
            if (segmentDistance(point, stroke.points[at - 1], stroke.points[at]) <= radius) return true;
        return false;
    });
    return next[index].strokes.size() != before && persist(m_centre, next);
}

bool NotesController::eraseDrawingPath(const QString &id, const QVariantList &values, double radius)
{
    if (values.isEmpty() || values.size() > maximumPointsPerStroke
        || !std::isfinite(radius) || radius <= 0) return false;
    QList<QPointF> path;
    for (const auto &value : values) {
        const QVariantMap point = value.toMap();
        const double x = point.value(QStringLiteral("x")).toDouble();
        const double y = point.value(QStringLiteral("y")).toDouble();
        if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || x > 1 || y < 0 || y > 1) return false;
        path.append({x, y});
    }
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("drawing")) return false;
    const qsizetype before = next[index].strokes.size();
    next[index].strokes.removeIf([&](const Stroke &stroke) {
        for (const QPointF &eraser : path) {
            if (stroke.points.size() == 1) {
                if (std::hypot(stroke.points[0].x() - eraser.x(), stroke.points[0].y() - eraser.y()) <= radius) return true;
                continue;
            }
            for (int at = 1; at < stroke.points.size(); ++at)
                if (segmentDistance(eraser, stroke.points[at - 1], stroke.points[at]) <= radius) return true;
        }
        return false;
    });
    return next[index].strokes.size() != before && persist(m_centre, next);
}

bool NotesController::clearDrawing(const QString &id)
{
    QList<Note> next = m_notes;
    const int index = noteIndex(next, id);
    if (index < 0 || next[index].kind != QStringLiteral("drawing") || next[index].strokes.isEmpty()) return false;
    next[index].strokes.clear();
    return persist(m_centre, next);
}
