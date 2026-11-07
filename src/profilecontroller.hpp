#pragma once
#include <QFutureWatcher>
#include <QImage>
#include <QQuickPaintedItem>
#include <QVariantMap>
#include <QUrl>

class ProfileController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
    explicit ProfileController(QObject *parent = nullptr, QString directory = {});
    ~ProfileController() override;
    QVariantMap state() const;
    Q_INVOKABLE bool setField(const QString &field, const QVariant &value, bool commit = true);
    Q_INVOKABLE void importImage(const QString &kind, const QUrl &url);
    Q_INVOKABLE bool resetImage(const QString &kind);
    const QImage &image(const QString &kind) const { return kind == "avatar" ? m_avatar : m_background; }
signals:
    void changed();
private:
    bool save();
    QString m_directory, m_path, m_error, m_importKind, m_pendingImage;
    QByteArray m_persisted;
    QVariantMap m_values, m_saved;
    QImage m_avatar, m_background;
    QFutureWatcher<QVariantMap> m_import;
    bool m_readable = true;
};

// Portrait crop and backdrop geometry from HUDPortraitArtwork / PersonalProfileCanvas.
class ProfileArtwork : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(ProfileController *controller READ controller WRITE setController NOTIFY appearanceChanged)
    Q_PROPERTY(QString kind MEMBER m_kind NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accent MEMBER m_accent NOTIFY appearanceChanged)
public:
    explicit ProfileArtwork(QQuickItem *parent = nullptr);
    ProfileController *controller() const { return m_controller; }
    void setController(ProfileController *value);
    void paint(QPainter *) override;
signals:
    void appearanceChanged();
private:
    ProfileController *m_controller = nullptr;
    QString m_kind = "avatar";
    QColor m_accent = Qt::yellow;
};
