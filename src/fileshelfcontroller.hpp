#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>

class QQuickWindow;

// EndfieldHUD FileShelfStore + HUDFileShelfInteraction. Only references and
// metadata are persisted; none of these operations modifies an original file.
class FileShelfController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap properties READ properties NOTIFY propertiesChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    explicit FileShelfController(QObject *parent = nullptr, QString directory = {});
    QVariantMap state() const;
    QVariantMap properties() const { return m_properties; }
    bool active() const { return m_active; }
    void setActive(bool value);
    void setWindow(QQuickWindow *window);

    Q_INVOKABLE bool addUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void pasteFiles();
    Q_INVOKABLE bool removeItems(const QStringList &ids);
    Q_INVOKABLE bool clear();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void readProperties(const QString &id);
    Q_INVOKABLE bool openItem(const QString &id);
    Q_INVOKABLE void revealItem(const QString &id);
    Q_INVOKABLE void beginDrag(const QStringList &ids);

    struct Entry {
        QString id, path, name, kind, extension, error;
        QString device, inode;
        qint64 bytes = 0;
        bool directory = false;
    };
    // Also used to verify native drag payloads without a desktop drag session.
    QList<QUrl> exportUrls(const QStringList &ids, QString *error) const;

signals:
    void stateChanged();
    void propertiesChanged();
    void activeChanged();
    void itemsAdded(const QStringList &ids);
    void actionFinished(const QString &action, bool success, const QString &message);
    void externalOpened();
    void dragStarted();
    void dragFinished(bool delivered);

private:
    void load();
    bool save(const QList<Entry> &entries);
    bool fail(const QString &action, const QString &message);
    const Entry *find(const QString &id) const;
    static Entry inspect(const QString &path, const Entry *previous = nullptr);
    static QVariantMap describe(const Entry &entry);

    QString m_directory, m_path, m_error;
    QByteArray m_persisted;
    bool m_fileExisted = false;
    bool m_ready = true, m_active = false, m_busy = false, m_dragging = false;
    QList<Entry> m_entries;
    QVariantMap m_properties;
    quint64 m_propertiesRequest = 0;
    QPointer<QQuickWindow> m_window;
};
