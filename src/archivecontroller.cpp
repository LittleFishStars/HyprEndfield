#include <QCoreApplication>
#include "archivecontroller.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QPasswordDigestor>
#include <QQuickTextDocument>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QUuid>
#include <QtConcurrentRun>

#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")
#include <security/pam_appl.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
constexpr int maximumDocuments = 2000;
constexpr int maximumCategories = 100;
constexpr int maximumMedia = 16;
constexpr qsizetype maximumBodyBytes = 2 * 1024 * 1024;
constexpr qsizetype maximumPayloadBytes = 20 * 1024 * 1024;
constexpr int verifierIterations = 600'000;
constexpr int verifierKeyBytes = 32;
constexpr int verifierSaltBytes = 16;

const SecretSchema archiveSecretSchema = [] {
    SecretSchema value{};
    value.name = "com.octoberprayrain.HyprEndfield.archive.classified";
    value.flags = SECRET_SCHEMA_NONE;
    value.attributes[0].name = "account";
    value.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING;
    return value;
}();

QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper(); }

QString secretError(GError *error)
{
    if (!error) return QCoreApplication::translate("HyprEndfield", "系统密钥服务没有返回原因。");
    const QString message = QString::fromUtf8(error->message);
    g_error_free(error);
    return message;
}

class SecretServiceArchiveVault final : public ArchivePasswordVault
{
public:
    QByteArray read(const QString &account, bool *found, QString *error) override
    {
        GError *failure = nullptr;
        const QByteArray accountBytes = account.toUtf8();
        gchar *password = secret_password_lookup_sync(&archiveSecretSchema, nullptr, &failure,
            "account", accountBytes.constData(), nullptr);
        if (failure) {
            if (found) *found = false;
            if (error) *error = secretError(failure);
            return {};
        }
        if (!password) {
            if (found) *found = false;
            return {};
        }
        if (found) *found = true;
        const QByteArray decoded = QByteArray::fromBase64(QByteArray(password));
        secret_password_free(password);
        return decoded;
    }

    bool add(const QString &account, const QByteArray &value, QString *error) override
    {
        GError *failure = nullptr;
        const QByteArray accountBytes = account.toUtf8();
        const QByteArray encoded = value.toBase64();
        const bool result = secret_password_store_sync(&archiveSecretSchema, SECRET_COLLECTION_DEFAULT,
            "HyprEndfield Archive", encoded.constData(), nullptr, &failure,
            "account", accountBytes.constData(), nullptr);
        if (!result && error) *error = secretError(failure);
        else if (failure) g_error_free(failure);
        return result;
    }

    bool remove(const QString &account, QString *error) override
    {
        GError *failure = nullptr;
        const QByteArray accountBytes = account.toUtf8();
        const bool result = secret_password_clear_sync(&archiveSecretSchema, nullptr, &failure,
            "account", accountBytes.constData(), nullptr);
        if (!result && failure) {
            if (error) *error = secretError(failure);
            return false;
        }
        if (failure) g_error_free(failure);
        return true;
    }
};

QByteArray verifier(const QByteArray &password)
{
    std::array<quint32, verifierSaltBytes / 4> random{};
    QRandomGenerator::system()->fillRange(random.data(), random.size());
    const QByteArray salt(reinterpret_cast<const char *>(random.data()), verifierSaltBytes);
    const QByteArray key = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
        password, salt, verifierIterations, verifierKeyBytes);
    return QJsonDocument(QJsonObject{{"algorithm", "PBKDF2-HMAC-SHA256"}, {"iterations", verifierIterations},
        {"key", QString::fromLatin1(key.toBase64())}, {"salt", QString::fromLatin1(salt.toBase64())},
        {"version", 1}}).toJson(QJsonDocument::Compact);
}

bool verifyPassword(const QByteArray &record, const QByteArray &password)
{
    if (record.size() > 4096) return false;
    QJsonParseError parseError{};
    const auto object = QJsonDocument::fromJson(record, &parseError).object();
    const int rounds = object.value("iterations").toInt();
    const QByteArray salt = QByteArray::fromBase64(object.value("salt").toString().toLatin1());
    const QByteArray expected = QByteArray::fromBase64(object.value("key").toString().toLatin1());
    if (parseError.error != QJsonParseError::NoError || object.value("version").toInt() != 1
        || object.value("algorithm").toString() != QStringLiteral("PBKDF2-HMAC-SHA256")
        || rounds < verifierIterations || rounds > 10'000'000 || salt.size() != verifierSaltBytes
        || expected.size() != verifierKeyBytes) return false;
    const QByteArray candidate = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
        password, salt, rounds, verifierKeyBytes);
    uchar difference = 0;
    for (int index = 0; index < verifierKeyBytes; ++index)
        difference |= uchar(candidate[index]) ^ uchar(expected[index]);
    return difference == 0;
}

QJsonObject mediaObject(const ArchiveController::Media &media)
{
    return {{"device", media.device}, {"id", media.id}, {"inode", media.inode},
            {"kind", media.kind}, {"name", media.name}, {"path", media.path}};
}

QByteArray payload(const ArchiveController::Entry &entry)
{
    QJsonArray media;
    for (const auto &item : entry.media) media.append(mediaObject(item));
    return QJsonDocument(QJsonObject{{"body", entry.body}, {"richBody", entry.richBody},
        {"media", media}}).toJson(QJsonDocument::Compact);
}

bool parsePayload(const QByteArray &bytes, ArchiveController::Entry &entry)
{
    if (bytes.size() > maximumPayloadBytes) return false;
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto object = document.object();
    entry.body = object.value("body").toString();
    entry.richBody = object.value("richBody").toString();
    const auto array = object.value("media").toArray();
    if (array.size() > maximumMedia || entry.body.toUtf8().size() > maximumBodyBytes) return false;
    for (const auto &value : array) {
        const auto item = value.toObject();
        ArchiveController::Media media;
        media.id = item.value("id").toString(); media.path = item.value("path").toString();
        media.name = item.value("name").toString(); media.kind = item.value("kind").toString();
        media.device = item.value("device").toString(); media.inode = item.value("inode").toString();
        if (media.id.isEmpty() || media.path.isEmpty() || (media.kind != "image" && media.kind != "video" && media.kind != "gif")) return false;
        entry.media.append(media);
    }
    return true;
}

struct PamConversationData { QByteArray password; };
int pamConversation(int count, const pam_message **messages, pam_response **responses, void *context)
{
    if (count <= 0 || !messages || !responses || !context) return PAM_CONV_ERR;
    auto *data = static_cast<PamConversationData *>(context);
    auto *values = static_cast<pam_response *>(calloc(size_t(count), sizeof(pam_response)));
    if (!values) return PAM_BUF_ERR;
    for (int index = 0; index < count; ++index) {
        if (messages[index]->msg_style == PAM_PROMPT_ECHO_OFF || messages[index]->msg_style == PAM_PROMPT_ECHO_ON)
            values[index].resp = strdup(data->password.constData());
        else if (messages[index]->msg_style != PAM_TEXT_INFO && messages[index]->msg_style != PAM_ERROR_MSG) {
            for (int release = 0; release <= index; ++release) free(values[release].resp);
            free(values); return PAM_CONV_ERR;
        }
    }
    *responses = values;
    return PAM_SUCCESS;
}

QVariantMap authenticateWithPam(QByteArray password)
{
    const passwd *user = getpwuid(geteuid());
    if (!user || !user->pw_name) return {{"ok", false}, {"message", QCoreApplication::translate("HyprEndfield", "无法确定当前 Linux 用户。")}};
    PamConversationData data{password};
    pam_conv conversation{pamConversation, &data};
    pam_handle_t *handle = nullptr;
    int result = pam_start("login", user->pw_name, &conversation, &handle);
    if (result == PAM_SUCCESS) result = pam_authenticate(handle, PAM_SILENT);
    if (result == PAM_SUCCESS) result = pam_acct_mgmt(handle, PAM_SILENT);
    QString message;
    if (result != PAM_SUCCESS) message = QString::fromLocal8Bit(handle ? pam_strerror(handle, result) : "PAM 初始化失败");
    if (handle) pam_end(handle, result);
    std::fill(password.begin(), password.end(), '\0');
    return {{"ok", result == PAM_SUCCESS}, {"message", result == PAM_SUCCESS ? QString() : message}};
}
}

QByteArray ArchiveMemoryPasswordVault::read(const QString &account, bool *found, QString *error)
{
    Q_UNUSED(error)
    QMutexLocker locker(&m_mutex);
    if (found) *found = m_values.contains(account);
    return m_values.value(account);
}
bool ArchiveMemoryPasswordVault::add(const QString &account, const QByteArray &value, QString *error)
{
    Q_UNUSED(error)
    QMutexLocker locker(&m_mutex); m_values.insert(account, value); return true;
}
bool ArchiveMemoryPasswordVault::remove(const QString &account, QString *error)
{
    Q_UNUSED(error)
    QMutexLocker locker(&m_mutex); m_values.remove(account); return true;
}
void ArchiveMemoryPasswordVault::forget(const QString &account)
{
    QMutexLocker locker(&m_mutex); m_values.remove(account);
}

ArchiveController::ArchiveController(QObject *parent, QString directory,
    std::shared_ptr<ArchivePasswordVault> vault) : QObject(parent), m_vault(std::move(vault))
{
    m_directory = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/HyprEndfield/Archive") : std::move(directory);
    m_path = m_directory + QStringLiteral("/archive.sqlite");
    m_connectionName = QStringLiteral("HyprEndfieldArchive-%1").arg(quintptr(this), 0, 16);
    if (!m_vault) m_vault = std::make_shared<SecretServiceArchiveVault>();
    m_saveTimer.setSingleShot(true); m_saveTimer.setInterval(350);
    connect(&m_saveTimer, &QTimer::timeout, this, &ArchiveController::flushSelected);
    load();
}

ArchiveController::~ArchiveController()
{
    flushSelected();
    const QString connection = m_connectionName;
    if (m_database.isValid()) m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(connection);
}

bool ArchiveController::exec(const QString &statement)
{
    QSqlQuery query(m_database);
    if (query.exec(statement)) return true;
    m_error = query.lastError().text(); return false;
}

void ArchiveController::load()
{
    if (!QDir().mkpath(m_directory)) { m_error = QCoreApplication::translate("HyprEndfield", "无法创建档案库目录。"); emit stateChanged(); return; }
    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_path);
    if (!m_database.open()) { m_error = m_database.lastError().text(); emit stateChanged(); return; }
    exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    if (!createSchema() || !loadRows()) { emit stateChanged(); return; }
    m_ready = true; emit stateChanged();
}

bool ArchiveController::createSchema()
{
    if (!m_database.transaction()) { m_error = m_database.lastError().text(); return false; }
    const bool ok = exec(QStringLiteral("CREATE TABLE IF NOT EXISTS categories (id TEXT PRIMARY KEY, name TEXT NOT NULL, created INTEGER NOT NULL, viewed INTEGER NOT NULL DEFAULT 1)"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS entries (id TEXT PRIMARY KEY, template TEXT NOT NULL, title TEXT NOT NULL, date INTEGER NOT NULL, modified INTEGER NOT NULL, mediaCount INTEGER NOT NULL, payload BLOB NOT NULL, categoryID TEXT, thumbnail BLOB, classification TEXT)"))
        && exec(QStringLiteral("CREATE TABLE IF NOT EXISTS state (key TEXT PRIMARY KEY, value TEXT NOT NULL)"));
    if (!ok || !m_database.commit()) { m_database.rollback(); if (m_error.isEmpty()) m_error = m_database.lastError().text(); return false; }
    QSqlQuery version(m_database); version.exec(QStringLiteral("PRAGMA user_version"));
    if (!version.next() || (version.value(0).toInt() != 0 && version.value(0).toInt() != 2 && version.value(0).toInt() != 3)) {
        m_error = QCoreApplication::translate("HyprEndfield", "档案库数据库版本无法识别，原文件保持不变。"); return false;
    }
    return true;
}

bool ArchiveController::loadRows()
{
    QSqlQuery categories(m_database);
    if (!categories.exec(QStringLiteral("SELECT id,name,created,viewed FROM categories ORDER BY created,id"))) { m_error = categories.lastError().text(); return false; }
    while (categories.next()) {
        Category category{categories.value(0).toString(), categories.value(1).toString(), categories.value(2).toLongLong(), categories.value(3).toBool()};
        if (category.id.isEmpty() || normalizedCategoryName(category.name).isEmpty() || category.name.size() > 80) { m_error = QCoreApplication::translate("HyprEndfield", "档案库含有无效分类，原文件保持不变。"); return false; }
        m_categories.append(category);
    }
    if (m_categories.size() > maximumCategories) { m_error = QCoreApplication::translate("HyprEndfield", "档案库分类数量超过上限。"); return false; }
    QSqlQuery entries(m_database);
    if (!entries.exec(QStringLiteral("SELECT id,title,date,modified,payload,categoryID,classification FROM entries ORDER BY date DESC,id"))) { m_error = entries.lastError().text(); return false; }
    while (entries.next()) {
        Entry entry; entry.id = entries.value(0).toString(); entry.title = entries.value(1).toString();
        entry.date = entries.value(2).toLongLong(); entry.modified = entries.value(3).toLongLong();
        entry.categoryId = entries.value(5).toString(); entry.keyId = entries.value(6).toString();
        if (entry.id.isEmpty() || entry.title.size() > 200 || !parsePayload(entries.value(4).toByteArray(), entry)) {
            m_error = QCoreApplication::translate("HyprEndfield", "档案库含有无效文档，原文件保持不变。"); return false;
        }
        m_entries.append(entry);
    }
    if (m_entries.size() > maximumDocuments) { m_error = QCoreApplication::translate("HyprEndfield", "档案库文档数量超过上限。"); return false; }
    QSqlQuery state(m_database);
    if (!state.exec(QStringLiteral("SELECT key,value FROM state"))) { m_error = state.lastError().text(); return false; }
    while (state.next()) {
        if (state.value(0).toString() == QStringLiteral("filter")) m_filter = state.value(1).toString();
        else if (state.value(0).toString() == QStringLiteral("selected")) m_selectedId = state.value(1).toString();
    }
    if (m_filter != "all" && m_filter != "uncategorized" && !findCategory(m_filter)) m_filter = QStringLiteral("all");
    if (!selectedEntry()) m_selectedId.clear();
    updateSchemaVersion();
    if (const auto *selected = selectedEntry(); selected && !selected->keyId.isEmpty())
        probePasswordStatus(selected->id, classificationAccount(*selected));
    return true;
}

ArchiveController::Entry *ArchiveController::selectedEntry()
{
    for (auto &entry : m_entries) if (entry.id == m_selectedId) return &entry;
    return nullptr;
}
const ArchiveController::Entry *ArchiveController::selectedEntry() const
{
    for (const auto &entry : m_entries) if (entry.id == m_selectedId) return &entry;
    return nullptr;
}
const ArchiveController::Category *ArchiveController::findCategory(const QString &id) const
{
    for (const auto &category : m_categories) if (category.id == id) return &category;
    return nullptr;
}

QString ArchiveController::dateLabel(qint64 milliseconds)
{
    return QDateTime::fromMSecsSinceEpoch(milliseconds).toLocalTime().toString(QStringLiteral("yyyy.MM.dd"));
}
QString ArchiveController::normalizedCategoryName(const QString &name)
{
    QString value = name.trimmed().normalized(QString::NormalizationForm_C);
    for (const QChar character : value) if (character.category() == QChar::Other_Control || character == '\n' || character == '\r') return {};
    return value;
}
QVariantMap ArchiveController::categoryMap(const Category &category, int count)
{
    return {{"id", category.id}, {"name", category.name}, {"count", count}, {"isNew", !category.viewed}};
}
bool ArchiveController::mediaStillAvailable(const Media &media)
{
    struct stat identity{}; const auto native = QFile::encodeName(media.path);
    return ::stat(native.constData(), &identity) == 0 && S_ISREG(identity.st_mode) && ::access(native.constData(), R_OK) == 0
        && QString::number(qulonglong(identity.st_dev)) == media.device && QString::number(qulonglong(identity.st_ino)) == media.inode;
}
QVariantMap ArchiveController::mediaMap(const Media &media)
{
    const bool available = mediaStillAvailable(media);
    return {{"id", media.id}, {"name", media.name}, {"kind", media.kind}, {"available", available},
            {"url", available ? QUrl::fromLocalFile(media.path) : QUrl()}};
}
QList<int> ArchiveController::redactionWidths(const QString &body)
{
    QList<int> widths;
    const QString clipped = body.left(1600);
    for (const auto &paragraph : clipped.split('\n')) {
        if (paragraph.isEmpty()) { widths.append(5); continue; }
        for (int position = 0; position < paragraph.size() && widths.size() < 16; position += 52)
            widths.append(std::min(52, int(paragraph.size() - position)));
        if (widths.size() >= 16) break;
    }
    return widths;
}

QVariantMap ArchiveController::state() const
{
    QVariantList categories;
    categories.append(QVariantMap{{"id", "all"}, {"name", QCoreApplication::translate("HyprEndfield", "全部")}, {"count", m_entries.size()}, {"isNew", false}});
    int uncategorized = 0;
    for (const auto &entry : m_entries) if (entry.categoryId.isEmpty()) ++uncategorized;
    categories.append(QVariantMap{{"id", "uncategorized"}, {"name", QCoreApplication::translate("HyprEndfield", "未分类")}, {"count", uncategorized}, {"isNew", false}});
    for (const auto &category : m_categories) {
        int count = 0; for (const auto &entry : m_entries) if (entry.categoryId == category.id) ++count;
        categories.append(categoryMap(category, count));
    }
    QVariantList entries;
    int number = 0;
    for (const auto &entry : m_entries) {
        ++number;
        if (m_filter == "uncategorized" && !entry.categoryId.isEmpty()) continue;
        if (m_filter != "all" && m_filter != "uncategorized" && entry.categoryId != m_filter) continue;
        const bool classified = !entry.keyId.isEmpty();
        const QVariant thumbnail = !classified && !entry.media.isEmpty() && mediaStillAvailable(entry.media.constFirst())
            ? QVariant(QUrl(QStringLiteral("image://archive-thumbnails/") + QString::fromLatin1(entry.media.constFirst().path.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))))
            : QVariant();
        entries.append(QVariantMap{{"id", entry.id}, {"title", classified ? QCoreApplication::translate("HyprEndfield", "[ 机密 ]") : entry.title},
            {"number", number}, {"date", dateLabel(entry.date)}, {"classified", classified}, {"mediaCount", classified ? 0 : entry.media.size()},
            {"thumbnail", thumbnail}});
    }
    QVariantMap result{{"ready", m_ready}, {"error", m_error}, {"busy", m_busy}, {"filter", m_filter},
        {"selectedId", m_selectedId}, {"categories", categories}, {"entries", entries}, {"total", m_entries.size()}};
    if (const auto *entry = selectedEntry()) {
        const bool classified = !entry->keyId.isEmpty();
        const bool unlocked = classified && m_unlockedId == entry->id;
        result.insert("page", classified && !unlocked ? "locked" : "editor");
        result.insert("classification", classified ? "protected" : "none");
        QVariantMap document{{"id", entry->id}, {"categoryId", entry->categoryId}, {"date", dateLabel(entry->date)},
            {"classified", classified}, {"unlocked", unlocked}};
        if (!classified || unlocked) {
            QVariantList media; for (const auto &item : entry->media) media.append(mediaMap(item));
            document.insert("title", entry->title); document.insert("body", entry->body);
            document.insert("richBody", entry->richBody); document.insert("media", media);
        } else {
            document.insert("passwordStatus", m_passwordStatuses.value(entry->id, QStringLiteral("checking")));
            document.insert("titleWidth", std::clamp(int(entry->title.size()), 5, 46));
            QVariantList bars; for (int width : redactionWidths(entry->body)) bars.append(width);
            document.insert("redactionWidths", bars);
        }
        result.insert("document", document);
    } else result.insert("page", "list");
    return result;
}

bool ArchiveController::fail(const QString &action, const QString &message)
{
    m_error = message; emit stateChanged(); emit actionFinished(action, false, message); return false;
}
void ArchiveController::setBusy(bool value) { if (m_busy == value) return; m_busy = value; emit stateChanged(); }
void ArchiveController::setActive(bool value)
{
    if (m_active == value) return;
    m_active = value;
    if (!value) { flushSelected(); m_unlockedId.clear(); m_ownerAuthorizedId.clear(); emit stateChanged(); }
    emit activeChanged();
}

bool ArchiveController::saveStateValue(const QString &key, const QString &value)
{
    QSqlQuery query(m_database); query.prepare(QStringLiteral("INSERT INTO state(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    query.addBindValue(key); query.addBindValue(value.isNull() ? QStringLiteral("") : value);
    if (query.exec()) return true;
    return fail(QStringLiteral("save"), query.lastError().text());
}
void ArchiveController::updateSchemaVersion()
{
    bool classified = false; for (const auto &entry : m_entries) if (!entry.keyId.isEmpty()) { classified = true; break; }
    exec(QStringLiteral("PRAGMA user_version=%1").arg(classified ? 3 : 2));
}
bool ArchiveController::saveEntry(const Entry &entry)
{
    const QByteArray encoded = payload(entry);
    if (encoded.size() > maximumPayloadBytes) return fail(QStringLiteral("save"), QCoreApplication::translate("HyprEndfield", "文档内容超过 20 MiB 上限。"));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO entries(id,template,title,date,modified,mediaCount,payload,categoryID,thumbnail,classification) VALUES(?,?,?,?,?,?,?,?,NULL,?) "
        "ON CONFLICT(id) DO UPDATE SET title=excluded.title,modified=excluded.modified,mediaCount=excluded.mediaCount,payload=excluded.payload,categoryID=excluded.categoryID,classification=excluded.classification"));
    query.addBindValue(entry.id); query.addBindValue(QStringLiteral("journal")); query.addBindValue(entry.title);
    query.addBindValue(entry.date); query.addBindValue(entry.modified); query.addBindValue(entry.media.size());
    query.addBindValue(encoded); query.addBindValue(entry.categoryId.isEmpty() ? QVariant(QMetaType::fromType<QString>()) : QVariant(entry.categoryId));
    query.addBindValue(entry.keyId.isEmpty() ? QVariant(QMetaType::fromType<QString>()) : QVariant(entry.keyId));
    if (query.exec()) return true;
    return fail(QStringLiteral("save"), query.lastError().text());
}
void ArchiveController::scheduleSave() { m_saveTimer.start(); }
void ArchiveController::flushSelected()
{
    m_saveTimer.stop();
    if (m_database.isOpen()) if (const auto *entry = selectedEntry()) saveEntry(*entry);
}

void ArchiveController::probePasswordStatus(const QString &id, const QString &account)
{
    m_passwordStatuses.insert(id, QStringLiteral("checking"));
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (std::any_of(m_entries.cbegin(), m_entries.cend(), [&](const Entry &entry) { return entry.id == id && !entry.keyId.isEmpty(); })) {
            m_passwordStatuses.insert(id, result.value("status").toString()); emit stateChanged();
        }
    });
    watcher->setFuture(QtConcurrent::run([vault = m_vault, account] {
        bool found = false; QString error; vault->read(account, &found, &error);
        return QVariantMap{{"status", !error.isEmpty() ? QStringLiteral("unavailable") : found ? QStringLiteral("protected") : QStringLiteral("missing")}};
    }));
}

bool ArchiveController::selectCategory(const QString &id)
{
    if (id != "all" && id != "uncategorized" && !findCategory(id)) return false;
    flushSelected(); m_filter = id; m_selectedId.clear(); m_unlockedId.clear(); m_ownerAuthorizedId.clear();
    for (auto &category : m_categories) if (category.id == id && !category.viewed) {
        category.viewed = true; QSqlQuery query(m_database); query.prepare("UPDATE categories SET viewed=1 WHERE id=?"); query.addBindValue(id); query.exec(); break;
    }
    saveStateValue("filter", id); saveStateValue("selected", {}); emit stateChanged(); return true;
}
QString ArchiveController::addCategory()
{
    if (!m_ready || m_categories.size() >= maximumCategories) { fail("category", QCoreApplication::translate("HyprEndfield", "最多可创建 100 个分类。")); return {}; }
    QString name; int number = m_categories.size() + 1;
    do { name = QCoreApplication::translate("HyprEndfield", "新分类 %1").arg(number++); } while (std::any_of(m_categories.cbegin(), m_categories.cend(), [&](const Category &item) { return item.name.compare(name, Qt::CaseInsensitive) == 0; }));
    Category category{uuid(), name, QDateTime::currentMSecsSinceEpoch(), false};
    QSqlQuery query(m_database); query.prepare("INSERT INTO categories(id,name,created,viewed) VALUES(?,?,?,0)");
    query.addBindValue(category.id); query.addBindValue(category.name); query.addBindValue(category.created);
    if (!query.exec()) { fail("category", query.lastError().text()); return {}; }
    m_categories.append(category); emit stateChanged(); emit actionFinished("category", true, QCoreApplication::translate("HyprEndfield", "分类已创建。")); return category.id;
}
bool ArchiveController::renameCategory(const QString &id, const QString &name)
{
    const QString normalized = normalizedCategoryName(name);
    if (normalized.isEmpty() || normalized.size() > 24 || normalized.compare("全部", Qt::CaseInsensitive) == 0 || normalized.compare("未分类", Qt::CaseInsensitive) == 0)
        return fail("category", QCoreApplication::translate("HyprEndfield", "分类名称需要为 1 至 24 个字符。"));
    if (std::any_of(m_categories.cbegin(), m_categories.cend(), [&](const Category &item) { return item.id != id && item.name.compare(normalized, Qt::CaseInsensitive) == 0; }))
        return fail("category", QCoreApplication::translate("HyprEndfield", "已经存在同名分类。"));
    for (auto &category : m_categories) if (category.id == id) {
        QSqlQuery query(m_database); query.prepare("UPDATE categories SET name=? WHERE id=?"); query.addBindValue(normalized); query.addBindValue(id);
        if (!query.exec()) return fail("category", query.lastError().text());
        category.name = normalized; emit stateChanged(); return true;
    }
    return false;
}
bool ArchiveController::removeCategory(const QString &id)
{
    if (!findCategory(id)) return false;
    if (!m_database.transaction()) return fail("category", m_database.lastError().text());
    QSqlQuery update(m_database); update.prepare("UPDATE entries SET categoryID=NULL WHERE categoryID=?"); update.addBindValue(id);
    QSqlQuery remove(m_database); remove.prepare("DELETE FROM categories WHERE id=?"); remove.addBindValue(id);
    if (!update.exec() || !remove.exec() || !m_database.commit()) { m_database.rollback(); return fail("category", QCoreApplication::translate("HyprEndfield", "无法删除分类。")); }
    for (auto &entry : m_entries) if (entry.categoryId == id) entry.categoryId.clear();
    m_categories.erase(std::remove_if(m_categories.begin(), m_categories.end(), [&](const Category &item) { return item.id == id; }), m_categories.end());
    if (m_filter == id) { m_filter = "uncategorized"; saveStateValue("filter", m_filter); }
    emit stateChanged(); return true;
}
QString ArchiveController::createDocument()
{
    if (!m_ready || m_entries.size() >= maximumDocuments) { fail("document", QCoreApplication::translate("HyprEndfield", "最多可保存 2000 份文档。")); return {}; }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    Entry entry; entry.id = uuid(); entry.title = QCoreApplication::translate("HyprEndfield", "未命名文档"); entry.date = now; entry.modified = now;
    if (m_filter != "all" && m_filter != "uncategorized") entry.categoryId = m_filter;
    if (!saveEntry(entry)) return {};
    m_entries.prepend(entry); m_selectedId = entry.id; saveStateValue("selected", m_selectedId); emit stateChanged(); return entry.id;
}
bool ArchiveController::selectDocument(const QString &id)
{
    flushSelected();
    const bool exists = std::any_of(m_entries.cbegin(), m_entries.cend(), [&](const Entry &entry) { return entry.id == id; });
    if (!exists) return false;
    m_selectedId = id; m_unlockedId.clear(); m_ownerAuthorizedId.clear(); saveStateValue("selected", id);
    if (const auto *entry = selectedEntry(); entry && !entry->keyId.isEmpty())
        probePasswordStatus(entry->id, classificationAccount(*entry));
    emit stateChanged(); return true;
}
void ArchiveController::closeDocument()
{
    flushSelected(); m_selectedId.clear(); m_unlockedId.clear(); m_ownerAuthorizedId.clear(); saveStateValue("selected", {}); emit stateChanged();
}
bool ArchiveController::updateSelected(const QString &title, const QString &body, const QString &richBody)
{
    auto *entry = selectedEntry(); if (!entry || (!entry->keyId.isEmpty() && m_unlockedId != entry->id)) return false;
    if (title.size() > 200 || body.toUtf8().size() > maximumBodyBytes || richBody.toUtf8().size() > maximumPayloadBytes)
        return fail("document", QCoreApplication::translate("HyprEndfield", "文档内容超过保存上限。"));
    entry->title = title; entry->body = body; entry->richBody = richBody; entry->modified = QDateTime::currentMSecsSinceEpoch();
    scheduleSave(); emit stateChanged(); return true;
}
bool ArchiveController::moveSelected(const QString &categoryId)
{
    auto *entry = selectedEntry(); if (!entry) return false;
    const QString target = categoryId == "uncategorized" ? QString() : categoryId;
    if (!target.isEmpty() && !findCategory(target)) return false;
    entry->categoryId = target; entry->modified = QDateTime::currentMSecsSinceEpoch(); scheduleSave(); emit stateChanged(); return true;
}
bool ArchiveController::deleteSelected()
{
    const auto *entry = selectedEntry(); if (!entry) return false;
    const QString account = classificationAccount(*entry), id = entry->id; const bool classified = !entry->keyId.isEmpty();
    QSqlQuery query(m_database); query.prepare("DELETE FROM entries WHERE id=?"); query.addBindValue(id);
    if (!query.exec()) return fail("document", query.lastError().text());
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(), [&](const Entry &value) { return value.id == id; }), m_entries.end());
    m_selectedId.clear(); m_unlockedId.clear(); m_ownerAuthorizedId.clear(); saveStateValue("selected", {}); updateSchemaVersion(); emit stateChanged();
    if (classified) (void)QtConcurrent::run([vault = m_vault, account] { QString ignored; vault->remove(account, &ignored); });
    return true;
}

bool ArchiveController::importMedia(const QList<QUrl> &urls)
{
    auto *entry = selectedEntry(); if (!entry || (!entry->keyId.isEmpty() && m_unlockedId != entry->id)) return false;
    if (urls.isEmpty() || entry->media.size() + urls.size() > maximumMedia) return fail("media", QCoreApplication::translate("HyprEndfield", "每份文档最多添加 16 个媒体文件。"));
    QList<Media> additions;
    for (const auto &url : urls) {
        if (!url.isLocalFile()) return fail("media", QCoreApplication::translate("HyprEndfield", "只支持本机图片和视频。"));
        const QString path = QFileInfo(url.toLocalFile()).canonicalFilePath(); QFileInfo info(path); struct stat identity{};
        const auto native = QFile::encodeName(path);
        if (path.isEmpty() || ::stat(native.constData(), &identity) != 0 || !S_ISREG(identity.st_mode) || ::access(native.constData(), R_OK) != 0)
            return fail("media", QCoreApplication::translate("HyprEndfield", "媒体文件无法访问。"));
        const QString mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchContent).name();
        QString kind = mime.startsWith("video/") ? "video" : mime == "image/gif" ? "gif" : mime.startsWith("image/") ? "image" : QString();
        if (kind.isEmpty()) return fail("media", QCoreApplication::translate("HyprEndfield", "只支持图片、GIF 和视频。"));
        Media media{uuid(), path, info.fileName(), kind, QString::number(qulonglong(identity.st_dev)), QString::number(qulonglong(identity.st_ino))};
        additions.append(media);
    }
    entry->media.append(additions); entry->modified = QDateTime::currentMSecsSinceEpoch(); scheduleSave(); emit stateChanged(); return true;
}
bool ArchiveController::removeMedia(int index)
{
    auto *entry = selectedEntry(); if (!entry || index < 0 || index >= entry->media.size()) return false;
    entry->media.removeAt(index); entry->modified = QDateTime::currentMSecsSinceEpoch(); scheduleSave(); emit stateChanged(); return true;
}

bool ArchiveController::validPassword(const QString &password, const QString &confirmation, QString *message)
{
    if (password.size() < 4) { if (message) *message = QCoreApplication::translate("HyprEndfield", "密码至少需要 4 个字符。"); return false; }
    if (password.normalized(QString::NormalizationForm_C).toUtf8().size() > 1024) { if (message) *message = QCoreApplication::translate("HyprEndfield", "密码最多可占用 1024 字节。"); return false; }
    if (password != confirmation) { if (message) *message = QCoreApplication::translate("HyprEndfield", "两次输入的密码不一致。"); return false; }
    return true;
}
QString ArchiveController::classificationAccount(const Entry &entry) const { return entry.id + '/' + entry.keyId; }

void ArchiveController::classifySelected(const QString &password, const QString &confirmation)
{
    auto *entry = selectedEntry(); QString validation;
    if (!entry || !entry->keyId.isEmpty() || m_busy || !validPassword(password, confirmation, &validation)) { if (!validation.isEmpty()) fail("classify", validation); return; }
    const QString id = entry->id, keyId = uuid(), account = id + '/' + keyId;
    QByteArray bytes = password.normalized(QString::NormalizationForm_C).toUtf8(); setBusy(true);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id, keyId] {
        const auto result = watcher->result(); watcher->deleteLater(); setBusy(false);
        if (!result.value("ok").toBool()) { fail("classify", result.value("message").toString()); return; }
        for (auto &value : m_entries) if (value.id == id) { value.keyId = keyId; value.modified = QDateTime::currentMSecsSinceEpoch(); saveEntry(value); m_unlockedId.clear(); break; }
        m_passwordStatuses.insert(id, QStringLiteral("protected"));
        updateSchemaVersion(); emit stateChanged(); emit actionFinished("classify", true, QCoreApplication::translate("HyprEndfield", "文档已设为机密。"));
    });
    watcher->setFuture(QtConcurrent::run([vault = m_vault, account, bytes]() mutable {
        const QByteArray record = verifier(bytes); std::fill(bytes.begin(), bytes.end(), '\0'); QString error;
        const bool ok = vault->add(account, record, &error);
        return QVariantMap{{"ok", ok}, {"message", ok ? QString() : QCoreApplication::translate("HyprEndfield", "无法写入系统密钥服务：") + error}};
    }));
}

void ArchiveController::unlockSelected(const QString &password)
{
    const auto *entry = selectedEntry(); if (!entry || entry->keyId.isEmpty() || m_busy) return;
    const QString id = entry->id, account = classificationAccount(*entry); QByteArray bytes = password.normalized(QString::NormalizationForm_C).toUtf8(); setBusy(true);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id] {
        const auto result = watcher->result(); watcher->deleteLater(); setBusy(false);
        if (m_selectedId != id) return;
        if (result.value("ok").toBool()) { m_passwordStatuses.insert(id, QStringLiteral("protected")); m_unlockedId = id; emit stateChanged(); emit unlockFinished(true, false, {}); }
        else {
            if (!result.value("found").toBool()) m_passwordStatuses.insert(id, QStringLiteral("missing"));
            else if (result.value("unavailable").toBool()) m_passwordStatuses.insert(id, QStringLiteral("unavailable"));
            emit stateChanged(); emit unlockFinished(false, result.value("wrong").toBool(), result.value("message").toString());
        }
    });
    watcher->setFuture(QtConcurrent::run([vault = m_vault, account, bytes]() mutable {
        bool found = false; QString error; const QByteArray record = vault->read(account, &found, &error);
        const bool matches = found && error.isEmpty() && verifyPassword(record, bytes); std::fill(bytes.begin(), bytes.end(), '\0');
        return QVariantMap{{"ok", matches}, {"wrong", found && error.isEmpty() && !matches}, {"found", found}, {"unavailable", !error.isEmpty()},
            {"message", !error.isEmpty() ? QCoreApplication::translate("HyprEndfield", "无法读取系统密钥服务：") + error : !found ? QCoreApplication::translate("HyprEndfield", "这份档案的密码记录已经丢失。") : QString()}};
    }));
}

void ArchiveController::declassifySelected()
{
    const auto *entry = selectedEntry(); if (!entry || entry->keyId.isEmpty() || m_unlockedId != entry->id || m_busy) return;
    const QString id = entry->id, account = classificationAccount(*entry); setBusy(true);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id] {
        const auto result = watcher->result(); watcher->deleteLater(); setBusy(false);
        if (!result.value("ok").toBool()) { fail("declassify", result.value("message").toString()); return; }
        for (auto &value : m_entries) if (value.id == id) { value.keyId.clear(); value.modified = QDateTime::currentMSecsSinceEpoch(); saveEntry(value); break; }
        m_passwordStatuses.remove(id); m_unlockedId.clear(); updateSchemaVersion(); emit stateChanged(); emit actionFinished("declassify", true, QCoreApplication::translate("HyprEndfield", "已取消机密。"));
    });
    watcher->setFuture(QtConcurrent::run([vault = m_vault, account] { QString error; const bool ok = vault->remove(account, &error); return QVariantMap{{"ok", ok}, {"message", error}}; }));
}

void ArchiveController::authenticateOwner(const QString &linuxPassword)
{
    const auto *entry = selectedEntry(); if (!entry || entry->keyId.isEmpty() || m_busy) return;
    m_ownerAuthorizedId.clear();
    const QString id = entry->id; QByteArray password = linuxPassword.toUtf8(); setBusy(true);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id] {
        const auto result = watcher->result(); watcher->deleteLater(); setBusy(false);
        const bool ok = result.value("ok").toBool() && m_selectedId == id;
        if (ok) m_ownerAuthorizedId = id;
        emit ownerAuthenticationFinished(ok, ok ? QString() : QCoreApplication::translate("HyprEndfield", "Linux 用户认证失败：") + result.value("message").toString());
    });
    watcher->setFuture(QtConcurrent::run([password]() mutable { return authenticateWithPam(password); }));
}

void ArchiveController::cancelRecovery() { m_ownerAuthorizedId.clear(); }

void ArchiveController::recoverSelected(const QString &newPassword, const QString &confirmation, bool declassify)
{
    const auto *entry = selectedEntry(); if (!entry || entry->id != m_ownerAuthorizedId || entry->keyId.isEmpty() || m_busy) return;
    QString validation;
    if (!declassify && !validPassword(newPassword, confirmation, &validation)) { emit recoveryFinished(false, validation); return; }
    const QString id = entry->id, oldAccount = classificationAccount(*entry), newKey = declassify ? QString() : uuid();
    const QString newAccount = id + '/' + newKey; QByteArray bytes = newPassword.normalized(QString::NormalizationForm_C).toUtf8(); setBusy(true); m_ownerAuthorizedId.clear();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, id, newKey, declassify] {
        const auto result = watcher->result(); watcher->deleteLater(); setBusy(false);
        if (!result.value("ok").toBool()) { emit recoveryFinished(false, result.value("message").toString()); return; }
        for (auto &value : m_entries) if (value.id == id) { value.keyId = newKey; value.modified = QDateTime::currentMSecsSinceEpoch(); saveEntry(value); break; }
        if (declassify) m_passwordStatuses.remove(id); else m_passwordStatuses.insert(id, QStringLiteral("protected"));
        m_unlockedId = declassify ? QString() : id; updateSchemaVersion(); emit stateChanged(); emit recoveryFinished(true, declassify ? QCoreApplication::translate("HyprEndfield", "已取消机密。") : QCoreApplication::translate("HyprEndfield", "密码已重新设置。"));
    });
    watcher->setFuture(QtConcurrent::run([vault = m_vault, oldAccount, newAccount, bytes, declassify]() mutable {
        QString error; bool ok = true;
        if (!declassify) { const QByteArray record = verifier(bytes); ok = vault->add(newAccount, record, &error); }
        std::fill(bytes.begin(), bytes.end(), '\0');
        if (ok) { QString ignored; vault->remove(oldAccount, &ignored); }
        return QVariantMap{{"ok", ok}, {"message", ok ? QString() : QCoreApplication::translate("HyprEndfield", "无法保存新密码：") + error}};
    }));
}

QVariantMap ArchiveController::formatState(QObject *quickDocument, int position) const
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(quickDocument); if (!wrapper || !wrapper->textDocument()) return {};
    QTextCursor cursor(wrapper->textDocument()); cursor.setPosition(std::clamp(position, 0, wrapper->textDocument()->characterCount() - 1));
    const QTextCharFormat format = cursor.charFormat();
    const QStringList families = format.fontFamilies().toStringList();
    return {{"family", families.value(0, QStringLiteral("HarmonyOS Sans"))},
        {"size", format.fontPointSize() > 0 ? format.fontPointSize() : 14}, {"color", format.foreground().color().name().toUpper()},
        {"bold", format.fontWeight() >= QFont::Bold}, {"italic", format.fontItalic()}, {"underline", format.fontUnderline()}, {"strikeout", format.fontStrikeOut()}};
}

bool ArchiveController::applyFormat(QObject *quickDocument, int start, int end, const QString &key, const QVariant &value)
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(quickDocument); if (!wrapper || !wrapper->textDocument()) return false;
    QTextDocument *document = wrapper->textDocument(); QTextCursor cursor(document);
    const int upper = std::max(0, document->characterCount() - 1);
    start = std::clamp(start, 0, upper); end = std::clamp(end, 0, upper);
    if (start == end) { cursor.select(QTextCursor::Document); } else { cursor.setPosition(std::min(start, end)); cursor.setPosition(std::max(start, end), QTextCursor::KeepAnchor); }
    QTextCharFormat format;
    if (key == "family") format.setFontFamilies({value.toString()});
    else if (key == "size") format.setFontPointSize(std::clamp(value.toDouble(), 6.0, 144.0));
    else if (key == "color") format.setForeground(QColor(value.toString()));
    else if (key == "bold") format.setFontWeight(value.toBool() ? QFont::Bold : QFont::Normal);
    else if (key == "italic") format.setFontItalic(value.toBool());
    else if (key == "underline") format.setFontUnderline(value.toBool());
    else if (key == "strikeout") format.setFontStrikeOut(value.toBool());
    else return false;
    cursor.mergeCharFormat(format); return true;
}
