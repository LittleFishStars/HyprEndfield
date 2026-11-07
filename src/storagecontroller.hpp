#pragma once
#include <QObject>
#include <QVariantMap>
#include <QFutureWatcher>
#include <QTimer>

class StorageController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    explicit StorageController(QObject *parent = nullptr);
    ~StorageController() override;
    QVariantMap state() const;
    bool active() const { return m_active; }
    void setActive(bool active);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectVolume(const QString &id);
    Q_INVOKABLE bool openDolphin();
    static QVariantList readVolumes();
signals:
    void changed();
    void activeChanged();
private:
    QFutureWatcher<QVariantList> m_reader;
    QTimer m_timer;
    QVariantList m_volumes;
    QString m_selection;
    bool m_active = false;
};
