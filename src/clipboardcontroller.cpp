#include "clipboardcontroller.hpp"

#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <memory>

namespace {
constexpr qsizetype maximumSnapshotBytes = 4 * 1024 * 1024;
constexpr qsizetype maximumImageBytes = 64 * 1024 * 1024;

QVariantMap emptyState()
{
    return {{"ready", false}, {"total", 0}, {"capacity", 0}, {"removable", 0},
            {"items", QVariantList{}}, {"status", QStringLiteral("正在连接 CopyQ")}};
}

QString imageFormat(const QString &mime)
{
    QString value = mime.section('/', 1, 1).toUpper();
    if (value == QStringLiteral("JPG")) value = QStringLiteral("JPEG");
    return value;
}

QString fileSize(qint64 bytes)
{
    if (bytes < 1000) return QStringLiteral("%1 字节").arg(bytes);
    static const QStringList units{QStringLiteral("KB"), QStringLiteral("MB"),
                                   QStringLiteral("GB"), QStringLiteral("TB")};
    double value = bytes;
    int unit = -1;
    do { value /= 1000.0; ++unit; } while (value >= 1000.0 && unit + 1 < units.size());
    const int decimals = unit == 0 ? 0 : unit == 1 ? 1 : 2;
    return QString::number(value, 'f', decimals).remove(QRegularExpression(QStringLiteral("\\.?0+$")))
        + QLatin1Char(' ') + units.at(unit);
}

QString fileMeta(const QVariantList &uris)
{
    if (uris.isEmpty()) return {};
    if (uris.size() > 1) return QStringLiteral("%1 项").arg(uris.size());
    const QFileInfo info(QUrl(uris.constFirst().toString()).toLocalFile());
    return info.isFile() ? fileSize(info.size()) : QStringLiteral("文件夹");
}
}

ClipboardController::ClipboardController(QObject *parent, QString executable, bool automatic)
    : QObject(parent), m_executable(std::move(executable)), m_automatic(automatic), m_state(emptyState())
{
    if (m_executable.isEmpty()) m_executable = QStandardPaths::findExecutable(QStringLiteral("copyq"));
    m_timer.setInterval(3000);
    connect(&m_timer, &QTimer::timeout, this, &ClipboardController::requestRefresh);
    connect(&m_refreshProcess, &QProcess::readyReadStandardOutput, this, [this] {
        m_refreshOutput += m_refreshProcess.readAllStandardOutput();
        if (m_refreshOutput.size() > maximumSnapshotBytes) m_refreshProcess.kill();
    });
    connect(&m_refreshProcess, &QProcess::finished, this, &ClipboardController::finishRefresh);
    if (m_automatic) {
        m_timer.start();
        QTimer::singleShot(0, this, &ClipboardController::requestRefresh);
    }
}

ClipboardController::~ClipboardController()
{
    m_timer.stop();
    if (m_refreshProcess.state() != QProcess::NotRunning) {
        m_refreshProcess.kill();
        m_refreshProcess.waitForFinished(1000);
    }
    if (m_thumbnailProcess && m_thumbnailProcess->state() != QProcess::NotRunning) {
        m_thumbnailProcess->kill();
        m_thumbnailProcess->waitForFinished(1000);
    }
}

void ClipboardController::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    m_timer.setInterval(active ? 1000 : 3000);
    emit activeChanged();
    if (active) {
        requestRefresh();
        requestNextThumbnail();
    }
}

QVariantMap ClipboardController::state() const
{
    QVariantMap result = m_state;
    QVariantList items;
    items.reserve(m_items.size());
    for (const auto &value : m_items) {
        auto entry = value.toMap();
        const QString id = entry.value("id").toString();
        if (entry.value("kind").toString() == QStringLiteral("image")) {
            QString meta = entry.value("format").toString();
            if (const QSize size = m_imageSizes.value(id); size.isValid())
                meta += QStringLiteral("   %1 × %2").arg(size.width()).arg(size.height());
            entry["meta"] = meta;
            if (m_thumbnails.contains(id)) entry["thumbnail"] = m_thumbnails.value(id);
        }
        items.append(entry);
    }
    result["items"] = items;
    return result;
}

void ClipboardController::requestRefresh()
{
    if (!m_automatic || m_executable.isEmpty()) return;
    if (m_refreshProcess.state() != QProcess::NotRunning) { m_refreshPending = true; return; }
    startRefresh();
}

void ClipboardController::startRefresh()
{
    m_refreshOutput.clear();
    m_refreshProcess.start(m_executable, {QStringLiteral("eval"), snapshotScript()});
}

void ClipboardController::finishRefresh(int exitCode, QProcess::ExitStatus status)
{
    m_refreshOutput += m_refreshProcess.readAllStandardOutput();
    const bool ok = status == QProcess::NormalExit && exitCode == 0
        && m_refreshOutput.size() <= maximumSnapshotBytes && acceptSnapshot(m_refreshOutput);
    if (!ok) {
        const QVariantMap before = state();
        m_state["ready"] = false;
        m_state["status"] = QStringLiteral("CopyQ 暂时无法读取");
        if (before != state()) emit stateChanged();
    }
    if (m_refreshPending) { m_refreshPending = false; QTimer::singleShot(0, this, &ClipboardController::startRefresh); }
}

bool ClipboardController::acceptSnapshot(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const QVariantMap source = document.object().toVariantMap();
    bool totalOk = false, capacityOk = false, removableOk = false;
    const int total = source.value("total").toInt(&totalOk);
    const int capacity = source.value("capacity").toInt(&capacityOk);
    const int removable = source.value("removable").toInt(&removableOk);
    if (!totalOk || !capacityOk || !removableOk || total < 0 || capacity < 1
        || removable < 0 || removable > total || !source.value("items").canConvert<QVariantList>()) return false;

    QVariantList parsed;
    for (const auto &value : source.value("items").toList()) {
        auto entry = value.toMap();
        const QString id = entry.value("id").toString();
        const QString kind = entry.value("kind").toString();
        const int row = entry.value("row").toInt();
        if (id.isEmpty() || row < 0 || !QStringList{"text", "url", "image", "files"}.contains(kind)) return false;
        const QString created = entry.take("createdAt").toString();
        const QDateTime time = QDateTime::fromString(created, Qt::ISODateWithMs);
        entry["createdAt"] = time.isValid() ? time.toMSecsSinceEpoch() : 0;
        QString meta;
        if (kind == QStringLiteral("text")) meta = QStringLiteral("%1 字符").arg(entry.value("characters").toLongLong());
        else if (kind == QStringLiteral("url")) meta = QUrl(entry.value("preview").toString().trimmed()).host();
        else if (kind == QStringLiteral("image")) {
            const QString format = imageFormat(entry.value("imageMime").toString());
            entry["format"] = format;
            meta = format;
        } else {
            const QVariantList uris = entry.take("fileUris").toList();
            entry["fileCount"] = uris.size();
            meta = fileMeta(uris);
        }
        entry["meta"] = meta;
        parsed.append(entry);
    }

    const QVariantMap before = state();
    if (m_initialised && m_automatic && !m_executable.isEmpty())
        stampNewItems(parsed, m_lastTotal, total);
    else
        m_initialised = true;
    for (const auto &value : parsed) m_seenIds.insert(value.toMap().value("id").toString());
    m_lastTotal = total;
    m_items = std::move(parsed);
    m_state = {{"ready", true}, {"total", total}, {"capacity", capacity},
               {"removable", removable}, {"status", QString()}};
    if (before != state()) emit stateChanged();
    requestNextThumbnail();
    return true;
}

QVariantMap ClipboardController::item(const QString &id) const
{
    for (const auto &value : m_items) {
        const auto entry = value.toMap();
        if (entry.value("id").toString() == id) return entry;
    }
    return {};
}

QString ClipboardController::jsString(const QString &value)
{
    QByteArray json = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

QString ClipboardController::mutationPrefix(const QString &id, int row)
{
    return QStringLiteral(R"JS(
const timeMime = 'application/x-hyprendfield-copy-time';
const wanted = %1;
const hint = %2;
function stableId(item) {
    const clean = {};
    const keys = Object.keys(item).sort();
    for (let index = 0; index < keys.length; ++index) {
        const key = keys[index];
        if (key !== timeMime) clean[key] = item[key];
    }
    return str(toBase64(sha256sum(pack(clean))));
}
function findRow() {
    if (hint >= 0 && hint < size() && stableId(getItem(hint)) === wanted) return hint;
    const maximum = Math.min(size(), 512);
    for (let row = 0; row < maximum; ++row)
        if (stableId(getItem(row)) === wanted) return row;
    return -1;
}
)JS").arg(jsString(id)).arg(row);
}

void ClipboardController::startMutation(const QString &action, const QString &script, bool refresh)
{
    if (m_executable.isEmpty()) {
        if (!action.isEmpty()) emit actionFinished(action, false, QStringLiteral("CopyQ 暂时无法使用"));
        return;
    }
    auto *process = new QProcess(this);
    auto output = std::make_shared<QByteArray>();
    connect(process, &QProcess::readyReadStandardOutput, process, [process, output] { *output += process->readAllStandardOutput(); });
    connect(process, &QProcess::finished, this,
            [this, process, output, action, refresh](int exitCode, QProcess::ExitStatus status) {
        *output += process->readAllStandardOutput();
        const bool success = status == QProcess::NormalExit && exitCode == 0 && output->trimmed() == QByteArrayLiteral("1");
        if (!action.isEmpty()) {
            const QString message = success ? QString() : QStringLiteral("CopyQ 没有完成此操作");
            emit actionFinished(action, success, message);
        }
        process->deleteLater();
        if (refresh) QTimer::singleShot(40, this, &ClipboardController::requestRefresh);
    });
    process->start(m_executable, {QStringLiteral("eval"), script});
}

bool ClipboardController::copyItem(const QString &id)
{
    const auto entry = item(id); if (entry.isEmpty()) return false;
    QString script = mutationPrefix(id, entry.value("row").toInt());
    script += QStringLiteral("const row = findRow(); if (row < 0) print('0'); else { copy(getItem(row)); print('1'); }");
    startMutation(QStringLiteral("copy"), script, false); return true;
}

bool ClipboardController::togglePin(const QString &id)
{
    const auto entry = item(id); if (entry.isEmpty()) return false;
    QString script = mutationPrefix(id, entry.value("row").toInt());
    script += QStringLiteral("const row = findRow(); if (row < 0) print('0'); else { if (plugins.itempinned.isPinned(row)) plugins.itempinned.unpin(row); else plugins.itempinned.pin(row); print('1'); }");
    startMutation(QStringLiteral("pin"), script); return true;
}

bool ClipboardController::removeItem(const QString &id)
{
    const auto entry = item(id); if (entry.isEmpty() || entry.value("pinned").toBool()) return false;
    QString script = mutationPrefix(id, entry.value("row").toInt());
    script += QStringLiteral("const row = findRow(); if (row < 0 || plugins.itempinned.isPinned(row)) print('0'); else { remove(row); print('1'); }");
    startMutation(QStringLiteral("remove"), script); return true;
}

bool ClipboardController::clearUnpinned()
{
    if (m_state.value("removable").toInt() <= 0) return false;
    startMutation(QStringLiteral("clear"), QStringLiteral("ItemSelection().selectRemovable().removeAll(); print('1');"));
    return true;
}

void ClipboardController::stampNewItems(const QVariantList &items, int previousTotal, int currentTotal)
{
    int remaining = std::max(0, currentTotal - previousTotal);
    if (!remaining) return;
    for (const auto &value : items) {
        const auto entry = value.toMap();
        const QString id = entry.value("id").toString();
        if (m_seenIds.contains(id) || entry.value("createdAt").toLongLong() > 0) continue;
        QString script = mutationPrefix(id, entry.value("row").toInt());
        script += QStringLiteral("const row = findRow(); if (row < 0 || getItem(row)[timeMime]) print('0'); else { change(row, timeMime, %1); print('1'); }")
            .arg(jsString(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)));
        startMutation({}, script);
        if (--remaining == 0) break;
    }
}

void ClipboardController::requestNextThumbnail()
{
    if (!m_active || m_thumbnailProcess) return;
    for (const auto &value : m_items) {
        const auto entry = value.toMap();
        const QString id = entry.value("id").toString();
        if (entry.value("kind").toString() != QStringLiteral("image") || m_thumbnails.contains(id)
            || m_thumbnailFailures.contains(id) || m_thumbnailPending.contains(id)) continue;
        startThumbnail(entry); return;
    }
}

void ClipboardController::startThumbnail(const QVariantMap &entry)
{
    const QString id = entry.value("id").toString();
    const QString mime = entry.value("imageMime").toString();
    if (id.isEmpty() || mime.isEmpty()) return;
    m_thumbnailId = id;
    m_thumbnailOutput.clear();
    m_thumbnailPending.insert(id);
    m_thumbnailProcess = new QProcess(this);
    QString script = mutationPrefix(id, entry.value("row").toInt());
    script += QStringLiteral("const row = findRow(); if (row >= 0) print(getItem(row)[%1]);").arg(jsString(mime));
    connect(m_thumbnailProcess, &QProcess::readyReadStandardOutput, this, [this] {
        m_thumbnailOutput += m_thumbnailProcess->readAllStandardOutput();
        if (m_thumbnailOutput.size() > maximumImageBytes) m_thumbnailProcess->kill();
    });
    connect(m_thumbnailProcess, &QProcess::finished, this, &ClipboardController::finishThumbnail);
    m_thumbnailProcess->start(m_executable, {QStringLiteral("eval"), script});
}

void ClipboardController::finishThumbnail(int exitCode, QProcess::ExitStatus status)
{
    QProcess *process = m_thumbnailProcess;
    m_thumbnailOutput += process->readAllStandardOutput();
    const QString id = m_thumbnailId;
    m_thumbnailPending.remove(id);
    QImage image;
    if (status == QProcess::NormalExit && exitCode == 0 && m_thumbnailOutput.size() <= maximumImageBytes)
        image.loadFromData(m_thumbnailOutput);
    if (image.isNull()) {
        m_thumbnailFailures.insert(id);
    } else {
        m_imageSizes[id] = image.size();
        const QImage thumb = image.scaled(QSize(128, 96), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); thumb.save(&buffer, "PNG");
        m_thumbnails[id] = QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
        emit stateChanged();
    }
    m_thumbnailProcess = nullptr;
    m_thumbnailId.clear();
    m_thumbnailOutput.clear();
    process->deleteLater();
    QTimer::singleShot(0, this, &ClipboardController::requestNextThumbnail);
}

QString ClipboardController::snapshotScript()
{
    return QStringLiteral(R"JS(
const timeMime = 'application/x-hyprendfield-copy-time';
const uriMime = 'text/uri-list';
const maximum = 64;
function stableId(item) {
    const clean = {};
    const keys = Object.keys(item).sort();
    for (let index = 0; index < keys.length; ++index) {
        const key = keys[index];
        if (key !== timeMime) clean[key] = item[key];
    }
    return str(toBase64(sha256sum(pack(clean))));
}
function flattened(value) { return str(value || '').replace(/\s+/g, ' ').trim(); }
function compact(value) {
    const characters = Array.from(value);
    return characters.length > 90 ? characters.slice(0, 89).join('') + '…' : value;
}
function uriLines(item) {
    if (!item[uriMime]) return [];
    return str(item[uriMime]).split(/\r?\n/).filter(line => line && line[0] !== '#');
}
function looksLikeLink(value) {
    const text = value.trim();
    return !/\s/.test(text) && /^(https?|ftp|mailto|tel|sms):/i.test(text);
}
const total = size();
const removable = ItemSelection().selectRemovable().length;
const items = [];
for (let row = 0; row < Math.min(total, maximum); ++row) {
    const item = getItem(row);
    const formats = Object.keys(item);
    const imageMime = formats.find(format => /^image\/(png|jpeg|jpg|tiff|bmp|webp)$/i.test(format)) || '';
    const uris = uriLines(item);
    const files = uris.filter(uri => /^file:/i.test(uri));
    const plain = str(item['text/plain'] || '');
    let kind = 'text';
    let preview = flattened(plain);
    let fileUris = [];
    if (files.length > 0) {
        kind = 'files'; fileUris = files;
        const names = files.slice(0, 3).map(uri => {
            const path = decodeURIComponent(uri.replace(/^file:\/\//i, ''));
            const pieces = path.replace(/\/$/, '').split('/');
            return pieces[pieces.length - 1] || path;
        });
        preview = names.join(', ') + (files.length > 3 ? ' (+' + (files.length - 3) + ')' : '');
    } else if (imageMime) {
        kind = 'image'; preview = '图片';
    } else if (uris.length > 0 || looksLikeLink(plain)) {
        kind = 'url'; preview = flattened(plain || uris[0]);
    }
    items.push({id: stableId(item), row: row, kind: kind, preview: compact(preview),
        characters: kind === 'text' ? Array.from(plain).length : 0,
        imageMime: imageMime, fileUris: fileUris, createdAt: str(item[timeMime] || ''),
        pinned: plugins.itempinned.isPinned(row)});
}
print(JSON.stringify({total: total, capacity: Number(config('maxitems')) || total,
    removable: removable, items: items}));
)JS");
}
