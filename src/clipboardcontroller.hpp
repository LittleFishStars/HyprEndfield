#pragma once

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QSize>
#include <QTimer>
#include <QVariantMap>

class ClipboardController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
public:
    explicit ClipboardController(QObject *parent = nullptr, QString executable = {}, bool automatic = true);
    ~ClipboardController() override;

    bool active() const { return m_active; }
    void setActive(bool active);
    QVariantMap state() const;

    Q_INVOKABLE bool copyItem(const QString &id);
    Q_INVOKABLE bool togglePin(const QString &id);
    Q_INVOKABLE bool removeItem(const QString &id);
    Q_INVOKABLE bool clearUnpinned();

    // Accepts the CopyQ query result without starting a process; used by the
    // isolated contract test and by the asynchronous query path.
    bool acceptSnapshot(const QByteArray &json);

signals:
    void activeChanged();
    void stateChanged();
    void actionFinished(const QString &action, bool success, const QString &message);

private:
    void requestRefresh();
    void startRefresh();
    void finishRefresh(int exitCode, QProcess::ExitStatus status);
    void startMutation(const QString &action, const QString &script, bool refresh = true);
    void stampNewItems(const QVariantList &items, int previousTotal, int currentTotal);
    void requestNextThumbnail();
    void startThumbnail(const QVariantMap &item);
    void finishThumbnail(int exitCode, QProcess::ExitStatus status);
    QVariantMap item(const QString &id) const;
    static QString snapshotScript();
    static QString mutationPrefix(const QString &id, int row);
    static QString jsString(const QString &value);

    QString m_executable;
    bool m_automatic = true;
    bool m_active = false;
    bool m_refreshPending = false;
    bool m_initialised = false;
    int m_lastTotal = 0;
    QVariantMap m_state;
    QVariantList m_items;
    QSet<QString> m_seenIds;
    QHash<QString, QString> m_thumbnails;
    QHash<QString, QSize> m_imageSizes;
    QSet<QString> m_thumbnailFailures;
    QSet<QString> m_thumbnailPending;
    QTimer m_timer;
    QProcess m_refreshProcess;
    QByteArray m_refreshOutput;
    QProcess *m_thumbnailProcess = nullptr;
    QByteArray m_thumbnailOutput;
    QString m_thumbnailId;
};
