#include "projectioncanvas.hpp"
#include "mediainfo.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>

void ProjectionDots::paint(QPainter *p) {
    // Preserve the source's 2 × 2 squares, 12px origin, 36px spacing and 0.14 alpha.
    QPainterPath dots;
    for (qreal y = 12; y <= height(); y += 36)
        for (qreal x = 12; x <= width(); x += 36) dots.addRect(x, y, 2, 2);
    p->fillPath(dots, QColor::fromRgbF(1, 1, 1, .14));
}

ProjectionCanvas::ProjectionCanvas(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton); setAntialiasing(true);
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { update(); });
    connect(this, &QQuickItem::visibleChanged, this, [this] { if (!isVisible()) { m_drawing = false; m_pending.points.clear(); update(); } });
    connect(&m_import, &QFutureWatcherBase::finished, this, [this] {
        auto item = m_import.result();
        if (item.contains("error")) m_message = item.value("error").toString();
        else if (m_media.size() >= 16) m_message = tr("投影最多添加 16 个媒体文件。");
        else {
            const double w = std::min(360., std::max(110., width() * .4));
            const double h = std::min(std::max(110., height() * .55), std::max(100., w * item.value("height").toDouble() / item.value("width").toDouble() + 52));
            item["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
            item["x"] = (width() - w) / 2; item["y"] = (height() - h) / 2; item["width"] = w; item["height"] = h;
            m_media.append(item); m_message.clear(); emit mediaChanged();
            emit eventOccurred("mediaAdded");
        }
        emit messageChanged();
    });
}
ProjectionCanvas::~ProjectionCanvas() { m_import.waitForFinished(); }
void ProjectionCanvas::paint(QPainter *p) {
    p->setRenderHint(QPainter::Antialiasing);
    const auto draw = [&](const Stroke &s) {
        if (s.points.isEmpty()) return;
        QPen pen(s.color, s.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin); p->setPen(pen);
        QPainterPath path; auto pixel = [&](QPointF v) { return QPointF(v.x() * width(), v.y() * height()); };
        path.moveTo(pixel(s.points.first()));
        for (qsizetype i = 1; i < s.points.size(); ++i) path.lineTo(pixel(s.points[i]));
        if (s.points.size() == 1) path.lineTo(pixel(s.points.first()) + QPointF(.01, 0));
        p->drawPath(path);
    };
    for (const auto &stroke : m_strokes) draw(stroke);
    if (m_drawing && !m_erasing) draw(m_pending);
}
void ProjectionCanvas::sample(QPointF point) {
    if (width() <= 0 || height() <= 0 || m_pending.points.size() >= 16384) return;
    const QPointF normalized(std::clamp(point.x() / width(), 0., 1.), std::clamp(point.y() / height(), 0., 1.));
    if (!m_pending.points.isEmpty()) { const auto d = normalized - m_pending.points.last(); if (std::hypot(d.x() * width(), d.y() * height()) < .8) return; }
    m_pending.points.append(normalized); update();
}
void ProjectionCanvas::erase(QPointF point) {
    const auto before = m_strokes.size();
    m_strokes.removeIf([&](const Stroke &s) {
        auto pixel = [&](QPointF v) { return QPointF(v.x() * width(), v.y() * height()); };
        QPointF previous = pixel(s.points.first()); const double threshold = (m_width + s.width) / 2;
        if (s.points.size() == 1) return QLineF(previous, point).length() <= threshold;
        for (qsizetype i = 1; i < s.points.size(); ++i) {
            const QPointF next = pixel(s.points[i]), d = next - previous;
            const double length = QPointF::dotProduct(d, d);
            const double t = length > 0 ? std::clamp(QPointF::dotProduct(point - previous, d) / length, 0., 1.) : 0;
            if (QLineF(previous + d * t, point).length() <= threshold) return true;
            previous = next;
        }
        return false;
    });
    if (before != m_strokes.size()) { update(); emit contentChanged(); emit eventOccurred("erased"); }
}
void ProjectionCanvas::mousePressEvent(QMouseEvent *e) {
    if (e->button() == Qt::RightButton) { m_erasing = !m_erasing; emit penChanged(); return; }
    beginStroke(e->position());
}
void ProjectionCanvas::beginStroke(QPointF point) {
    m_drawing = true; m_pending = {{}, m_color, std::clamp(m_width, 1., 80.)};
    if (m_erasing) erase(point); else sample(point);
}
void ProjectionCanvas::moveStroke(QPointF point) { if (m_drawing) { if (m_erasing) erase(point); else sample(point); } }
void ProjectionCanvas::mouseMoveEvent(QMouseEvent *e) { moveStroke(e->position()); }
void ProjectionCanvas::mouseReleaseEvent(QMouseEvent *) { endStroke(); }
void ProjectionCanvas::endStroke() {
    if (m_drawing && !m_erasing && !m_pending.points.isEmpty() && m_strokes.size() < 4096) { m_strokes.append(m_pending); emit contentChanged(); emit eventOccurred("drawingEdited"); }
    m_drawing = false; m_pending.points.clear(); update();
}
void ProjectionCanvas::wheelEvent(QWheelEvent *e) { m_width = std::clamp(m_width + e->angleDelta().y() / 120., 1., 80.); emit penChanged(); e->accept(); }
void ProjectionCanvas::clearContent() { const bool changed = !m_strokes.isEmpty() || !m_media.isEmpty(); m_strokes.clear(); m_pending.points.clear(); m_media.clear(); update(); emit contentChanged(); emit mediaChanged(); if (changed) emit eventOccurred("cleared"); }
void ProjectionCanvas::importMedia(const QUrl &url) {
    if (m_import.isRunning()) return;
    if (m_media.size() >= 16) { m_message = tr("投影最多添加 16 个媒体文件。"); emit messageChanged(); return; }
    m_message = tr("正在导入媒体，请稍候。"); emit messageChanged();
    m_import.setFuture(QtConcurrent::run(inspectLocalMedia, url));
}
void ProjectionCanvas::removeMedia(const QString &id) {
    if (m_media.removeIf([&](const QVariant &v) { return v.toMap().value("id").toString() == id; })) emit eventOccurred("mediaRemoved");
    emit mediaChanged();
}
void ProjectionCanvas::placeMedia(const QString &id, qreal x, qreal y, qreal w, qreal h) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h)) return;
    for (auto &value : m_media) { auto item = value.toMap(); if (item.value("id").toString() != id) continue;
        w = std::min(width(), std::max(110., w)); h = std::min(height(), std::max(100., h));
        item["x"] = std::clamp(x, 0., width() - w); item["y"] = std::clamp(y, 0., height() - h); item["width"] = w; item["height"] = h;
        value = item; emit mediaChanged(); return;
    }
}
