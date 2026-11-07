#include "fileshelfcontroller.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDrag>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QMimeDatabase>
#include <QProcess>
#include <QQuickWindow>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <QtConcurrentRun>

#include <algorithm>

#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr qsizetype maximumArchiveBytes = 4 * 1024 * 1024;
QString sizeLabel(qint64 size)
{
    if (size < 1000) return QString::number(size) + QStringLiteral(" B");
    double value = size;
    const QStringList units{QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB"), QStringLiteral("TB")};
    QString unit;
    for (const auto &candidate : units) { value /= 1000; unit = candidate; if (value < 1000) break; }
    return QString::number(value, 'f', value < 10 ? 1 : 0) + ' ' + unit;
}
QString dateLabel(const QDateTime &date)
{
    return date.isValid() ? date.toLocalTime().toString(QStringLiteral("yyyy.MM.dd HH:mm")) : QStringLiteral("—");
}
}

FileShelfController::FileShelfController(QObject *parent, QString directory) : QObject(parent)
{
    m_directory = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/HyprEndfield/FileShelf") : std::move(directory);
    m_path = m_directory + QStringLiteral("/shelf.json");
    load();
}

void FileShelfController::setWindow(QQuickWindow *window) { m_window = window; }

FileShelfController::Entry FileShelfController::inspect(const QString &path, const Entry *previous)
{
    Entry entry = previous ? *previous : Entry{};
    entry.path = QDir::cleanPath(path);
    entry.error.clear();
    struct stat identity{};
    const auto native = QFile::encodeName(entry.path);
    const QFileInfo info(entry.path);
    if (::lstat(native.constData(), &identity) != 0 || !info.exists()
        || (!S_ISREG(identity.st_mode) && !S_ISDIR(identity.st_mode) && !S_ISLNK(identity.st_mode))
        || ::access(native.constData(), R_OK) != 0) {
        entry.error = QStringLiteral("原文件已移动、删除或无法访问，请重新添加。");
        return entry;
    }
    const auto device = QString::number(qulonglong(identity.st_dev));
    const auto inode = QString::number(qulonglong(identity.st_ino));
    if (previous && (device != previous->device || inode != previous->inode)) {
        entry.error = QStringLiteral("原文件不可用，原路径已被其他文件占用。");
        return entry;
    }
    entry.device = device; entry.inode = inode;
    if (entry.id.isEmpty()) entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.name = info.fileName().isEmpty() ? entry.path : info.fileName();
    entry.directory = info.isDir(); entry.bytes = entry.directory ? 0 : info.size();
    entry.extension = entry.directory ? QStringLiteral("DIR") : info.suffix().toUpper();
    if (entry.extension.isEmpty()) entry.extension = QStringLiteral("FILE");
    entry.kind = QStringLiteral("txt");
    if (!entry.directory) {
        const QString mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
        if (mime == QStringLiteral("application/pdf")) entry.kind = QStringLiteral("pdf");
        else if (mime.startsWith(QStringLiteral("image/"))) entry.kind = QStringLiteral("img");
        else if (mime.startsWith(QStringLiteral("audio/")) || mime.startsWith(QStringLiteral("video/"))) entry.kind = QStringLiteral("vid");
        else if (QStringList{QStringLiteral("ZIP"), QStringLiteral("RAR"), QStringLiteral("7Z"), QStringLiteral("TAR"),
                            QStringLiteral("GZ"), QStringLiteral("TGZ"), QStringLiteral("BZ2"), QStringLiteral("XZ"),
                            QStringLiteral("ZST"), QStringLiteral("ISO"), QStringLiteral("DMG"), QStringLiteral("JAR"),
                            QStringLiteral("DEB"), QStringLiteral("RPM")}.contains(entry.extension)) entry.kind = QStringLiteral("zip");
    }
    return entry;
}

QVariantMap FileShelfController::describe(const Entry &entry)
{
    QString location = QFileInfo(entry.path).absolutePath();
    const auto home = QDir::homePath();
    if (location == home || location.startsWith(home + '/')) location.replace(0, home.size(), QStringLiteral("~"));
    return {{"id", entry.id}, {"path", entry.path}, {"name", entry.name}, {"kind", entry.kind},
            {"extension", entry.extension}, {"directory", entry.directory}, {"available", entry.error.isEmpty()},
            {"error", entry.error}, {"location", location},
            {"kindText", entry.directory ? QStringLiteral("文件夹") : entry.extension == QStringLiteral("FILE")
                ? QStringLiteral("文件") : entry.extension + QStringLiteral(" 文件")},
            {"size", entry.directory ? QString() : sizeLabel(entry.bytes)},
            {"sizeTag", !entry.error.isEmpty() ? QStringLiteral("不可用") : entry.directory ? QString() : sizeLabel(entry.bytes)}};
}

QVariantMap FileShelfController::state() const
{
    QVariantList items;
    for (const auto &entry : m_entries) items.append(describe(entry));
    return {{"ready", m_ready}, {"busy", m_busy}, {"error", m_error}, {"items", items}, {"total", items.size()}};
}

bool FileShelfController::fail(const QString &action, const QString &message)
{
    m_error = message;
    emit stateChanged();
    emit actionFinished(action, false, message);
    return false;
}

void FileShelfController::load()
{
    QFile file(m_path);
    m_fileExisted = file.exists();
    if (!m_fileExisted) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumArchiveBytes) {
        m_ready = false; m_error = QStringLiteral("无法读取暂存架数据，原始数据已保留。"); return;
    }
    m_persisted = file.readAll();
    const auto document = QJsonDocument::fromJson(m_persisted);
    const auto root = document.object();
    const auto invalid = [this] {
        m_ready = false; m_entries.clear(); m_error = QStringLiteral("无法读取暂存架数据，原始数据已保留。");
    };
    if (!document.isObject() || root.value("version").toInt() != 1 || !root.value("items").isArray()) { invalid(); return; }
    QSet<QString> ids;
    for (const auto &value : root.value("items").toArray()) {
        const auto object = value.toObject();
        Entry entry;
        entry.id = object.value("id").toString(); entry.path = object.value("path").toString();
        entry.name = object.value("name").toString(); entry.kind = object.value("kind").toString();
        entry.extension = object.value("extension").toString(); entry.directory = object.value("directory").toBool();
        entry.bytes = object.value("bytes").toInteger(-1);
        entry.device = object.value("device").toString(); entry.inode = object.value("inode").toString();
        bool deviceValid = false, inodeValid = false;
        entry.device.toULongLong(&deviceValid); entry.inode.toULongLong(&inodeValid);
        if (QUuid(entry.id).isNull() || ids.contains(entry.id) || !QDir::isAbsolutePath(entry.path)
            || entry.path.contains(QChar::Null) || entry.name.isEmpty() || entry.bytes < 0
            || !deviceValid || !inodeValid || entry.extension.isEmpty()
            || !QStringList{"pdf", "img", "zip", "vid", "txt"}.contains(entry.kind)) { invalid(); return; }
        ids.insert(entry.id); m_entries.append(entry);
    }
}

bool FileShelfController::save(const QList<Entry> &entries)
{
    if (!m_ready) return fail("save", QStringLiteral("文件暂存架存储不可用。"));
    QFile current(m_path);
    if (current.exists() != m_fileExisted || (m_fileExisted
        && (!current.open(QIODevice::ReadOnly) || current.size() > maximumArchiveBytes || current.readAll() != m_persisted)))
        return fail("save", QStringLiteral("暂存架数据已被其他程序修改，请重启后再保存。"));
    QJsonArray records;
    for (const auto &entry : entries) records.append(QJsonObject{
        {"id", entry.id}, {"path", entry.path}, {"name", entry.name}, {"kind", entry.kind},
        {"extension", entry.extension}, {"directory", entry.directory}, {"bytes", entry.bytes},
        {"device", entry.device}, {"inode", entry.inode}});
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"items", records}}).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumArchiveBytes) return fail("save", QStringLiteral("暂存架引用列表已达到容量上限。"));
    if (!QDir().mkpath(m_directory)) return fail("save", QStringLiteral("无法创建暂存架数据目录。"));
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail("save", QStringLiteral("无法保存暂存架数据。"));
    m_entries = entries; m_persisted = bytes; m_fileExisted = true; m_error.clear();
    emit stateChanged();
    return true;
}

void FileShelfController::setActive(bool value)
{
    if (m_active == value) return;
    m_active = value; emit activeChanged();
    if (value) refresh();
}

void FileShelfController::refresh()
{
    if (!m_ready || m_busy) return;
    m_busy = true; emit stateChanged();
    auto *watcher = new QFutureWatcher<QList<Entry>>(this);
    connect(watcher, &QFutureWatcher<QList<Entry>>::finished, this, [this, watcher] {
        m_entries = watcher->result(); watcher->deleteLater();
        m_busy = false; emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([entries = m_entries] {
        QList<Entry> result;
        for (const auto &entry : entries) result.append(inspect(entry.path, &entry));
        return result;
    }));
}

bool FileShelfController::addUrls(const QList<QUrl> &urls)
{
    if (m_busy || m_dragging) return false;
    if (!m_ready) return fail("add", QStringLiteral("文件暂存架存储不可用。"));
    if (urls.isEmpty()) return false;
    for (const auto &url : urls) {
        if (!url.isLocalFile() || !QDir::isAbsolutePath(url.toLocalFile()) || url.toLocalFile().contains(QChar::Null))
            return fail("add", QStringLiteral("请选择本机可访问的文件或文件夹。"));
    }
    m_busy = true; emit stateChanged();
    auto *watcher = new QFutureWatcher<QList<Entry>>(this);
    connect(watcher, &QFutureWatcher<QList<Entry>>::finished, this, [this, watcher] {
        const auto incoming = watcher->result(); watcher->deleteLater(); m_busy = false;
        auto next = m_entries; QStringList added;
        for (const auto &entry : incoming) {
            if (!entry.error.isEmpty()) { fail("add", entry.name.isEmpty() ? entry.error : entry.name + QStringLiteral("：") + entry.error); return; }
            const auto existing = std::find_if(next.cbegin(), next.cend(), [&entry](const Entry &candidate) {
                return candidate.device == entry.device && candidate.inode == entry.inode;
            });
            if (existing != next.cend()) continue;
            next.append(entry); added.append(entry.id);
        }
        if (added.isEmpty()) { emit stateChanged(); emit actionFinished("add", true, QStringLiteral("所选项目已在暂存架中。")); return; }
        if (save(next)) { emit itemsAdded(added); emit actionFinished("add", true, QStringLiteral("已添加 %1 个引用").arg(added.size())); }
    });
    watcher->setFuture(QtConcurrent::run([urls] {
        QList<Entry> result;
        for (const auto &url : urls) result.append(inspect(url.toLocalFile()));
        return result;
    }));
    return true;
}

void FileShelfController::pasteFiles()
{
    if (m_busy || m_dragging) return;
    const auto executable = QStandardPaths::findExecutable(QStringLiteral("wl-paste"));
    if (executable.isEmpty()) { fail("paste", QStringLiteral("需要 wl-clipboard 才能粘贴文件引用。")); return; }
    auto *process = new QProcess(this);
    auto *timeout = new QTimer(process); timeout->setSingleShot(true); timeout->setInterval(2000);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { fail("paste", QStringLiteral("无法读取文件剪贴板。")); process->deleteLater(); }
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [process] {
        if (process->bytesAvailable() > maximumArchiveBytes) process->kill();
    });
    connect(process, &QProcess::finished, this, [this, process, timeout](int code, QProcess::ExitStatus status) {
        timeout->stop();
        const auto bytes = process->readAllStandardOutput(); process->deleteLater();
        if (status != QProcess::NormalExit || code != 0 || bytes.size() > maximumArchiveBytes) {
            fail("paste", QStringLiteral("剪贴板没有可添加的本地文件。")); return;
        }
        QMimeData mime; mime.setData(QStringLiteral("text/uri-list"), bytes);
        if (mime.urls().isEmpty()) fail("paste", QStringLiteral("剪贴板没有可添加的本地文件。"));
        else addUrls(mime.urls());
    });
    process->start(executable, {QStringLiteral("--type"), QStringLiteral("text/uri-list"), QStringLiteral("--no-newline")});
    timeout->start();
}

bool FileShelfController::removeItems(const QStringList &ids)
{
    if (m_busy || m_dragging || ids.isEmpty()) return false;
    auto next = m_entries;
    next.removeIf([&ids](const Entry &entry) { return ids.contains(entry.id); });
    if (save(next)) { emit actionFinished("remove", true, QStringLiteral("已从暂存架移除")); return true; }
    return false;
}

bool FileShelfController::clear()
{
    if (m_busy || m_dragging) return false;
    if (save({})) { emit actionFinished("clear", true, QStringLiteral("暂存架已清空")); return true; }
    return false;
}

const FileShelfController::Entry *FileShelfController::find(const QString &id) const
{
    for (const auto &entry : m_entries) if (entry.id == id) return &entry;
    return nullptr;
}

void FileShelfController::readProperties(const QString &id)
{
    const auto token = ++m_propertiesRequest;
    m_properties = {{"id", id}}; emit propertiesChanged();
    const auto *entry = find(id);
    if (!entry || !entry->error.isEmpty()) return;
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, token] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (token != m_propertiesRequest) return;
        m_properties = result; emit propertiesChanged();
    });
    watcher->setFuture(QtConcurrent::run([entry = *entry] {
        const auto checked = inspect(entry.path, &entry);
        QVariantMap result{{"id", entry.id}, {"created", QStringLiteral("—")}, {"modified", QStringLiteral("—")}, {"count", QStringLiteral("—")}};
        if (!checked.error.isEmpty()) return result;
        const QFileInfo info(entry.path);
        result["created"] = dateLabel(info.birthTime()); result["modified"] = dateLabel(info.lastModified());
        if (entry.directory) {
            QDirIterator children(entry.path, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags);
            int count = 0;
            while (children.hasNext() && count <= 10000) { children.next(); ++count; }
            if (count <= 10000) result["count"] = QStringLiteral("%1 项").arg(count);
        }
        return result;
    }));
}

QList<QUrl> FileShelfController::exportUrls(const QStringList &ids, QString *error) const
{
    QList<QUrl> urls;
    for (const auto &id : ids) {
        const auto *entry = find(id);
        if (!entry) { *error = QStringLiteral("此项目已不在暂存架中。"); return {}; }
        const auto checked = inspect(entry->path, entry);
        if (!checked.error.isEmpty()) { *error = checked.error; return {}; }
        urls.append(QUrl::fromLocalFile(entry->path));
    }
    return urls;
}

bool FileShelfController::openItem(const QString &id)
{
    QString error;
    const auto urls = exportUrls({id}, &error);
    if (urls.isEmpty()) return fail("open", error);
    if (!QDesktopServices::openUrl(urls.constFirst())) return fail("open", QStringLiteral("无法使用默认应用打开此项目。"));
    emit externalOpened();
    return true;
}

void FileShelfController::revealItem(const QString &id)
{
    QString error;
    const auto urls = exportUrls({id}, &error);
    if (urls.isEmpty()) { fail("reveal", error); return; }
    const auto url = urls.constFirst();
    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
        QStringLiteral("/org/freedesktop/FileManager1"), QStringLiteral("org.freedesktop.FileManager1"), QStringLiteral("ShowItems"));
    message << QStringList{url.toString(QUrl::FullyEncoded)} << QString();
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, url] {
        const QDBusPendingReply<> reply = *watcher; watcher->deleteLater();
        if (!reply.isError() || QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absolutePath())))
            emit externalOpened();
        else fail("reveal", QStringLiteral("无法在文件管理器中显示此项目。"));
    });
}

void FileShelfController::beginDrag(const QStringList &ids)
{
    if (m_dragging || m_busy || !m_window || ids.isEmpty()) return;
    QString error;
    const auto urls = exportUrls(ids, &error);
    if (urls.isEmpty()) { fail("drag", error); return; }
    auto *mime = new QMimeData;
    mime->setUrls(urls);
    mime->setData(QStringLiteral("application/x-kde-cutselection"), QByteArrayLiteral("0"));
    QPointer<QDrag> drag = new QDrag(m_window);
    drag->setMimeData(mime);
    const auto iconName = QMimeDatabase().mimeTypeForFile(urls.constFirst().toLocalFile(), QMimeDatabase::MatchExtension).iconName();
    QPixmap icon = QIcon::fromTheme(iconName, QIcon::fromTheme(QStringLiteral("text-x-generic"))).pixmap(64, 64);
    if (!icon.isNull()) { drag->setPixmap(icon); drag->setHotSpot(QPoint(32, 32)); }
    bool started = false;
    m_dragging = true;
    // This callback runs inside QDrag's native event loop, after Wayland has
    // acquired the source surface. Keep that surface mapped until the drop.
    QTimer::singleShot(0, drag, [this, &started] { started = true; emit dragStarted(); });
    const auto result = drag->exec(Qt::CopyAction, Qt::CopyAction);
    if (drag) delete drag;
    m_dragging = false;
    if (started) emit dragFinished(result == Qt::CopyAction);
}

QVariantList FileShelfController::mediaChoices() const
{
    QVariantList result;
    const QMimeDatabase database;
    for (const auto &entry : m_entries) {
        const Entry current = inspect(entry.path, &entry);
        if (!current.error.isEmpty() || current.directory) continue;
        const QString mime = database.mimeTypeForFile(current.path, QMimeDatabase::MatchContent).name();
        if (!mime.startsWith(QStringLiteral("image/")) && !mime.startsWith(QStringLiteral("video/"))) continue;
        result.append(QVariantMap{{QStringLiteral("id"), current.id}, {QStringLiteral("name"), current.name},
            {QStringLiteral("kind"), mime.startsWith(QStringLiteral("video/")) ? QStringLiteral("video") : QStringLiteral("image")}});
    }
    return result;
}

QUrl FileShelfController::mediaUrl(const QString &id) const
{
    const Entry *entry = find(id);
    if (!entry) return {};
    const Entry current = inspect(entry->path, entry);
    if (!current.error.isEmpty() || current.directory) return {};
    const QString mime = QMimeDatabase().mimeTypeForFile(current.path, QMimeDatabase::MatchContent).name();
    return mime.startsWith(QStringLiteral("image/")) || mime.startsWith(QStringLiteral("video/"))
        ? QUrl::fromLocalFile(current.path) : QUrl();
}
