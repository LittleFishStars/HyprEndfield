#pragma once
#include "watchanimationcontroller.hpp"
#include <QQuickFramebufferObject>
#include <QVariantMap>
#include <QImage>
#include <QUrl>

// OpenGL owns only the source decorative subtree. Interactive QML remains
// independent and uses the same source camera through MenuPlane.
class SourceDial : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(WatchAnimationController* animation MEMBER animation NOTIFY animationChanged)
    Q_PROPERTY(QVariantMap pose READ pose WRITE setPose NOTIFY poseChanged)
    Q_PROPERTY(QUrl logo READ logo WRITE setLogo NOTIFY logoChanged)
public:
    explicit SourceDial(QQuickItem* parent = nullptr);
    Renderer* createRenderer() const override;
    QVariantMap pose() const { return m_pose; }
    void setPose(const QVariantMap& pose);
    QUrl logo() const { return m_logo; }
    void setLogo(const QUrl &url);
    QImage logoImage() const { return m_logoImage; }
    WatchAnimationController* animation = nullptr;
signals:
    void animationChanged();
    void poseChanged();
    void logoChanged();

private:
    QVariantMap m_pose;
    QUrl m_logo;
    QImage m_logoImage;
};
