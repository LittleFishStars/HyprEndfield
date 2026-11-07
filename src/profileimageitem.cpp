#include "profileimageitem.hpp"
#include <QDebug>
#include <QTimer>
#include <QtConcurrentRun>
#include <exception>
#include <utility>

ProfileImageItem::ProfileImageItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    connect(&m_render, &QFutureWatcherBase::finished, this, [this] {
        m_busy = false;
        const auto result = m_render.result();
        const bool latest = m_running == m_requested;
        if (latest) m_pending = {};
        // Keep a drag preview moving while newer geometry waits. A replaced
        // image/controller/recording mode must never receive the old result.
        const bool preview = !m_runningSource.isEmpty() && m_runningSource == m_source && m_requested != m_published;
        if (latest || preview) {
            if (!result.error.isEmpty()) {
                qWarning().noquote() << "Cannot render profile image:" << result.error;
            } else {
                m_image = result.image;
                m_published = m_running;
                update();
                emit imageReady();
            }
        }
        schedule();
    });
}

void ProfileImageItem::requestImage(QVariantList key, std::function<QImage()> render, QVariantList source) {
    if (key == m_requested) return;
    m_requested = std::move(key);
    m_source = std::move(source);
    if (m_requested == m_published) {
        m_pending = {};
        update();
        return;
    }
    m_pending = std::move(render);
    schedule();
}

void ProfileImageItem::clearImage() {
    m_requested.clear();
    m_source.clear();
    m_pending = {};
    if (m_image.isNull() && m_published.isEmpty()) return;
    m_image = {};
    m_published.clear();
    update();
    emit imageReady();
}

void ProfileImageItem::schedule() {
    if (m_busy || m_scheduled || !m_pending) return;
    m_scheduled = true;
    // Combine property bindings and pointer events delivered in the same turn.
    QTimer::singleShot(0, this, [this] {
        m_scheduled = false;
        if (m_busy || !m_pending) return;
        m_running = m_requested;
        m_runningSource = m_source;
        m_busy = true;
        auto render = std::exchange(m_pending, {});
        m_render.setFuture(QtConcurrent::run([render = std::move(render)]() -> Result {
            try {
                return {render(), {}};
            } catch (const std::exception &error) {
                return {{}, QString::fromUtf8(error.what())};
            }
        }));
    });
}
