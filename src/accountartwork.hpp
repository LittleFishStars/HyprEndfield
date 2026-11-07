#pragma once
#include "profilecontroller.hpp"
#include <QQuickPaintedItem>

// Rasterize only on content changes. The original camera projects these items.
class GameProfileArtwork : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(ProfileController *controller READ controller WRITE setController NOTIFY appearanceChanged)
    Q_PROPERTY(QVariantMap spec MEMBER m_spec NOTIFY appearanceChanged)
    Q_PROPERTY(QColor accent MEMBER m_accent NOTIFY appearanceChanged)
    Q_PROPERTY(bool recording MEMBER m_recording NOTIFY appearanceChanged)
public:
    explicit GameProfileArtwork(QQuickItem *parent = nullptr);
    ProfileController *controller() const { return m_controller; }
    void setController(ProfileController *value);
    void paint(QPainter *) override;
signals:
    void appearanceChanged();
private:
    ProfileController *m_controller = nullptr;
    QVariantMap m_spec;
    QColor m_accent = Qt::yellow;
    bool m_recording = false, m_dirty = true;
    QImage m_image;
};

class AccountGaugeArtwork : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString value MEMBER m_value NOTIFY appearanceChanged)
    Q_PROPERTY(bool highlighted MEMBER m_highlighted NOTIFY appearanceChanged)
public:
    explicit AccountGaugeArtwork(QQuickItem *parent = nullptr);
    void paint(QPainter *) override;
signals:
    void appearanceChanged();
private:
    QString m_value;
    bool m_highlighted = false;
};
