#pragma once

#include <QHash>
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <memory>
#include <optional>

class ReaderImageProvider final : public QQuickImageProvider
{
public:
    ReaderImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    void insert(const QString &key, const QImage &image);
    void clear();

private:
    QMutex m_mutex;
    QHash<QString, QImage> m_images;
    QList<QString> m_order;
};

// EndfieldHUD ReaderController / ReaderStore for Linux. The library only
// persists references, progress, bookmarks and typography; books are read in
// place and never copied or modified.
class ReaderController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    struct Location {
        int section = 0;
        int block = 0;
        int character = 0;
        bool valid() const;
        bool operator==(const Location &) const = default;
    };
    struct Preferences {
        QString fontName = QStringLiteral("Georgia");
        double fontSize = 10;
        double lineSpacing = 2;
        double margin = 16;
        bool vertical = true;
        bool valid() const;
        bool operator==(const Preferences &) const = default;
    };
    struct Bookmark {
        QString id;
        Location location;
        double progress = 0;
    };
    struct Book {
        QString id;
        QString path;
        QString title;
        QString format;
        QString device;
        QString inode;
        Location location;
        double progress = 0;
        QList<Bookmark> bookmarks;
    };
    struct SpineItem {
        QString path;
        QString type;
    };
    struct Document {
        enum class Format { Pdf, Epub, Text } format = Format::Text;
        QString path;
        QString title;
        int sections = 1;
        QString text;
        QString packagePath;
        QList<SpineItem> spine;
    };
    struct Page {
        Location location;
        std::optional<Location> next;
        std::optional<Location> previous;
        double progress = 0;
        QString summary;
        bool illustration = false;
        int pageCount = 0;
        double textHeight = 0;
        QImage image;
        QString imageKey;
    };
    struct OpenResult {
        std::shared_ptr<Document> document;
        QString error;
    };
    struct RenderResult {
        QList<Page> pages;
        QString error;
        bool detail = false;
        double zoom = 1;
        QPointF pan;
    };

    explicit ReaderController(QObject *parent = nullptr, ReaderImageProvider *provider = nullptr,
                              QString directory = {});

    QVariantMap state() const;
    bool active() const { return m_active; }
    void setActive(bool value);

    Q_INVOKABLE bool importUrl(const QUrl &url);
    Q_INVOKABLE bool selectBook(const QString &id);
    Q_INVOKABLE bool removeBook(const QString &id);
    Q_INVOKABLE bool relinkBook(const QString &id, const QUrl &url);
    Q_INVOKABLE void setTab(const QString &tab);
    Q_INVOKABLE void turn(int direction);
    Q_INVOKABLE void jump(int page);
    Q_INVOKABLE void toggleBookmark();
    Q_INVOKABLE void goBookmark(const QString &id);
    Q_INVOKABLE void removeBookmark(const QString &id);
    Q_INVOKABLE void setPreference(const QString &name, const QVariant &value);
    Q_INVOKABLE void setImageView(double zoom, double panX, double panY);
    Q_INVOKABLE void zoomAt(double factor, double x, double y);
    Q_INVOKABLE void panBy(double dx, double dy);
    Q_INVOKABLE void resetZoom();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList fontFamilies() const;

signals:
    void stateChanged();
    void activeChanged();
    void actionFinished(const QString &action, bool success, const QString &message);

private:
    struct RenderRequest {
        std::optional<Location> location;
        std::optional<double> progress;
        bool detail = false;
    };

    void loadLibrary();
    bool saveLibrary();
    bool fail(const QString &action, const QString &message);
    Book *findBook(const QString &id);
    const Book *findBook(const QString &id) const;
    Book *selectedBook();
    const Book *selectedBook() const;
    bool bookAvailable(const Book &book, QString *message = nullptr) const;
    void openSelected();
    void requestRender(std::optional<Location> location = {}, std::optional<double> progress = {}, bool detail = false);
    void startRender();
    void applyRender(RenderResult result, quint64 token);
    void persistProgress(const Page &page);
    void clearDocument(bool keepPage = false);
    void resetImageViewInternal();
    QVariantMap describeBook(const Book &book, int number) const;
    QVariantMap describePage(const Page &page, bool current) const;

    QString m_directory;
    QString m_path;
    QByteArray m_persisted;
    bool m_fileExisted = false;
    bool m_ready = true;
    bool m_active = false;
    bool m_opening = false;
    bool m_rendering = false;
    bool m_reading = false;
    QString m_error;
    QString m_selected;
    QString m_pageBook;
    QList<Book> m_books;
    Preferences m_preferences;
    std::shared_ptr<Document> m_document;
    QList<Page> m_pages;
    std::optional<Page> m_detail;
    QList<Location> m_history;
    std::optional<RenderRequest> m_pendingRender;
    quint64 m_generation = 0;
    quint64 m_imageSerial = 0;
    double m_zoom = 1;
    QPointF m_pan;
    ReaderImageProvider *m_provider = nullptr;
    QTimer m_detailTimer;
};

