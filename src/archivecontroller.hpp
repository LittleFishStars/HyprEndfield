#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <memory>

class QQuickTextDocument;

class ArchivePasswordVault
{
public:
    virtual ~ArchivePasswordVault() = default;
    virtual QByteArray read(const QString &account, bool *found, QString *error) = 0;
    virtual bool add(const QString &account, const QByteArray &value, QString *error) = 0;
    virtual bool remove(const QString &account, QString *error) = 0;
};

class ArchiveMemoryPasswordVault final : public ArchivePasswordVault
{
public:
    QByteArray read(const QString &account, bool *found, QString *error) override;
    bool add(const QString &account, const QByteArray &value, QString *error) override;
    bool remove(const QString &account, QString *error) override;
    void forget(const QString &account);
private:
    QMutex m_mutex;
    QHash<QString, QByteArray> m_values;
};

// EndfieldHUD ArchiveStore + ArchiveClassification. Documents are stored in
// Archive/archive.sqlite; classified password verifiers stay in Secret Service.
class ArchiveController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    explicit ArchiveController(QObject *parent = nullptr, QString directory = {},
        std::shared_ptr<ArchivePasswordVault> vault = {});
    ~ArchiveController() override;

    QVariantMap state() const;
    bool active() const { return m_active; }
    void setActive(bool value);

    Q_INVOKABLE bool selectCategory(const QString &id);
    Q_INVOKABLE QString addCategory();
    Q_INVOKABLE bool renameCategory(const QString &id, const QString &name);
    Q_INVOKABLE bool removeCategory(const QString &id);
    Q_INVOKABLE QString createDocument();
    Q_INVOKABLE bool selectDocument(const QString &id);
    Q_INVOKABLE void closeDocument();
    Q_INVOKABLE bool updateSelected(const QString &title, const QString &body, const QString &richBody);
    Q_INVOKABLE bool moveSelected(const QString &categoryId);
    Q_INVOKABLE bool deleteSelected();
    Q_INVOKABLE bool importMedia(const QList<QUrl> &urls);
    Q_INVOKABLE bool removeMedia(int index);
    Q_INVOKABLE void classifySelected(const QString &password, const QString &confirmation);
    Q_INVOKABLE void unlockSelected(const QString &password);
    Q_INVOKABLE void declassifySelected();
    Q_INVOKABLE void authenticateOwner(const QString &linuxPassword);
    Q_INVOKABLE void recoverSelected(const QString &newPassword, const QString &confirmation, bool declassify);
    Q_INVOKABLE void cancelRecovery();
    Q_INVOKABLE QVariantMap formatState(QObject *quickDocument, int position) const;
    Q_INVOKABLE bool applyFormat(QObject *quickDocument, int start, int end,
        const QString &key, const QVariant &value);

signals:
    void stateChanged();
    void activeChanged();
    void actionFinished(const QString &action, bool success, const QString &message);
    void unlockFinished(bool success, bool wrongPassword, const QString &message);
    void ownerAuthenticationFinished(bool success, const QString &message);
    void recoveryFinished(bool success, const QString &message);

public:
    struct Category {
        QString id, name;
        qint64 created = 0;
        bool viewed = true;
    };
    struct Media {
        QString id, path, name, kind, device, inode;
    };
    struct Entry {
        QString id, title, body, richBody, categoryId, keyId;
        qint64 date = 0, modified = 0;
        QList<Media> media;
    };

private:
    void load();
    bool createSchema();
    bool loadRows();
    bool exec(const QString &statement);
    bool saveEntry(const Entry &entry);
    bool saveStateValue(const QString &key, const QString &value);
    void updateSchemaVersion();
    void scheduleSave();
    void flushSelected();
    void probePasswordStatus(const QString &id, const QString &account);
    void setBusy(bool value);
    bool fail(const QString &action, const QString &message);
    Entry *selectedEntry();
    const Entry *selectedEntry() const;
    const Category *findCategory(const QString &id) const;
    QString classificationAccount(const Entry &entry) const;
    static QVariantMap categoryMap(const Category &category, int count);
    static QVariantMap mediaMap(const Media &media);
    static QString dateLabel(qint64 milliseconds);
    static QString normalizedCategoryName(const QString &name);
    static bool validPassword(const QString &password, const QString &confirmation, QString *message);
    static bool mediaStillAvailable(const Media &media);
    static QList<int> redactionWidths(const QString &body);

    QString m_directory, m_path, m_connectionName;
    QSqlDatabase m_database;
    std::shared_ptr<ArchivePasswordVault> m_vault;
    QList<Category> m_categories;
    QList<Entry> m_entries;
    QString m_filter = QStringLiteral("all");
    QString m_selectedId, m_unlockedId, m_ownerAuthorizedId;
    QHash<QString, QString> m_passwordStatuses;
    QString m_error;
    bool m_ready = false, m_active = false, m_busy = false;
    QTimer m_saveTimer;
};
