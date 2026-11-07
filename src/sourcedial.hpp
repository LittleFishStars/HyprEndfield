#pragma once
#include "watchanimationcontroller.hpp"
#include <QQuickFramebufferObject>
#include <QVariantMap>

// OpenGL owns only the source decorative subtree. Interactive QML remains
// independent and uses the same source camera through MenuPlane.
class SourceDial : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(WatchAnimationController* animation MEMBER animation NOTIFY animationChanged)
    Q_PROPERTY(QVariantMap pose READ pose WRITE setPose NOTIFY poseChanged)
public:
    explicit SourceDial(QQuickItem* parent = nullptr);
    Renderer* createRenderer() const override;
    QVariantMap pose() const { return m_pose; }
    void setPose(const QVariantMap& pose);
    WatchAnimationController* animation = nullptr;
signals:
    void animationChanged();
    void poseChanged();

private:
    QVariantMap m_pose;
};
