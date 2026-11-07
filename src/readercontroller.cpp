#include <QCoreApplication>
#include "readercontroller.hpp"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QTextLayout>
#include <QTextOption>
#include <QTransform>
#include <QUuid>
#include <QXmlStreamReader>
#include <QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <iconv.h>
#include <limits>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <zip.h>

namespace {
using Location = ReaderController::Location;
using Preferences = ReaderController::Preferences;
using Document = ReaderController::Document;
using Page = ReaderController::Page;

constexpr qsizetype maximumLibraryBytes = 4 * 1024 * 1024;
constexpr qsizetype maximumTextBytes = 32 * 1024 * 1024;
constexpr qint64 maximumPdfBytes = qint64(1024) * 1024 * 1024;
constexpr qint64 maximumEpubBytes = qint64(512) * 1024 * 1024;
constexpr quint64 maximumInflatedBytes = quint64(512) * 1024 * 1024;
constexpr quint64 maximumEntryBytes = quint64(32) * 1024 * 1024;
constexpr int maximumZipEntries = 8192;
constexpr int imageWidth = 1260;
constexpr int imageHeight = 748;
constexpr double logicalWidth = 420.0;
constexpr double logicalHeight = 349.0 / 1.4;

QString unsupportedMessage() { return QCoreApplication::translate("HyprEndfield", "请选择 PDF、EPUB 或 TXT 书籍。"); }
QString invalidArchiveMessage() { return QCoreApplication::translate("HyprEndfield", "此 EPUB 压缩包已损坏或不安全。"); }
QString tooLargeMessage() { return QCoreApplication::translate("HyprEndfield", "此书籍超过阅读器的容量限制。"); }
QString invalidBookMessage() { return QCoreApplication::translate("HyprEndfield", "无法读取此书籍。"); }
QString unavailableMessage() { return QCoreApplication::translate("HyprEndfield", "原始书籍不可用，请重新选择。"); }
QString encryptedMessage() { return QCoreApplication::translate("HyprEndfield", "暂不支持加密书籍。"); }

bool finiteFraction(double value)
{
    return std::isfinite(value) && value >= 0 && value <= 1;
}

ReaderController::OpenResult openDocument(const QString &path);
QString locationKey(const Location &location);
QJsonObject locationJson(const Location &location);
Location locationFromJson(const QJsonValue &value, bool *ok);
QVariantMap locationMap(const Location &location);
bool supportedPath(const QString &path);
bool readIdentity(const QString &path, QString *device, QString *inode, QString *error,
                  const QString &expectedDevice = {}, const QString &expectedInode = {});
ReaderController::RenderResult renderDocument(const std::shared_ptr<Document> &document,
                                               const Preferences &preferences,
                                               std::optional<Location> requested,
                                               std::optional<double> progress,
                                               bool detail, double zoom, QPointF pan);
}

ReaderImageProvider::ReaderImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage ReaderImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QMutexLocker locker(&m_mutex);
    QImage image = m_images.value(id.section('?', 0, 0));
    if (size) *size = image.size();
    if (!image.isNull() && requestedSize.isValid() && requestedSize != image.size())
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image;
}

void ReaderImageProvider::insert(const QString &key, const QImage &image)
{
    QMutexLocker locker(&m_mutex);
    if (m_images.contains(key)) m_order.removeAll(key);
    m_images.insert(key, image);
    m_order.append(key);
    while (m_order.size() > 32) m_images.remove(m_order.takeFirst());
}

void ReaderImageProvider::clear()
{
    QMutexLocker locker(&m_mutex);
    m_images.clear();
    m_order.clear();
}

bool ReaderController::Location::valid() const
{
    return section >= 0 && section < 100000 && block >= 0 && block < 100000
        && character >= 0 && character <= 64000000;
}

bool ReaderController::Preferences::valid() const
{
    return !fontName.isEmpty() && fontName.toUtf8().size() <= 256
        && std::isfinite(fontSize) && fontSize >= 10 && fontSize <= 32
        && std::isfinite(lineSpacing) && lineSpacing >= 0 && lineSpacing <= 18
        && std::isfinite(margin) && margin >= 6 && margin <= 48;
}

ReaderController::ReaderController(QObject *parent, ReaderImageProvider *provider, QString directory)
    : QObject(parent), m_provider(provider)
{
    m_directory = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/HyprEndfield/Reader") : std::move(directory);
    m_path = m_directory + QStringLiteral("/library.json");
    m_detailTimer.setSingleShot(true);
    m_detailTimer.setInterval(120);
    connect(&m_detailTimer, &QTimer::timeout, this, [this] {
        if (!m_active || m_zoom <= 1 || m_pages.isEmpty()) return;
        requestRender(m_pages.constFirst().location, {}, true);
    });
    loadLibrary();
}

QVariantMap ReaderController::describeBook(const Book &book, int number) const
{
    QString availability;
    const bool available = bookAvailable(book, &availability);
    QVariantList marks;
    for (int index = 0; index < book.bookmarks.size(); ++index) {
        const auto &mark = book.bookmarks.at(index);
        const int page = book.format == QStringLiteral("pdf") ? mark.location.section + 1
            : std::max(1, int(std::round(mark.progress * 100)));
        marks.append(QVariantMap{{QStringLiteral("id"), mark.id}, {QStringLiteral("number"), index + 1},
                                 {QStringLiteral("page"), page}, {QStringLiteral("progress"), mark.progress},
                                 {QStringLiteral("location"), locationMap(mark.location)}});
    }
    return {{QStringLiteral("id"), book.id}, {QStringLiteral("number"), number}, {QStringLiteral("title"), book.title},
            {QStringLiteral("path"), book.path}, {QStringLiteral("format"), book.format.toUpper()},
            {QStringLiteral("progress"), book.progress}, {QStringLiteral("available"), available},
            {QStringLiteral("error"), availability}, {QStringLiteral("selected"), book.id == m_selected},
            {QStringLiteral("bookmarks"), marks}};
}

QVariantMap ReaderController::describePage(const Page &page, bool current) const
{
    const double columnInset = std::round(std::min(349.0 * 0.4, m_preferences.margin * 1.4));
    const double fullHeight = 349 - 2 * columnInset;
    const double columnHeight = page.illustration || page.textHeight <= 0 ? fullHeight
        : std::min(fullHeight, std::round((page.textHeight + m_preferences.lineSpacing) * 1.4));
    return {{QStringLiteral("key"), locationKey(page.location)}, {QStringLiteral("location"), locationMap(page.location)},
            {QStringLiteral("source"), QStringLiteral("image://reader-pages/") + page.imageKey},
            {QStringLiteral("current"), current}, {QStringLiteral("illustration"), page.illustration},
            {QStringLiteral("progress"), page.progress}, {QStringLiteral("summary"), page.summary},
            {QStringLiteral("columnHeight"), columnHeight}, {QStringLiteral("hasPrevious"), page.previous.has_value()},
            {QStringLiteral("hasNext"), page.next.has_value()}};
}

QVariantMap ReaderController::state() const
{
    QVariantList books;
    for (int index = 0; index < m_books.size(); ++index) books.append(describeBook(m_books.at(index), index + 1));
    QVariantMap preferences{{QStringLiteral("fontName"), m_preferences.fontName}, {QStringLiteral("fontSize"), m_preferences.fontSize},
                            {QStringLiteral("lineSpacing"), m_preferences.lineSpacing}, {QStringLiteral("margin"), m_preferences.margin},
                            {QStringLiteral("vertical"), m_preferences.vertical}};
    QVariantMap selected;
    if (const auto *book = selectedBook()) {
        int number = 1;
        for (int index = 0; index < m_books.size(); ++index) if (m_books.at(index).id == book->id) { number = index + 1; break; }
        selected = describeBook(*book, number);
    }
    QVariantList pages;
    QVariantMap current;
    if (!m_pages.isEmpty()) {
        const auto &anchor = m_pages.constFirst();
        for (const auto &page : m_pages) {
            auto value = describePage(page, page.location == anchor.location);
            if (page.location == anchor.location) value.insert(QStringLiteral("role"), QStringLiteral("current"));
            else if (anchor.previous && page.location == *anchor.previous) value.insert(QStringLiteral("role"), QStringLiteral("previous"));
            else value.insert(QStringLiteral("role"), QStringLiteral("next"));
            pages.append(value);
        }
        const Page *shown = &anchor;
        bool detailReady = false;
        if (m_detail && m_detail->location == anchor.location) { shown = &*m_detail; detailReady = true; }
        current = describePage(*shown, true);
        const auto *book = selectedBook();
        const bool bookmarked = book && std::any_of(book->bookmarks.cbegin(), book->bookmarks.cend(),
            [&](const Bookmark &mark) { return mark.location == anchor.location; });
        const int total = anchor.pageCount > 0 ? anchor.pageCount : 100;
        const int number = anchor.pageCount > 0 ? anchor.location.section + 1
            : std::max(1, int(std::round(anchor.progress * 100)));
        current.insert(QStringLiteral("baseSource"), QStringLiteral("image://reader-pages/") + anchor.imageKey);
        current.insert(QStringLiteral("detailReady"), detailReady);
        current.insert(QStringLiteral("bookmarked"), bookmarked);
        current.insert(QStringLiteral("number"), number);
        current.insert(QStringLiteral("total"), total);
        current.insert(QStringLiteral("canPrevious"), !m_history.isEmpty() || anchor.previous.has_value());
        current.insert(QStringLiteral("canNext"), anchor.next.has_value());
    }
    return {{QStringLiteral("ready"), m_ready}, {QStringLiteral("busy"), m_opening || m_rendering},
            {QStringLiteral("loading"), m_opening || (m_rendering && m_pages.isEmpty())}, {QStringLiteral("error"), m_error},
            {QStringLiteral("books"), books}, {QStringLiteral("total"), books.size()}, {QStringLiteral("selected"), m_selected},
            {QStringLiteral("book"), selected}, {QStringLiteral("current"), current}, {QStringLiteral("pages"), pages},
            {QStringLiteral("preferences"), preferences}, {QStringLiteral("reading"), m_reading},
            {QStringLiteral("zoom"), m_zoom}, {QStringLiteral("panX"), m_pan.x()}, {QStringLiteral("panY"), m_pan.y()}};
}

void ReaderController::loadLibrary()
{
    QFile file(m_path);
    m_fileExisted = file.exists();
    if (!m_fileExisted) return;
    auto invalidate = [this] {
        m_ready = false;
        m_books.clear();
        m_selected.clear();
        m_error = QCoreApplication::translate("HyprEndfield", "无法读取阅读器书库，原始数据已保留。");
    };
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumLibraryBytes) { invalidate(); return; }
    m_persisted = file.readAll();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(m_persisted, &parseError);
    const auto root = document.object();
    if (!document.isObject() || parseError.error != QJsonParseError::NoError
        || root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("books")).isArray()) {
        invalidate(); return;
    }
    const auto books = root.value(QStringLiteral("books")).toArray();
    if (books.size() > 50) { invalidate(); return; }
    Preferences loadedPreferences;
    const auto prefs = root.value(QStringLiteral("preferences")).toObject();
    loadedPreferences.fontName = prefs.value(QStringLiteral("fontName")).toString();
    loadedPreferences.fontSize = prefs.value(QStringLiteral("fontSize")).toDouble(std::numeric_limits<double>::quiet_NaN());
    loadedPreferences.lineSpacing = prefs.value(QStringLiteral("lineSpacing")).toDouble(std::numeric_limits<double>::quiet_NaN());
    loadedPreferences.margin = prefs.value(QStringLiteral("margin")).toDouble(std::numeric_limits<double>::quiet_NaN());
    loadedPreferences.vertical = prefs.value(QStringLiteral("continuous")).toBool(true);
    if (!loadedPreferences.valid()) { invalidate(); return; }
    QList<Book> loadedBooks;
    QSet<QString> ids;
    for (const auto &value : books) {
        const auto object = value.toObject();
        Book book;
        book.id = object.value(QStringLiteral("id")).toString();
        book.path = object.value(QStringLiteral("path")).toString();
        book.title = object.value(QStringLiteral("title")).toString();
        book.format = object.value(QStringLiteral("format")).toString().toLower();
        book.device = object.value(QStringLiteral("device")).toString();
        book.inode = object.value(QStringLiteral("inode")).toString();
        bool locationOk = false;
        book.location = locationFromJson(object.value(QStringLiteral("location")), &locationOk);
        book.progress = object.value(QStringLiteral("progress")).toDouble(std::numeric_limits<double>::quiet_NaN());
        bool deviceOk = false, inodeOk = false;
        book.device.toULongLong(&deviceOk); book.inode.toULongLong(&inodeOk);
        if (!value.isObject() || QUuid(book.id).isNull() || ids.contains(book.id) || !QDir::isAbsolutePath(book.path)
            || !supportedPath(book.path) || book.format != QFileInfo(book.path).suffix().toLower()
            || book.title.isEmpty() || book.title.toUtf8().size() > 4096 || !deviceOk || !inodeOk
            || !locationOk || !finiteFraction(book.progress) || !object.value(QStringLiteral("bookmarks")).isArray()) {
            invalidate(); return;
        }
        const auto bookmarks = object.value(QStringLiteral("bookmarks")).toArray();
        if (bookmarks.size() > 128) { invalidate(); return; }
        QSet<QString> markIds;
        for (const auto &markValue : bookmarks) {
            const auto markObject = markValue.toObject();
            Bookmark mark;
            mark.id = markObject.value(QStringLiteral("id")).toString();
            bool markLocationOk = false;
            mark.location = locationFromJson(markObject.value(QStringLiteral("location")), &markLocationOk);
            mark.progress = markObject.value(QStringLiteral("progress")).toDouble(std::numeric_limits<double>::quiet_NaN());
            if (!markValue.isObject() || QUuid(mark.id).isNull() || markIds.contains(mark.id)
                || !markLocationOk || !finiteFraction(mark.progress)) { invalidate(); return; }
            markIds.insert(mark.id); book.bookmarks.append(mark);
        }
        ids.insert(book.id); loadedBooks.append(book);
    }
    const QString selected = root.value(QStringLiteral("selected")).toString();
    if (!selected.isEmpty() && !ids.contains(selected)) { invalidate(); return; }
    if (root.contains(QStringLiteral("reading")) && !root.value(QStringLiteral("reading")).isBool()) { invalidate(); return; }
    m_books = std::move(loadedBooks);
    m_selected = selected;
    m_preferences = loadedPreferences;
    m_reading = root.value(QStringLiteral("reading")).toBool(false);
}

bool ReaderController::saveLibrary()
{
    if (!m_ready) { m_error = QCoreApplication::translate("HyprEndfield", "阅读器书库存储不可用。"); return false; }
    QFile current(m_path);
    if (current.exists() != m_fileExisted || (m_fileExisted
        && (!current.open(QIODevice::ReadOnly) || current.size() > maximumLibraryBytes || current.readAll() != m_persisted))) {
        m_error = QCoreApplication::translate("HyprEndfield", "阅读器书库已被其他程序修改，请重新启动应用后保存。");
        return false;
    }
    QJsonArray books;
    for (const auto &book : m_books) {
        QJsonArray marks;
        for (const auto &mark : book.bookmarks) marks.append(QJsonObject{{QStringLiteral("id"), mark.id},
            {QStringLiteral("location"), locationJson(mark.location)}, {QStringLiteral("progress"), mark.progress}});
        books.append(QJsonObject{{QStringLiteral("id"), book.id}, {QStringLiteral("path"), book.path},
            {QStringLiteral("title"), book.title}, {QStringLiteral("format"), book.format},
            {QStringLiteral("device"), book.device}, {QStringLiteral("inode"), book.inode},
            {QStringLiteral("location"), locationJson(book.location)}, {QStringLiteral("progress"), book.progress},
            {QStringLiteral("bookmarks"), marks}});
    }
    QJsonObject root{{QStringLiteral("version"), 1}, {QStringLiteral("books"), books},
        {QStringLiteral("preferences"), QJsonObject{{QStringLiteral("fontName"), m_preferences.fontName},
            {QStringLiteral("fontSize"), m_preferences.fontSize}, {QStringLiteral("lineSpacing"), m_preferences.lineSpacing},
            {QStringLiteral("margin"), m_preferences.margin}, {QStringLiteral("rightToLeft"), false},
            {QStringLiteral("continuous"), m_preferences.vertical}}}};
    if (!m_selected.isEmpty()) root.insert(QStringLiteral("selected"), m_selected);
    if (m_reading) root.insert(QStringLiteral("reading"), true);
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (bytes.size() > maximumLibraryBytes) { m_error = tooLargeMessage(); return false; }
    if (!QDir().mkpath(m_directory)) { m_error = QCoreApplication::translate("HyprEndfield", "无法创建阅读器数据目录。"); return false; }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_error = QCoreApplication::translate("HyprEndfield", "无法保存阅读器书库。"); return false;
    }
    m_persisted = bytes;
    m_fileExisted = true;
    m_error.clear();
    return true;
}

bool ReaderController::fail(const QString &action, const QString &message)
{
    m_error = message;
    emit stateChanged();
    emit actionFinished(action, false, message);
    return false;
}

ReaderController::Book *ReaderController::findBook(const QString &id)
{
    for (auto &book : m_books) if (book.id == id) return &book;
    return nullptr;
}

const ReaderController::Book *ReaderController::findBook(const QString &id) const
{
    for (const auto &book : m_books) if (book.id == id) return &book;
    return nullptr;
}

ReaderController::Book *ReaderController::selectedBook() { return findBook(m_selected); }
const ReaderController::Book *ReaderController::selectedBook() const { return findBook(m_selected); }

bool ReaderController::bookAvailable(const Book &book, QString *message) const
{
    QString ignoredDevice, ignoredInode, error;
    const bool available = readIdentity(book.path, &ignoredDevice, &ignoredInode, &error, book.device, book.inode);
    if (message) *message = error;
    return available;
}

void ReaderController::setActive(bool value)
{
    if (m_active == value) return;
    m_active = value;
    emit activeChanged();
    if (value) openSelected();
    else {
        ++m_generation;
        m_detailTimer.stop();
        m_document.reset();
        m_opening = false;
        m_rendering = false;
        m_pendingRender.reset();
        m_detail.reset();
        m_history.clear();
        emit stateChanged();
    }
}

void ReaderController::clearDocument(bool keepPage)
{
    m_document.reset();
    m_detail.reset();
    m_pendingRender.reset();
    m_history.clear();
    if (!keepPage) { m_pages.clear(); m_pageBook.clear(); }
}

void ReaderController::openSelected()
{
    if (!m_active) return;
    const auto *book = selectedBook();
    if (!book) { clearDocument(); emit stateChanged(); return; }
    const QString id = book->id, path = book->path, device = book->device, inode = book->inode;
    const bool keepPage = m_pageBook == id && !m_pages.isEmpty();
    ++m_generation;
    const quint64 token = m_generation;
    clearDocument(keepPage);
    m_opening = true;
    m_error.clear();
    emit stateChanged();
    auto *watcher = new QFutureWatcher<OpenResult>(this);
    connect(watcher, &QFutureWatcher<OpenResult>::finished, this, [this, watcher, token, id] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (token != m_generation || !m_active) return;
        m_opening = false;
        if (!result.error.isEmpty()) {
            clearDocument();
            m_error = result.error;
            emit stateChanged();
            return;
        }
        const auto *book = findBook(id);
        if (!book || m_selected != id) { emit stateChanged(); return; }
        m_document = result.document;
        requestRender(book->location);
    });
    watcher->setFuture(QtConcurrent::run([path, device, inode] {
        QString error;
        if (!readIdentity(path, nullptr, nullptr, &error, device, inode)) return OpenResult{{}, error};
        return openDocument(path);
    }));
}

bool ReaderController::importUrl(const QUrl &url)
{
    if (!m_active || m_opening || !m_ready) return false;
    if (!url.isLocalFile() || !supportedPath(url.toLocalFile())) return fail(QStringLiteral("import"), unsupportedMessage());
    const QString path = QDir::cleanPath(url.toLocalFile());
    ++m_generation;
    const quint64 token = m_generation;
    clearDocument();
    m_opening = true;
    m_error.clear();
    emit stateChanged();
    auto *watcher = new QFutureWatcher<OpenResult>(this);
    connect(watcher, &QFutureWatcher<OpenResult>::finished, this, [this, watcher, token, path] {
        const auto result = watcher->result();
        watcher->deleteLater();
        if (token != m_generation || !m_active) return;
        m_opening = false;
        if (!result.error.isEmpty()) { fail(QStringLiteral("import"), result.error); return; }
        QString device, inode, error;
        if (!readIdentity(path, &device, &inode, &error)) { fail(QStringLiteral("import"), error); return; }
        const auto oldBooks = m_books;
        const QString oldSelected = m_selected;
        auto found = std::find_if(m_books.begin(), m_books.end(), [&](const Book &book) {
            return book.device == device && book.inode == inode;
        });
        if (found == m_books.end()) {
            if (m_books.size() >= 50) { fail(QStringLiteral("import"), QCoreApplication::translate("HyprEndfield", "书库最多保存 50 本书。")); return; }
            Book book;
            book.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            book.path = path; book.title = result.document->title; book.format = QFileInfo(path).suffix().toLower();
            book.device = device; book.inode = inode;
            m_books.append(book); m_selected = book.id;
        } else {
            found->path = path; found->title = result.document->title; found->format = QFileInfo(path).suffix().toLower();
            m_selected = found->id;
        }
        if (!saveLibrary()) {
            m_books = oldBooks; m_selected = oldSelected;
            fail(QStringLiteral("import"), m_error); return;
        }
        m_document = result.document;
        resetImageViewInternal();
        m_pageBook.clear();
        m_pages.clear();
        emit actionFinished(QStringLiteral("import"), true, QCoreApplication::translate("HyprEndfield", "已加入书库"));
        const auto *book = selectedBook();
        requestRender(book ? std::optional<Location>(book->location) : std::nullopt);
    });
    watcher->setFuture(QtConcurrent::run([path] { return openDocument(path); }));
    return true;
}

bool ReaderController::selectBook(const QString &id)
{
    if (id == m_selected) { if (m_active && !m_document) openSelected(); return true; }
    if (!findBook(id)) return false;
    const QString previous = m_selected;
    m_selected = id;
    if (!saveLibrary()) { m_selected = previous; return fail(QStringLiteral("select"), m_error); }
    resetImageViewInternal();
    clearDocument();
    if (m_active) openSelected(); else emit stateChanged();
    return true;
}

bool ReaderController::removeBook(const QString &id)
{
    const auto found = std::find_if(m_books.cbegin(), m_books.cend(), [&](const Book &book) { return book.id == id; });
    if (found == m_books.cend()) return false;
    const auto oldBooks = m_books;
    const QString oldSelected = m_selected;
    const bool removedCurrent = id == m_selected;
    m_books.removeIf([&](const Book &book) { return book.id == id; });
    if (removedCurrent) m_selected = m_books.isEmpty() ? QString() : m_books.constFirst().id;
    if (!saveLibrary()) { m_books = oldBooks; m_selected = oldSelected; return fail(QStringLiteral("remove"), m_error); }
    if (removedCurrent) {
        ++m_generation;
        resetImageViewInternal();
        clearDocument();
        if (m_active && !m_selected.isEmpty()) openSelected();
    }
    emit stateChanged();
    emit actionFinished(QStringLiteral("remove"), true, QCoreApplication::translate("HyprEndfield", "已从书库移除"));
    return true;
}

bool ReaderController::relinkBook(const QString &id, const QUrl &url)
{
    if (m_opening || !url.isLocalFile() || !supportedPath(url.toLocalFile()) || !findBook(id))
        return fail(QStringLiteral("relink"), unsupportedMessage());
    const QString path = QDir::cleanPath(url.toLocalFile());
    ++m_generation;
    const quint64 token = m_generation;
    m_opening = true;
    m_error.clear();
    emit stateChanged();
    auto *watcher = new QFutureWatcher<OpenResult>(this);
    connect(watcher, &QFutureWatcher<OpenResult>::finished, this, [this, watcher, token, id, path] {
        const auto result = watcher->result(); watcher->deleteLater();
        if (token != m_generation) return;
        m_opening = false;
        if (!result.error.isEmpty()) { fail(QStringLiteral("relink"), result.error); return; }
        auto *book = findBook(id);
        if (!book) { emit stateChanged(); return; }
        QString device, inode, error;
        if (!readIdentity(path, &device, &inode, &error)) { fail(QStringLiteral("relink"), error); return; }
        const Book previous = *book;
        book->path = path; book->format = QFileInfo(path).suffix().toLower(); book->device = device; book->inode = inode;
        if (!saveLibrary()) { *book = previous; fail(QStringLiteral("relink"), m_error); return; }
        emit actionFinished(QStringLiteral("relink"), true, QCoreApplication::translate("HyprEndfield", "已重新链接"));
        if (m_active && m_selected == id) {
            m_document = result.document; resetImageViewInternal(); m_pages.clear(); m_pageBook.clear(); requestRender(book->location);
        } else emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([path] { return openDocument(path); }));
    return true;
}

void ReaderController::setTab(const QString &tab)
{
    const bool reading = tab == QStringLiteral("reading");
    if (reading == m_reading) return;
    const bool previous = m_reading;
    m_reading = reading;
    if (!saveLibrary()) m_reading = previous;
    emit stateChanged();
}

void ReaderController::requestRender(std::optional<Location> location, std::optional<double> progress, bool detail)
{
    if (!m_active || !m_document) return;
    m_pendingRender = RenderRequest{location, progress, detail};
    ++m_generation;
    if (!m_rendering) startRender();
}

void ReaderController::startRender()
{
    if (!m_pendingRender || !m_document || !m_active) return;
    const auto request = *m_pendingRender;
    m_pendingRender.reset();
    const quint64 token = m_generation;
    const auto document = m_document;
    const auto preferences = m_preferences;
    const double zoom = m_zoom;
    const QPointF pan = m_pan;
    m_rendering = true;
    if (!request.detail) m_error.clear();
    emit stateChanged();
    auto *watcher = new QFutureWatcher<RenderResult>(this);
    connect(watcher, &QFutureWatcher<RenderResult>::finished, this, [this, watcher, token] {
        auto result = watcher->result(); watcher->deleteLater();
        m_rendering = false;
        if (token == m_generation && m_active) applyRender(std::move(result), token);
        if (m_pendingRender && m_active) startRender();
        else emit stateChanged();
    });
    watcher->setFuture(QtConcurrent::run([document, preferences, request, zoom, pan] {
        return renderDocument(document, preferences, request.location, request.progress, request.detail, zoom, pan);
    }));
}

void ReaderController::applyRender(RenderResult result, quint64 token)
{
    if (token != m_generation) return;
    if (!result.error.isEmpty()) { m_error = result.error; return; }
    if (result.pages.isEmpty() || !m_provider) { m_error = invalidBookMessage(); return; }
    for (auto &page : result.pages) {
        page.imageKey = QStringLiteral("%1-%2").arg(++m_imageSerial).arg(locationKey(page.location));
        m_provider->insert(page.imageKey, page.image);
    }
    if (result.detail) {
        if (!m_pages.isEmpty() && result.pages.constFirst().location == m_pages.constFirst().location
            && qFuzzyCompare(result.zoom, m_zoom) && result.pan == m_pan)
            m_detail = result.pages.constFirst();
        return;
    }
    if (!m_history.isEmpty()) result.pages.first().previous = m_history.constLast();
    m_pages = std::move(result.pages);
    m_pageBook = m_selected;
    m_detail.reset();
    if (!m_pages.isEmpty()) persistProgress(m_pages.constFirst());
    if (m_zoom > 1) m_detailTimer.start();
}

void ReaderController::persistProgress(const Page &page)
{
    auto *book = selectedBook();
    if (!book || (book->location == page.location && qFuzzyCompare(book->progress + 1, page.progress + 1))) return;
    const auto previousLocation = book->location;
    const double previousProgress = book->progress;
    book->location = page.location;
    book->progress = std::clamp(page.progress, 0.0, 1.0);
    if (!saveLibrary()) { book->location = previousLocation; book->progress = previousProgress; }
}

void ReaderController::turn(int direction)
{
    if (!m_active || m_pages.isEmpty() || direction == 0) return;
    const auto current = m_pages.constFirst();
    std::optional<Location> target;
    if (direction > 0) {
        target = current.next;
        if (target) {
            m_history.append(current.location);
            while (m_history.size() > 256) m_history.removeFirst();
        }
    } else if (!m_history.isEmpty()) {
        target = m_history.takeLast();
    } else {
        target = current.previous;
    }
    if (!target) return;
    m_detail.reset();
    requestRender(*target);
}

void ReaderController::jump(int page)
{
    if (m_pages.isEmpty()) return;
    const auto &current = m_pages.constFirst();
    const int total = current.pageCount > 0 ? current.pageCount : 100;
    page = std::clamp(page, 1, std::max(1, total));
    m_history.clear();
    const double progress = current.pageCount > 0 ? (double(page) - 0.5) / total
        : page <= 1 ? 0 : double(page) / 100;
    m_detail.reset();
    requestRender({}, progress);
}

void ReaderController::toggleBookmark()
{
    auto *book = selectedBook();
    if (!book || m_pages.isEmpty()) return;
    const auto previous = book->bookmarks;
    const auto &page = m_pages.constFirst();
    const auto found = std::find_if(book->bookmarks.begin(), book->bookmarks.end(),
        [&](const Bookmark &mark) { return mark.location == page.location; });
    bool added = false;
    if (found != book->bookmarks.end()) book->bookmarks.erase(found);
    else {
        if (book->bookmarks.size() >= 128) { fail(QStringLiteral("bookmark"), QCoreApplication::translate("HyprEndfield", "每本书最多保存 128 个书签。")); return; }
        book->bookmarks.append({QUuid::createUuid().toString(QUuid::WithoutBraces), page.location, page.progress});
        added = true;
    }
    if (!saveLibrary()) { book->bookmarks = previous; fail(QStringLiteral("bookmark"), m_error); return; }
    emit stateChanged();
    emit actionFinished(QStringLiteral("bookmark"), true, added ? QCoreApplication::translate("HyprEndfield", "已添加书签") : QCoreApplication::translate("HyprEndfield", "已移除书签"));
}

void ReaderController::removeBookmark(const QString &id)
{
    auto *book = selectedBook();
    if (!book) return;
    const auto previous = book->bookmarks;
    const auto previousSize = book->bookmarks.size();
    book->bookmarks.removeIf([&](const Bookmark &mark) { return mark.id == id; });
    if (book->bookmarks.size() == previousSize) return;
    if (!saveLibrary()) { book->bookmarks = previous; fail(QStringLiteral("bookmark"), m_error); return; }
    emit stateChanged();
    emit actionFinished(QStringLiteral("bookmark"), true, QCoreApplication::translate("HyprEndfield", "已移除书签"));
}

void ReaderController::goBookmark(const QString &id)
{
    const auto *book = selectedBook();
    if (!book) return;
    const auto found = std::find_if(book->bookmarks.cbegin(), book->bookmarks.cend(),
        [&](const Bookmark &mark) { return mark.id == id; });
    if (found == book->bookmarks.cend()) return;
    m_history.clear();
    m_detail.reset();
    requestRender(found->location);
}

void ReaderController::setPreference(const QString &name, const QVariant &value)
{
    Preferences next = m_preferences;
    if (name == QStringLiteral("fontName")) next.fontName = value.toString();
    else if (name == QStringLiteral("fontSize")) next.fontSize = value.toDouble();
    else if (name == QStringLiteral("lineSpacing")) next.lineSpacing = value.toDouble();
    else if (name == QStringLiteral("margin")) next.margin = value.toDouble();
    else if (name == QStringLiteral("vertical")) next.vertical = value.toBool();
    else return;
    if (!next.valid() || next == m_preferences) return;
    const auto previous = m_preferences;
    m_preferences = next;
    if (!saveLibrary()) { m_preferences = previous; fail(QStringLiteral("settings"), m_error); return; }
    emit actionFinished("settings", true, {});
    m_history.clear();
    m_detail.reset();
    if (!m_pages.isEmpty()) requestRender(m_pages.constFirst().location);
    else emit stateChanged();
}

void ReaderController::setImageView(double zoom, double panX, double panY)
{
    if (m_pages.isEmpty()) return;
    if (!std::isfinite(zoom) || !std::isfinite(panX) || !std::isfinite(panY)) return;
    zoom = std::clamp(zoom, 1.0, 12.0);
    const double limitX = logicalWidth * (zoom - 1) / 2;
    const double limitY = logicalHeight * (zoom - 1) / 2;
    const QPointF pan(std::clamp(panX, -limitX, limitX), std::clamp(panY, -limitY, limitY));
    if (qFuzzyCompare(zoom, m_zoom) && pan == m_pan) return;
    m_zoom = zoom;
    m_pan = pan;
    m_detail.reset();
    emit stateChanged();
    m_detailTimer.stop();
    if (m_zoom > 1) m_detailTimer.start();
}

void ReaderController::zoomAt(double factor, double x, double y)
{
    if (!std::isfinite(factor) || factor <= 0) return;
    const double next = std::clamp(m_zoom * factor, 1.0, 12.0);
    const double ratio = next / m_zoom;
    const QPointF point(x / 1.4 - logicalWidth / 2, y / 1.4 - logicalHeight / 2);
    const QPointF pan(point.x() - (point.x() - m_pan.x()) * ratio,
                      point.y() - (point.y() - m_pan.y()) * ratio);
    setImageView(next, pan.x(), pan.y());
}

void ReaderController::panBy(double dx, double dy)
{
    if (m_zoom <= 1) return;
    setImageView(m_zoom, m_pan.x() + dx / 1.4, m_pan.y() + dy / 1.4);
}

void ReaderController::resetImageViewInternal()
{
    m_detailTimer.stop();
    m_zoom = 1;
    m_pan = {};
    m_detail.reset();
}

void ReaderController::resetZoom()
{
    if (qFuzzyCompare(m_zoom, 1.0) && m_pan.isNull()) return;
    resetImageViewInternal();
    emit stateChanged();
}

void ReaderController::refresh()
{
    emit stateChanged();
    if (m_active && !m_selected.isEmpty() && !m_document) openSelected();
}

QStringList ReaderController::fontFamilies() const
{
    auto families = QFontDatabase::families();
    families.sort(Qt::CaseInsensitive);
    return families;
}

namespace {

QString locationKey(const Location &location)
{
    return QStringLiteral("%1:%2:%3").arg(location.section).arg(location.block).arg(location.character);
}

QJsonObject locationJson(const Location &location)
{
    return {{QStringLiteral("section"), location.section}, {QStringLiteral("block"), location.block},
            {QStringLiteral("character"), location.character}};
}

Location locationFromJson(const QJsonValue &value, bool *ok)
{
    const auto object = value.toObject();
    Location location{object.value(QStringLiteral("section")).toInt(-1),
                      object.value(QStringLiteral("block")).toInt(-1),
                      object.value(QStringLiteral("character")).toInt(-1)};
    *ok = value.isObject() && location.valid();
    return location;
}

QVariantMap locationMap(const Location &location)
{
    return {{QStringLiteral("section"), location.section}, {QStringLiteral("block"), location.block},
            {QStringLiteral("character"), location.character}, {QStringLiteral("key"), locationKey(location)}};
}

bool supportedPath(const QString &path)
{
    const auto suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("pdf") || suffix == QStringLiteral("epub") || suffix == QStringLiteral("txt");
}

bool readIdentity(const QString &path, QString *device, QString *inode, QString *error,
                  const QString &expectedDevice, const QString &expectedInode)
{
    const auto native = QFile::encodeName(path);
    struct stat info {};
    if (!QDir::isAbsolutePath(path) || path.contains(QChar::Null) || ::lstat(native.constData(), &info) != 0
        || !S_ISREG(info.st_mode) || ::access(native.constData(), R_OK) != 0) {
        if (error) *error = unavailableMessage();
        return false;
    }
    const QString foundDevice = QString::number(qulonglong(info.st_dev));
    const QString foundInode = QString::number(qulonglong(info.st_ino));
    if ((!expectedDevice.isEmpty() && foundDevice != expectedDevice)
        || (!expectedInode.isEmpty() && foundInode != expectedInode)) {
        if (error) *error = QCoreApplication::translate("HyprEndfield", "原始书籍路径已被其他文件占用，请重新选择。");
        return false;
    }
    if (device) *device = foundDevice;
    if (inode) *inode = foundInode;
    return true;
}

QString decodeGB18030(const QByteArray &bytes)
{
    iconv_t converter = iconv_open("UTF-8", "GB18030");
    if (converter == reinterpret_cast<iconv_t>(-1)) return {};
    QByteArray output(std::max<qsizetype>(64, bytes.size() * 4 + 16), Qt::Uninitialized);
    char *input = const_cast<char *>(bytes.constData());
    size_t inputLeft = size_t(bytes.size());
    char *target = output.data();
    size_t targetLeft = size_t(output.size());
    const size_t status = iconv(converter, &input, &inputLeft, &target, &targetLeft);
    iconv_close(converter);
    if (status == size_t(-1) || inputLeft != 0) return {};
    output.resize(qsizetype(output.size() - qsizetype(targetLeft)));
    return QString::fromUtf8(output);
}

QString decodeText(const QByteArray &bytes)
{
    QString text;
    if (bytes.startsWith("\xFF\xFE")) {
        QStringDecoder decoder(QStringDecoder::Utf16LE);
        text = decoder.decode(QByteArrayView(bytes).sliced(2));
        if (decoder.hasError()) return {};
    } else if (bytes.startsWith("\xFE\xFF")) {
        QStringDecoder decoder(QStringDecoder::Utf16BE);
        text = decoder.decode(QByteArrayView(bytes).sliced(2));
        if (decoder.hasError()) return {};
    } else {
        QStringDecoder decoder(QStringDecoder::Utf8);
        text = decoder.decode(bytes);
        if (decoder.hasError()) text = decodeGB18030(bytes);
    }
    if (text.isEmpty() || text.contains(QChar::Null)) return {};
    return text;
}

struct ZipCloser {
    void operator()(zip_t *archive) const { if (archive) zip_close(archive); }
};
using ZipHandle = std::unique_ptr<zip_t, ZipCloser>;

bool safeArchivePath(const QString &path)
{
    if (path.isEmpty() || path.startsWith('/') || path.contains('\\') || path.contains(':') || path.contains(QChar::Null)) return false;
    const auto parts = path.split('/', Qt::KeepEmptyParts);
    return std::none_of(parts.cbegin(), parts.cend(), [](const QString &part) {
        return part == QStringLiteral(".") || part == QStringLiteral("..");
    });
}

ZipHandle openArchive(const QString &path, QString *error)
{
    if (QFileInfo(path).size() > maximumEpubBytes) {
        *error = tooLargeMessage();
        return {};
    }
    int code = 0;
    ZipHandle archive(zip_open(QFile::encodeName(path).constData(), ZIP_RDONLY, &code));
    if (!archive) {
        *error = invalidArchiveMessage();
        return {};
    }
    const auto count = zip_get_num_entries(archive.get(), 0);
    if (count <= 0 || count > maximumZipEntries) {
        *error = invalidArchiveMessage();
        return {};
    }
    quint64 inflated = 0;
    for (zip_int64_t index = 0; index < count; ++index) {
        zip_stat_t stat {};
        zip_stat_init(&stat);
        if (zip_stat_index(archive.get(), zip_uint64_t(index), ZIP_FL_ENC_GUESS, &stat) != 0 || !stat.name) {
            *error = invalidArchiveMessage();
            return {};
        }
        const QString name = QString::fromUtf8(stat.name);
        if (!safeArchivePath(name) || stat.size > maximumEntryBytes || stat.comp_size > maximumEntryBytes
            || stat.encryption_method != ZIP_EM_NONE || stat.size > std::max<quint64>(1, stat.comp_size) * 2000) {
            *error = stat.encryption_method != ZIP_EM_NONE ? encryptedMessage() : invalidArchiveMessage();
            return {};
        }
        zip_uint8_t operatingSystem = 0;
        zip_uint32_t attributes = 0;
        if (zip_file_get_external_attributes(archive.get(), zip_uint64_t(index), 0, &operatingSystem, &attributes) == 0
            && operatingSystem == ZIP_OPSYS_UNIX && ((attributes >> 16) & 0170000) == 0120000) {
            *error = invalidArchiveMessage();
            return {};
        }
        inflated += stat.size;
        if (inflated > maximumInflatedBytes) {
            *error = tooLargeMessage();
            return {};
        }
    }
    return archive;
}

QByteArray archiveData(zip_t *archive, const QString &name, quint64 limit, QString *error)
{
    if (!safeArchivePath(name)) {
        *error = invalidArchiveMessage();
        return {};
    }
    zip_stat_t stat {};
    zip_stat_init(&stat);
    const QByteArray encoded = name.toUtf8();
    if (zip_stat(archive, encoded.constData(), ZIP_FL_ENC_GUESS, &stat) != 0 || stat.size > limit
        || stat.size > quint64(std::numeric_limits<int>::max())) {
        *error = invalidArchiveMessage();
        return {};
    }
    std::unique_ptr<zip_file_t, decltype(&zip_fclose)> file(zip_fopen(archive, encoded.constData(), ZIP_FL_ENC_GUESS), &zip_fclose);
    if (!file) {
        *error = invalidArchiveMessage();
        return {};
    }
    QByteArray result(int(stat.size), Qt::Uninitialized);
    zip_uint64_t offset = 0;
    while (offset < stat.size) {
        const auto read = zip_fread(file.get(), result.data() + offset, stat.size - offset);
        if (read <= 0) {
            *error = invalidArchiveMessage();
            return {};
        }
        offset += zip_uint64_t(read);
    }
    return result;
}

QString resolveArchivePath(const QString &href, const QString &base, QString *error)
{
    QString raw = href.section('#', 0, 0);
    raw = QUrl::fromPercentEncoding(raw.toUtf8());
    if (raw.isEmpty() || raw.startsWith('/') || raw.contains(':') || raw.contains('\\')
        || raw.contains('?') || raw.contains(QChar::Null)) {
        *error = invalidBookMessage();
        return {};
    }
    QStringList parts = base.split('/');
    if (!parts.isEmpty()) parts.removeLast();
    for (const auto &part : raw.split('/', Qt::SkipEmptyParts)) {
        if (part == QStringLiteral(".")) continue;
        if (part == QStringLiteral("..")) {
            if (parts.isEmpty()) { *error = invalidBookMessage(); return {}; }
            parts.removeLast();
        } else {
            parts.append(part);
        }
    }
    const QString result = parts.join('/');
    if (!safeArchivePath(result)) { *error = invalidBookMessage(); return {}; }
    return result;
}

struct Block {
    enum class Kind { Text, Image } kind = Kind::Text;
    QString value;
};

struct ParsedXml {
    struct ManifestItem { QString href; QString type; };
    QString rootFile;
    QString title;
    QHash<QString, ManifestItem> manifest;
    QStringList spine;
    QList<Block> blocks;
};

QString xmlAttribute(const QXmlStreamAttributes &attributes, const QString &name)
{
    for (const auto &attribute : attributes) {
        if (attribute.name().compare(name, Qt::CaseInsensitive) == 0
            || attribute.qualifiedName().compare(name, Qt::CaseInsensitive) == 0)
            return attribute.value().toString();
    }
    return {};
}

ParsedXml parseXml(const QByteArray &bytes, QString *error)
{
    QString source;
    if (bytes.startsWith("\xFF\xFE")) {
        QStringDecoder decoder(QStringDecoder::Utf16LE);
        source = decoder.decode(QByteArrayView(bytes).sliced(2));
        if (decoder.hasError()) { *error = invalidBookMessage(); return {}; }
    } else if (bytes.startsWith("\xFE\xFF")) {
        QStringDecoder decoder(QStringDecoder::Utf16BE);
        source = decoder.decode(QByteArrayView(bytes).sliced(2));
        if (decoder.hasError()) { *error = invalidBookMessage(); return {}; }
    } else {
        QStringDecoder decoder(QStringDecoder::Utf8);
        source = decoder.decode(bytes);
        if (decoder.hasError()) { *error = invalidBookMessage(); return {}; }
    }
    if (source.contains(QChar::Null) || source.contains(QStringLiteral("<!ENTITY"), Qt::CaseInsensitive)) {
        *error = invalidBookMessage();
        return {};
    }
    source.replace(QStringLiteral("&nbsp;"), QStringLiteral("&#160;"));
    ParsedXml result;
    QXmlStreamReader xml(source);
    int depth = 0;
    int nodes = 0;
    int bodyDepth = -1;
    int ignoredDepth = -1;
    int titleDepth = -1;
    QString text;
    auto flush = [&] {
        const QString clean = text.trimmed();
        if (!clean.isEmpty()) result.blocks.append({Block::Kind::Text, clean});
        text.clear();
    };
    while (!xml.atEnd()) {
        const auto token = xml.readNext();
        if (token == QXmlStreamReader::DTD || token == QXmlStreamReader::EntityReference) {
            *error = invalidBookMessage();
            return {};
        }
        if (token == QXmlStreamReader::StartElement) {
            ++depth;
            ++nodes;
            if (depth > 128 || nodes > 100000 || result.blocks.size() > 8192) {
                *error = invalidBookMessage();
                return {};
            }
            const QString tag = xml.name().toString().toLower();
            const auto attributes = xml.attributes();
            if (tag == QStringLiteral("rootfile") && result.rootFile.isEmpty()) result.rootFile = xmlAttribute(attributes, QStringLiteral("full-path"));
            if (tag == QStringLiteral("item")) {
                const QString id = xmlAttribute(attributes, QStringLiteral("id"));
                const QString href = xmlAttribute(attributes, QStringLiteral("href"));
                const QString type = xmlAttribute(attributes, QStringLiteral("media-type"));
                if (!id.isEmpty() && !href.isEmpty() && !type.isEmpty()) {
                    if (result.manifest.contains(id)) { *error = invalidBookMessage(); return {}; }
                    result.manifest.insert(id, {href, type});
                }
            }
            if (tag == QStringLiteral("itemref") && xmlAttribute(attributes, QStringLiteral("linear")) != QStringLiteral("no")) {
                const QString id = xmlAttribute(attributes, QStringLiteral("idref"));
                if (!id.isEmpty()) result.spine.append(id);
            }
            if (tag == QStringLiteral("title") && bodyDepth < 0) titleDepth = depth;
            if (tag == QStringLiteral("body")) bodyDepth = depth;
            if (ignoredDepth < 0 && QStringList{QStringLiteral("script"), QStringLiteral("style"), QStringLiteral("iframe"),
                                                QStringLiteral("object"), QStringLiteral("audio"), QStringLiteral("video")}.contains(tag))
                ignoredDepth = depth;
            if (bodyDepth >= 0 && ignoredDepth < 0) {
                if (!text.isEmpty() && QStringList{QStringLiteral("p"), QStringLiteral("div"), QStringLiteral("br"),
                                                   QStringLiteral("h1"), QStringLiteral("h2"), QStringLiteral("h3"),
                                                   QStringLiteral("li"), QStringLiteral("section")}.contains(tag))
                    text.append('\n');
                if (tag == QStringLiteral("img") || tag == QStringLiteral("image")) {
                    QString href = xmlAttribute(attributes, QStringLiteral("src"));
                    if (href.isEmpty()) href = xmlAttribute(attributes, QStringLiteral("href"));
                    if (href.isEmpty()) href = xmlAttribute(attributes, QStringLiteral("xlink:href"));
                    if (!href.isEmpty()) { flush(); result.blocks.append({Block::Kind::Image, href}); }
                }
            }
        } else if (token == QXmlStreamReader::Characters) {
            if (titleDepth >= 0) result.title.append(xml.text());
            if (bodyDepth >= 0 && ignoredDepth < 0) text.append(xml.text());
        } else if (token == QXmlStreamReader::EndElement) {
            if (depth == ignoredDepth) ignoredDepth = -1;
            if (depth == titleDepth) titleDepth = -1;
            if (depth == bodyDepth) { flush(); bodyDepth = -1; }
            --depth;
        }
    }
    if (xml.hasError() || depth != 0) {
        *error = invalidBookMessage();
        return {};
    }
    flush();
    return result;
}

ReaderController::OpenResult openDocument(const QString &path)
{
    ReaderController::OpenResult result;
    QString identityError;
    if (!readIdentity(path, nullptr, nullptr, &identityError)) { result.error = identityError; return result; }
    if (!supportedPath(path)) { result.error = unsupportedMessage(); return result; }
    auto document = std::make_shared<Document>();
    document->path = path;
    document->title = QFileInfo(path).completeBaseName();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("pdf")) {
        if (QFileInfo(path).size() > maximumPdfBytes) { result.error = tooLargeMessage(); return result; }
        QPdfDocument pdf;
        const auto status = pdf.load(path);
        if (status == QPdfDocument::Error::IncorrectPassword || status == QPdfDocument::Error::UnsupportedSecurityScheme) {
            result.error = encryptedMessage(); return result;
        }
        if (status != QPdfDocument::Error::None || pdf.pageCount() <= 0 || pdf.pageCount() >= 100000) {
            result.error = invalidBookMessage(); return result;
        }
        const QString metadataTitle = pdf.metaData(QPdfDocument::MetaDataField::Title).toString().trimmed();
        if (!metadataTitle.isEmpty()) document->title = metadataTitle;
        document->format = Document::Format::Pdf;
        document->sections = pdf.pageCount();
    } else if (suffix == QStringLiteral("txt")) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > maximumTextBytes) {
            result.error = file.size() > maximumTextBytes ? tooLargeMessage() : unavailableMessage(); return result;
        }
        const QByteArray bytes = file.read(maximumTextBytes + 1);
        if (bytes.size() > maximumTextBytes) { result.error = tooLargeMessage(); return result; }
        document->text = decodeText(bytes);
        if (document->text.isEmpty()) { result.error = invalidBookMessage(); return result; }
        document->format = Document::Format::Text;
        document->sections = 1;
    } else {
        QString error;
        auto archive = openArchive(path, &error);
        if (!archive) { result.error = error; return result; }
        const auto mimetype = archiveData(archive.get(), QStringLiteral("mimetype"), 100, &error).trimmed();
        if (!error.isEmpty() || mimetype != QByteArrayLiteral("application/epub+zip")) {
            result.error = error.isEmpty() ? invalidBookMessage() : error; return result;
        }
        const auto container = parseXml(archiveData(archive.get(), QStringLiteral("META-INF/container.xml"), 256 * 1024, &error), &error);
        if (!error.isEmpty() || !safeArchivePath(container.rootFile)) {
            result.error = error.isEmpty() ? invalidBookMessage() : error; return result;
        }
        const auto package = parseXml(archiveData(archive.get(), container.rootFile, 2 * 1024 * 1024, &error), &error);
        if (!error.isEmpty() || package.spine.isEmpty() || package.spine.size() > 4096) {
            result.error = error.isEmpty() ? invalidBookMessage() : error; return result;
        }
        const QStringList accepted{QStringLiteral("application/xhtml+xml"), QStringLiteral("text/html"),
                                   QStringLiteral("image/jpeg"), QStringLiteral("image/png"),
                                   QStringLiteral("image/webp"), QStringLiteral("image/gif")};
        for (const auto &id : package.spine) {
            if (!package.manifest.contains(id) || !accepted.contains(package.manifest.value(id).type)) {
                result.error = invalidBookMessage(); return result;
            }
            const auto item = package.manifest.value(id);
            const QString resolved = resolveArchivePath(item.href, container.rootFile, &error);
            if (!error.isEmpty() || zip_name_locate(archive.get(), resolved.toUtf8().constData(), ZIP_FL_ENC_GUESS) < 0) {
                result.error = error.isEmpty() ? invalidBookMessage() : error; return result;
            }
            document->spine.append({resolved, item.type});
        }
        const QString title = package.title.trimmed();
        if (!title.isEmpty()) document->title = title;
        document->format = Document::Format::Epub;
        document->packagePath = container.rootFile;
        document->sections = document->spine.size();
    }
    if (document->title.isEmpty()) document->title = QFileInfo(path).fileName();
    document->title = document->title.left(200);
    result.document = std::move(document);
    return result;
}

struct TextPageLayout {
    int consumed = 0;
    double height = 0;
    QString summary;
};

QFont readerFont(const Preferences &preferences)
{
    QFont font(preferences.fontName);
    font.setPixelSize(std::max(1, int(std::round(preferences.fontSize))));
    font.setHintingPreference(QFont::PreferFullHinting);
    return font;
}

TextPageLayout layoutTextPage(const QString &text, int start, const Preferences &preferences,
                              QPainter *painter = nullptr, QRectF area = {})
{
    TextPageLayout result;
    if (start < 0 || start >= text.size()) return result;
    const int maximum = std::min(16384, int(text.size()) - start);
    QString chunk = text.mid(start, maximum);
    if (!chunk.isEmpty() && chunk.back().isHighSurrogate() && start + chunk.size() < text.size()) chunk.chop(1);
    QTextLayout layout(chunk, readerFont(preferences));
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setUseDesignMetrics(true);
    layout.setTextOption(option);
    const double width = area.isValid() ? area.width() : std::max(1.0, logicalWidth - preferences.margin * 2);
    const double height = area.isValid() ? area.height() : std::max(1.0, logicalHeight - preferences.margin * 2);
    double y = 0;
    layout.beginLayout();
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(width);
        const double lineHeight = line.height();
        if (result.consumed > 0 && y + lineHeight > height + 0.01) {
            line.setPosition(QPointF(0, height + lineHeight));
            break;
        }
        line.setPosition(QPointF(0, y));
        result.consumed = line.textStart() + line.textLength();
        result.height = y + lineHeight;
        y += lineHeight + preferences.lineSpacing;
        if (y > height + preferences.lineSpacing) break;
    }
    layout.endLayout();
    if (result.consumed <= 0 && !chunk.isEmpty()) result.consumed = 1;
    if (result.consumed < chunk.size() && result.consumed > 0
        && chunk.at(result.consumed).isLowSurrogate() && chunk.at(result.consumed - 1).isHighSurrogate())
        --result.consumed;
    result.summary = chunk.left(std::min(80, result.consumed)).simplified();
    if (painter && result.consumed > 0) {
        painter->save();
        painter->setClipRect(area);
        layout.draw(painter, area.topLeft());
        painter->restore();
    }
    return result;
}

int previousTextStart(const QString &text, int current, const Preferences &preferences)
{
    if (current <= 0) return -1;
    int position = 0;
    for (int count = 0; count < 4096 && position < current; ++count) {
        const int consumed = layoutTextPage(text, position, preferences).consumed;
        if (consumed <= 0) break;
        const int next = std::min(int(text.size()), position + consumed);
        if (next >= current) return position;
        position = next;
    }
    const int probe = std::max(0, current - 16384);
    const int amount = std::max(1, layoutTextPage(text, probe, preferences).consumed);
    return std::max(0, current - amount);
}

int lastTextPageStart(const QString &text, const Preferences &preferences)
{
    if (text.isEmpty()) return 0;
    int position = 0;
    for (int count = 0; count < 4096; ++count) {
        const int consumed = layoutTextPage(text, position, preferences).consumed;
        if (consumed <= 0 || position + consumed >= text.size()) return position;
        position += consumed;
    }
    return std::max(0, int(text.size()) - std::max(1, layoutTextPage(text, std::max(0, int(text.size()) - 16384), preferences).consumed));
}

QRectF fitRect(const QSizeF &source, const QRectF &target)
{
    if (source.width() <= 0 || source.height() <= 0) return {};
    const double scale = std::min(target.width() / source.width(), target.height() / source.height());
    const QSizeF size(source.width() * scale, source.height() * scale);
    return QRectF(target.center().x() - size.width() / 2, target.center().y() - size.height() / 2,
                  size.width(), size.height());
}

struct ImageView {
    double zoom = 1;
    QPointF pan;
};

class RenderContext
{
public:
    RenderContext(std::shared_ptr<Document> document, Preferences preferences)
        : m_document(std::move(document)), m_preferences(std::move(preferences))
    {
        if (m_document->format == Document::Format::Pdf) {
            const auto status = m_pdf.load(m_document->path);
            if (status != QPdfDocument::Error::None) m_error = status == QPdfDocument::Error::IncorrectPassword
                ? encryptedMessage() : invalidBookMessage();
        } else if (m_document->format == Document::Format::Epub) {
            m_archive = openArchive(m_document->path, &m_error);
        }
    }

    bool valid() const { return m_error.isEmpty(); }
    const QString &error() const { return m_error; }

    QList<Block> blocks(int section)
    {
        if (section < 0 || section >= m_document->sections) { m_error = invalidBookMessage(); return {}; }
        if (m_blocks.contains(section)) return m_blocks.value(section);
        QList<Block> result;
        if (m_document->format == Document::Format::Text) {
            result.append({Block::Kind::Text, m_document->text});
        } else if (m_document->format == Document::Format::Epub) {
            const auto item = m_document->spine.value(section);
            if (item.type.startsWith(QStringLiteral("image/"))) {
                result.append({Block::Kind::Image, item.path});
            } else {
                QString error;
                const auto parsed = parseXml(archiveData(m_archive.get(), item.path, 8 * 1024 * 1024, &error), &error);
                if (!error.isEmpty()) { m_error = error; return {}; }
                for (auto block : parsed.blocks) {
                    if (block.kind == Block::Kind::Image) {
                        block.value = resolveArchivePath(block.value, item.path, &error);
                        if (!error.isEmpty() || zip_name_locate(m_archive.get(), block.value.toUtf8().constData(), ZIP_FL_ENC_GUESS) < 0) {
                            m_error = error.isEmpty() ? invalidBookMessage() : error;
                            return {};
                        }
                    }
                    result.append(std::move(block));
                }
            }
        }
        if (result.isEmpty()) m_error = invalidBookMessage();
        else m_blocks.insert(section, result);
        return result;
    }

    Location normalized(Location location)
    {
        location.section = std::clamp(location.section, 0, std::max(0, m_document->sections - 1));
        if (m_document->format == Document::Format::Pdf) return {location.section, 0, 0};
        const auto contents = blocks(location.section);
        if (!m_error.isEmpty()) return {};
        location.block = std::clamp(location.block, 0, std::max(0, int(contents.size()) - 1));
        if (contents.at(location.block).kind == Block::Kind::Text) {
            const auto &text = contents.at(location.block).value;
            location.character = std::clamp(location.character, 0, std::max(0, int(text.size()) - 1));
            if (location.character > 0 && text.at(location.character).isLowSurrogate()) --location.character;
        } else {
            location.character = 0;
        }
        return location;
    }

    Location locationAt(double progress)
    {
        const double amount = std::clamp(progress, 0.0, 0.999999);
        const int section = std::min(m_document->sections - 1, int(amount * m_document->sections));
        if (m_document->format == Document::Format::Pdf) return {section, 0, 0};
        const auto contents = blocks(section);
        if (contents.isEmpty()) return {};
        const double withinSection = amount * m_document->sections - section;
        const int block = std::min(int(contents.size()) - 1, int(withinSection * contents.size()));
        Location result{section, block, 0};
        if (contents.at(block).kind == Block::Kind::Text) {
            const QString &text = contents.at(block).value;
            const double withinBlock = withinSection * contents.size() - block;
            result.character = std::clamp(int(withinBlock * text.size()), 0, std::max(0, int(text.size()) - 1));
            if (result.character > 0) {
                const int begin = std::max(0, result.character - 200);
                const int newline = text.lastIndexOf('\n', result.character - 1);
                if (newline >= begin) result.character = newline + 1;
                if (result.character < text.size() && text.at(result.character).isLowSurrogate()) --result.character;
            }
        }
        return normalized(result);
    }

    Page render(Location requested, ImageView view)
    {
        Page page;
        page.location = normalized(requested);
        if (!m_error.isEmpty()) return page;
        QImage canvas(imageWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(QColor(QStringLiteral("#EFEDE6")));
        QPainter painter(&canvas);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const double scaleX = imageWidth / logicalWidth;
        const double scaleY = imageHeight / logicalHeight;
        painter.scale(scaleX, scaleY);
        const QRectF logicalPage(0, 0, logicalWidth, logicalHeight);
        const QRectF inner = logicalPage.adjusted(m_preferences.margin, m_preferences.margin,
                                                  -m_preferences.margin, -m_preferences.margin);
        if (m_document->format == Document::Format::Pdf) {
            page.illustration = true;
            page.pageCount = m_document->sections;
            const QSizeF points = m_pdf.pagePointSize(page.location.section);
            const QRectF base = fitRect(points, inner);
            if (!base.isValid()) { m_error = invalidBookMessage(); return page; }
            const QPointF center = logicalPage.center();
            QRectF destination(center.x() + (base.x() - center.x()) * view.zoom + view.pan.x(),
                               center.y() + (base.y() - center.y()) * view.zoom + view.pan.y(),
                               base.width() * view.zoom, base.height() * view.zoom);
            if (view.zoom <= 1.0001) {
                const QSize renderSize(std::max(1, int(std::ceil(destination.width() * scaleX))),
                                       std::max(1, int(std::ceil(destination.height() * scaleY))));
                const QImage image = m_pdf.render(page.location.section, renderSize);
                if (image.isNull()) { m_error = invalidBookMessage(); return page; }
                painter.fillRect(destination, Qt::white);
                painter.drawImage(destination, image);
            } else {
                painter.end();
                const QRectF target(destination.x() * scaleX, destination.y() * scaleY,
                                    destination.width() * scaleX, destination.height() * scaleY);
                const QRect visible = target.toAlignedRect().intersected(canvas.rect());
                if (!visible.isEmpty()) {
                    QRect clip = QRectF(visible.x() - target.x(), visible.y() - target.y(), visible.width(), visible.height()).toAlignedRect();
                    const QSize full(std::max(1, int(std::round(target.width()))), std::max(1, int(std::round(target.height()))));
                    clip = clip.intersected(QRect(QPoint(), full));
                    QPdfDocumentRenderOptions options;
                    options.setScaledSize(full);
                    options.setScaledClipRect(clip);
                    const QImage image = m_pdf.render(page.location.section, clip.size(), options);
                    if (image.isNull()) { m_error = invalidBookMessage(); return page; }
                    QPainter physical(&canvas);
                    physical.fillRect(visible, Qt::white);
                    physical.drawImage(visible.topLeft(), image);
                }
            }
            page.next = page.location.section + 1 < m_document->sections
                ? std::optional<Location>{Location{page.location.section + 1, 0, 0}} : std::nullopt;
            page.previous = page.location.section > 0
                ? std::optional<Location>{Location{page.location.section - 1, 0, 0}} : std::nullopt;
            page.progress = double(page.location.section) / std::max(1, m_document->sections - 1);
            page.summary = QStringLiteral("%1 / %2").arg(page.location.section + 1).arg(m_document->sections);
        } else {
            const auto contents = blocks(page.location.section);
            if (!m_error.isEmpty() || page.location.block >= contents.size()) return page;
            const auto &block = contents.at(page.location.block);
            double portion = 0;
            if (block.kind == Block::Kind::Image) {
                page.illustration = true;
                QString error;
                const QByteArray bytes = archiveData(m_archive.get(), block.value, maximumEntryBytes, &error);
                if (!error.isEmpty()) { m_error = error; return page; }
                QBuffer buffer;
                buffer.setData(bytes);
                buffer.open(QIODevice::ReadOnly);
                QImageReader reader(&buffer);
                reader.setAutoTransform(true);
                const QSize original = reader.size();
                if (!original.isValid() || qint64(original.width()) * original.height() > qint64(268) * 1024 * 1024) {
                    m_error = tooLargeMessage(); return page;
                }
                QSize decoded = original;
                const int maximum = std::min(2048, std::max(1200, int(std::round(1200 * view.zoom))));
                if (decoded.width() > maximum || decoded.height() > maximum) {
                    decoded.scale(maximum, maximum, Qt::KeepAspectRatio);
                    reader.setScaledSize(decoded);
                }
                const QImage image = reader.read();
                if (image.isNull()) { m_error = invalidBookMessage(); return page; }
                const QRectF base = fitRect(image.size(), inner);
                const QPointF center = logicalPage.center();
                const QRectF destination(center.x() + (base.x() - center.x()) * view.zoom + view.pan.x(),
                                         center.y() + (base.y() - center.y()) * view.zoom + view.pan.y(),
                                         base.width() * view.zoom, base.height() * view.zoom);
                painter.drawImage(destination, image);
            } else {
                painter.setPen(QColor(QStringLiteral("#222222")));
                if (view.zoom > 1.0001) {
                    const double offsetX = scaleX * ((1 - view.zoom) * logicalWidth / 2 + view.pan.x());
                    const double offsetY = scaleY * ((1 - view.zoom) * logicalHeight / 2 + view.pan.y());
                    painter.setTransform(QTransform(scaleX * view.zoom, 0, 0, scaleY * view.zoom,
                                                    offsetX, offsetY));
                }
                const auto layout = layoutTextPage(block.value, page.location.character, m_preferences, &painter, inner);
                if (layout.consumed <= 0) { m_error = invalidBookMessage(); return page; }
                page.textHeight = std::min(inner.height(), std::max(1.0, layout.height));
                page.summary = layout.summary;
                portion = double(page.location.character) / std::max(1, int(block.value.size()));
                if (page.location.character + layout.consumed < block.value.size())
                    page.next = Location{page.location.section, page.location.block, page.location.character + layout.consumed};
            }
            if (!page.next) {
                if (page.location.block + 1 < contents.size()) page.next = Location{page.location.section, page.location.block + 1, 0};
                else if (page.location.section + 1 < m_document->sections) page.next = Location{page.location.section + 1, 0, 0};
            }
            if (block.kind == Block::Kind::Text && page.location.character > 0) {
                const int previous = previousTextStart(block.value, page.location.character, m_preferences);
                if (previous >= 0) page.previous = Location{page.location.section, page.location.block, previous};
            } else if (page.location.block > 0) {
                const auto &before = contents.at(page.location.block - 1);
                page.previous = Location{page.location.section, page.location.block - 1,
                    before.kind == Block::Kind::Text ? lastTextPageStart(before.value, m_preferences) : 0};
            } else if (page.location.section > 0) {
                const auto previousBlocks = blocks(page.location.section - 1);
                if (!previousBlocks.isEmpty()) {
                    const int blockIndex = previousBlocks.size() - 1;
                    const auto &before = previousBlocks.at(blockIndex);
                    page.previous = Location{page.location.section - 1, blockIndex,
                        before.kind == Block::Kind::Text ? lastTextPageStart(before.value, m_preferences) : 0};
                }
            }
            page.progress = page.next ? (double(page.location.section)
                + (double(page.location.block) + portion) / std::max(1, int(contents.size()))) / m_document->sections : 1;
        }
        if (painter.isActive()) painter.end();
        page.image = std::move(canvas);
        return page;
    }

private:
    std::shared_ptr<Document> m_document;
    Preferences m_preferences;
    QPdfDocument m_pdf;
    ZipHandle m_archive;
    QHash<int, QList<Block>> m_blocks;
    QString m_error;
};

ReaderController::RenderResult renderDocument(const std::shared_ptr<Document> &document,
                                               const Preferences &preferences,
                                               std::optional<Location> requested,
                                               std::optional<double> progress,
                                               bool detail, double zoom, QPointF pan)
{
    ReaderController::RenderResult result;
    result.detail = detail;
    result.zoom = zoom;
    result.pan = pan;
    if (!document) { result.error = unavailableMessage(); return result; }
    RenderContext context(document, preferences);
    if (!context.valid()) { result.error = context.error(); return result; }
    Location location = progress ? context.locationAt(*progress) : context.normalized(requested.value_or(Location{}));
    if (!context.error().isEmpty()) { result.error = context.error(); return result; }
    Page current = context.render(location, detail ? ImageView{zoom, pan} : ImageView{});
    if (!context.error().isEmpty() || current.image.isNull()) {
        result.error = context.error().isEmpty() ? invalidBookMessage() : context.error(); return result;
    }
    result.pages.append(current);
    if (detail) return result;
    const int ahead = preferences.vertical ? 3 : 1;
    Page last = current;
    for (int index = 0; index < ahead && last.next; ++index) {
        Page following = context.render(*last.next, {});
        if (!context.error().isEmpty() || following.image.isNull()) break;
        if (std::any_of(result.pages.cbegin(), result.pages.cend(), [&](const Page &page) { return page.location == following.location; })) break;
        result.pages.append(following);
        last = following;
    }
    if (current.previous && std::none_of(result.pages.cbegin(), result.pages.cend(), [&](const Page &page) { return page.location == *current.previous; })) {
        Page previous = context.render(*current.previous, {});
        if (context.error().isEmpty() && !previous.image.isNull()) result.pages.append(previous);
    }
    return result;
}
}

