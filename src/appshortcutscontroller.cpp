#include "appshortcutscontroller.hpp"
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QTextBoundaryFinder>
#include <QTimer>
#include <QUuid>
#pragma push_macro("signals")
#undef signals
#include <gio/gdesktopappinfo.h>
#pragma pop_macro("signals")

namespace {
constexpr qint64 maximumBytes = 2 * 1024 * 1024;
const QStringList presetIDs{"original", "bolt", "star", "terminal", "globe", "folder", "music", "play", "brush", "code", "game", "camera", "grid", "textBubble"};
QJsonArray presetData() {
    static const auto data = [] { QFile file(":/assets/app-shortcut/artwork.json"); if (!file.open(QIODevice::ReadOnly)) return QJsonArray(); return QJsonDocument::fromJson(file.readAll()).object()["presets"].toArray(); }();
    return data;
}
QColor averageColor(QImage image) {
    if (image.isNull()) return QColor("#2A2A2A");
    image = image.scaled(8, 8, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    double r = 0, g = 0, b = 0, a = 0;
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) { const auto pixel = image.pixel(x, y); r += qRed(pixel); g += qGreen(pixel); b += qBlue(pixel); a += qAlpha(pixel); }
    if (!a) return QColor("#2A2A2A");
    r = r / a * .72; g = g / a * .72; b = b / a * .72;
    const double luminance = .2126 * r + .7152 * g + .0722 * b;
    if (luminance > .55) { r *= .55 / luminance; g *= .55 / luminance; b *= .55 / luminance; }
    return QColor::fromRgbF(r, g, b);
}
}
AppShortcutsController::AppShortcutsController(QObject *parent, QString directory, TestLauncher launcher)
    : QObject(parent), m_directory(directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/AppShortcuts" : directory),
      m_path(m_directory + "/shortcuts.json"), m_testLauncher(std::move(launcher)) { load(); refresh(); }
QString AppShortcutsController::normalizeName(const QString &value) {
    const auto text = value.trimmed();
    for (const auto c : text) if (c.category() == QChar::Other_Control) return {};
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int count = 0, end = 0;
    for (int next = finder.toNextBoundary(); next >= 0 && count < 24; next = finder.toNextBoundary(), ++count) end = next;
    return text.left(end);
}
std::optional<AppShortcutsController::App> AppShortcutsController::inspect(const QString &path) const {
    const QFileInfo info(path);
    if (info.suffix().compare("desktop", Qt::CaseInsensitive) || !info.isFile() || !info.isReadable() || info.size() > 1024 * 1024) return {};
    auto *desktop = g_desktop_app_info_new_from_filename(QFile::encodeName(info.canonicalFilePath()).constData());
    if (!desktop) return {};
    App app;
    app.name = QString::fromUtf8(g_app_info_get_display_name(G_APP_INFO(desktop)));
    app.executable = QString::fromUtf8(g_app_info_get_executable(G_APP_INFO(desktop)));
    char *type = g_desktop_app_info_get_string(desktop, "Type");
    const bool valid = type && QByteArray(type) == "Application" && !g_desktop_app_info_get_boolean(desktop, "Hidden") && !app.name.isEmpty() && !app.executable.isEmpty();
    g_free(type);
    if (GIcon *icon = g_app_info_get_icon(G_APP_INFO(desktop))) {
        QIcon qtIcon;
        if (G_IS_THEMED_ICON(icon)) {
            const auto names = g_themed_icon_get_names(G_THEMED_ICON(icon));
            for (int i = 0; names && names[i] && qtIcon.isNull(); ++i) qtIcon = QIcon::fromTheme(QString::fromUtf8(names[i]));
        } else if (G_IS_FILE_ICON(icon)) {
            char *file = g_file_get_path(g_file_icon_get_file(G_FILE_ICON(icon))); if (file) qtIcon = QIcon(QString::fromUtf8(file)); g_free(file);
        }
        app.icon = qtIcon.pixmap(96, 96).toImage(); app.color = averageColor(app.icon);
    }
    g_object_unref(desktop);
    return valid ? std::optional(app) : std::nullopt;
}
void AppShortcutsController::load() {
    QFile file(m_path); if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumBytes) { m_readable = false; m_error = tr("无法读取应用快捷方式，原始数据已保留。"); return; }
    const auto bytes = file.readAll(); const auto document = QJsonDocument::fromJson(bytes);
    const auto object = document.object();
    if (document.isNull() || object["version"].toInt() != 1 || !object["items"].isArray()) { m_readable = false; m_error = tr("无法读取应用快捷方式，原始数据已保留。"); return; }
    QSet<QString> ids, paths;
    for (const auto &value : object["items"].toArray()) {
        const auto row = value.toObject();
        Entry entry{row["id"].toString(), row["name"].toString(), row["originalName"].toString(), row["path"].toString(), row["executable"].toString(), row["iconPreset"].toString(), qint64(row["created"].toDouble())};
        if (QUuid(entry.id).isNull() || ids.contains(entry.id) || paths.contains(entry.path) || entry.name.isEmpty() || entry.name != normalizeName(entry.name)
            || !QFileInfo(entry.path).isAbsolute() || !entry.path.endsWith(".desktop", Qt::CaseInsensitive) || entry.executable.isEmpty() || !presetIDs.contains(entry.iconPreset)) {
            m_entries.clear(); m_readable = false; m_error = tr("无法读取应用快捷方式，原始数据已保留。"); return;
        }
        ids.insert(entry.id); paths.insert(entry.path); m_entries.append(entry);
    }
    m_saved = bytes;
}
bool AppShortcutsController::fail(const QString &message) { m_error = message; emit changed(); return false; }
bool AppShortcutsController::commit(const QList<Entry> &next) {
    if (!m_readable) return false;
    QFile current(m_path);
    if (current.exists() && (!current.open(QIODevice::ReadOnly) || current.size() > maximumBytes)) return fail(tr("无法读取应用快捷方式，原始数据已保留。"));
    if ((current.isOpen() ? current.readAll() : QByteArray()) != m_saved) return fail(tr("应用快捷方式已在外部更改，请重启 HyprEndfield 后再编辑。"));
    QJsonArray rows;
    for (const auto &entry : next) rows.append(QJsonObject{{"id", entry.id}, {"name", entry.name}, {"originalName", entry.originalName}, {"path", entry.path},
        {"executable", entry.executable}, {"iconPreset", entry.iconPreset}, {"created", entry.created}});
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"items", rows}}).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumBytes || !QDir().mkpath(m_directory)) return fail(tr("无法保存应用快捷方式。"));
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) return fail(tr("无法保存应用快捷方式。"));
    m_entries = next; m_saved = bytes; m_error.clear(); emit changed(); return true;
}
QVariantMap AppShortcutsController::state() const {
    QVariantList entries;
    for (const auto &entry : m_entries) entries.append(QVariantMap{{"id", entry.id}, {"name", entry.name}, {"iconPreset", entry.iconPreset},
        {"available", m_apps.contains(entry.id)}, {"icon", "image://shortcut-icons/" + entry.id + '/' + entry.iconPreset}});
    return {{"items", entries}, {"count", entries.size()}, {"error", m_error}, {"available", m_readable}, {"launching", m_launching}};
}
QVariantList AppShortcutsController::presets() const {
    QVariantList result;
    for (const auto &value : presetData()) { const auto entry = value.toObject(); result.append(QVariantMap{{"id", entry["id"].toString()}, {"title", tr(qPrintable(entry["simplifiedChinese"].toString()))}}); }
    return result;
}
void AppShortcutsController::refresh() {
    m_apps.clear();
    for (const auto &entry : m_entries) if (const auto app = inspect(entry.path); app && app->executable == entry.executable) m_apps.insert(entry.id, *app);
    emit changed();
}
bool AppShortcutsController::add(const QUrl &url) {
    if (!url.isLocalFile()) return fail(tr("请选择可用的 Linux 应用快捷方式（.desktop）。"));
    const auto path = QFileInfo(url.toLocalFile()).canonicalFilePath(); const auto app = inspect(path);
    if (!app) return fail(tr("请选择可用的 Linux 应用快捷方式（.desktop）。"));
    for (const auto &entry : m_entries) if (entry.path == path) return fail(tr("此应用已添加，请编辑现有快捷方式。"));
    Entry entry{QUuid::createUuid().toString(QUuid::WithoutBraces), normalizeName(app->name), app->name, path, app->executable, "original", QDateTime::currentMSecsSinceEpoch()};
    if (entry.name.isEmpty()) return fail(tr("请输入有效的快捷方式名称。"));
    auto next = m_entries; next.append(entry);
    if (!commit(next)) return false;
    m_apps.insert(entry.id, *app); emit changed(); emit added(entry.id); return true;
}
bool AppShortcutsController::rename(const QString &id, const QString &name) {
    const auto normalized = normalizeName(name); if (normalized.isEmpty()) return fail(tr("请输入有效的快捷方式名称。"));
    auto next = m_entries;
    for (auto &entry : next) if (entry.id == id) { if (entry.name == normalized) return true; entry.name = normalized; return commit(next); }
    return false;
}
bool AppShortcutsController::setIcon(const QString &id, const QString &preset) {
    if (!presetIDs.contains(preset)) return false;
    auto next = m_entries;
    for (auto &entry : next) if (entry.id == id) { if (entry.iconPreset == preset) return true; entry.iconPreset = preset; return commit(next); }
    return false;
}
bool AppShortcutsController::remove(const QString &id) {
    auto next = m_entries; if (!next.removeIf([&](const Entry &entry) { return entry.id == id; })) return false;
    if (!commit(next)) return false;
    m_apps.remove(id); return true;
}
void AppShortcutsController::paste() {
    // Match the shelf's asynchronous URI-list read; never block the UI waiting for another Wayland client.
    auto *process = new QProcess(this); auto *timeout = new QTimer(process); timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) process->deleteLater(); });
    connect(process, &QProcess::readyReadStandardOutput, this, [process] { if (process->bytesAvailable() > 1024 * 1024) process->kill(); });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, process, timeout](int code, QProcess::ExitStatus status) {
        timeout->stop(); const auto bytes = process->readAllStandardOutput(); process->deleteLater();
        if (code != 0 || status != QProcess::NormalExit || bytes.size() > 1024 * 1024) return;
        QList<QUrl> urls;
        for (const auto &line : bytes.split('\n')) if (!line.trimmed().isEmpty() && !line.startsWith('#')) urls.append(QUrl::fromEncoded(line.trimmed()));
        if (urls.size() == 1) add(urls.first());
    });
    process->start("wl-paste", {"--type", "text/uri-list", "--no-newline"}); timeout->start(3000);
}
bool AppShortcutsController::requestOpen(const QString &id) {
    if (m_launching) return false;
    for (const auto &entry : m_entries) if (entry.id == id) { m_pending = id; emit launchRequested(); return true; }
    return false;
}
void AppShortcutsController::launchPending() {
    if (m_pending.isEmpty() || m_launching) return;
    const auto id = std::exchange(m_pending, {});
    for (const auto &entry : m_entries) if (entry.id == id) {
        const auto app = inspect(entry.path);
        if (!app || app->executable != entry.executable) { fail(tr("应用已移动、移除或更改，请重新选择应用。")); emit launchFailed(); return; }
        const auto done = [this, name = entry.name](bool success, const QString &error) {
            m_launching = false;
            if (success) { m_error.clear(); emit changed(); emit opened(name); }
            else { fail(tr("无法打开应用：") + error.left(500)); emit launchFailed(); }
        };
        if (m_testLauncher) { const auto error = m_testLauncher(entry.path); done(error.isEmpty(), error); return; }
        auto *process = new QProcess(this); auto *timeout = new QTimer(process); timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, process, &QProcess::kill);
        connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) { done(false, process->errorString()); process->deleteLater(); }
        });
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [process, timeout, done](int code, QProcess::ExitStatus status) {
            timeout->stop(); done(code == 0 && status == QProcess::NormalExit, QString::fromUtf8(process->readAllStandardError()).trimmed()); process->deleteLater();
        });
        m_launching = true; emit changed(); process->start("gio", {"launch", entry.path}); timeout->start(15000); return;
    }
}
QImage AppShortcutsController::artwork(const QString &id, const QString &preset, QColor accent, QSize size) const {
    if (size.isEmpty()) size = {80, 80}; size = size.boundedTo({512, 512});
    QImage result(size, QImage::Format_ARGB32_Premultiplied); result.fill(Qt::transparent);
    QPainter painter(&result); painter.setRenderHint(QPainter::SmoothPixmapTransform); painter.scale(size.width() / 40., size.height() / 40.);
    const auto app = m_apps.value(id); const bool original = preset == "original";
    painter.fillRect(QRectF(0, 0, 40, 40), original ? app.color : accent);
    if (original && !app.icon.isNull()) {
        const auto target = QSizeF(app.icon.size()).scaled({30, 30}, Qt::KeepAspectRatio);
        painter.drawImage(QRectF((40 - target.width()) / 2, (40 - target.height()) / 2, target.width(), target.height()), app.icon);
    } else for (const auto &value : presetData()) {
        const auto data = value.toObject(); if (data["id"].toString() != preset) continue;
        const auto frame = data["frame"].toArray();
        QImage icon = QImage(":/assets/app-shortcut/" + data["file"].toString()).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QPainter tint(&icon); tint.setCompositionMode(QPainter::CompositionMode_SourceIn); tint.fillRect(icon.rect(), original ? QColor("#EDEDED") : QColor("#111111")); tint.end();
        painter.drawImage(QRectF(frame[0].toDouble(), frame[1].toDouble(), frame[2].toDouble(), frame[3].toDouble()), icon); break;
    }
    painter.setPen(QColor(0, 0, 0, 64)); painter.drawRect(QRectF(.5, .5, 39, 39));
    return result;
}
