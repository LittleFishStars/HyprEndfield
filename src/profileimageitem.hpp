#pragma once
#include <QFutureWatcher>
#include <QImage>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <functional>

// ProfileOwner::scheduleCard: one running render and only the latest pending
// request. Workers own image snapshots; they never access a controller or item.
class ProfileImageItem : public QQuickPaintedItem {
    Q_OBJECT
public:
    explicit ProfileImageItem(QQuickItem *parent = nullptr);
    bool isRendering() const { return m_busy || m_scheduled; }
signals:
    void imageReady();
protected:
    // key contains every pixel-affecting value. An optional source groups
    // geometry-only changes so completed previews can appear during a drag;
    // changing the source discards any in-flight result from the old image.
    void requestImage(QVariantList key, std::function<QImage()> render, QVariantList source = {});
    void clearImage();
    const QImage &renderedImage() const { return m_image; }
    bool imageIsCurrent() const { return !m_published.isEmpty() && m_published == m_requested; }
private:
    struct Result { QImage image; QString error; };
    void schedule();
    QFutureWatcher<Result> m_render;
    QVariantList m_requested, m_running, m_published;
    QVariantList m_source, m_runningSource;
    std::function<QImage()> m_pending;
    QImage m_image;
    bool m_busy = false, m_scheduled = false;
};
