#include "mediaeditor.hpp"
#include "mediaengine.hpp"
#include "mediainfo.hpp"
#include <QDateTime>
#include <QFileInfo>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVideoFrame>
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>

namespace m = endfield::modules;
namespace c = endfield::core;
namespace {
c::Point point(QPointF p) { return {p.x(), p.y()}; }
QRectF rect(c::Rect r) { return {r.x, r.y, r.width, r.height}; }
QString asset(std::string_view id) { return QString::fromUtf8(id.data(), id.size()); }
}
MediaEditor::MediaEditor(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::LeftButton); setAntialiasing(true); setClip(true);
    m_player.setAudioOutput(&m_audio); m_player.setVideoSink(&m_sink);
    m_renderTimer.setInterval(40); m_renderTimer.setSingleShot(true);
    connect(&m_renderTimer, &QTimer::timeout, this, &MediaEditor::renderPreview);
    m_progressTimer.setInterval(100); connect(&m_progressTimer, &QTimer::timeout, this, &MediaEditor::changed);
    connect(&m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        if (!frame.isValid()) return;
        auto image = frame.toImage(); if (image.isNull()) return;
        m_original = image.scaled(1152, 1152, Qt::KeepAspectRatio, Qt::SmoothTransformation); requestPreview();
    });
    connect(&m_player, &QMediaPlayer::positionChanged, this, [this] {
        if (m_player.playbackState() == QMediaPlayer::PlayingState && m_player.position() / 1000. >= m_adjustments.trimEnd.value_or(m_metadata.value("duration").toDouble())) { m_player.pause(); m_player.setPosition(qRound64(m_adjustments.trimStart * 1000)); }
        emit changed();
    });
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, &MediaEditor::changed);
    connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &error) { m_error = error; emit changed(); });
    connect(&m_open, &QFutureWatcherBase::finished, this, [this] {
        auto result = m_open.result();
        if (result.contains("error")) { m_error = result.value("error").toString(); emit changed(); return; }
        m_player.stop(); m_player.setSource({}); m_metadata = result; m_adjustments = {}; m_viewport.reset(); m_selected = -1; m_cropping = false;
        const QFileInfo file(m_metadata.value("path").toString()); m_sourceBytes = file.size(); m_sourceModified = file.lastModified().toMSecsSinceEpoch();
        m_preview = {}; m_original = result.take("image").value<QImage>(); m_metadata.remove("image"); ++m_revision;
        if (m_metadata.value("kind") == "video") m_player.setSource(m_metadata.value("source").toUrl());
        else requestPreview();
        update(); emit changed();
    });
    connect(&m_render, &QFutureWatcherBase::finished, this, [this] {
        if (m_renderRevision == m_revision) {
            const auto image = m_render.result();
            if (!image.isNull()) m_preview = image; else m_error = tr("无法生成媒体预览。");
            update(); emit changed();
        }
        if (m_previewPending) { m_previewPending = false; renderPreview(); }
    });
    connect(&m_export, &QFutureWatcherBase::finished, this, [this] {
        m_progressTimer.stop(); m_error = m_export.result();
        m_message = m_error.isEmpty() ? tr("导出完成。") : QString(); emit changed();
    });
}
MediaEditor::~MediaEditor() { m_cancel = true; m_export.waitForFinished(); m_open.waitForFinished(); m_render.waitForFinished(); }
bool MediaEditor::editable() const { return !m_metadata.isEmpty() && !m_open.isRunning() && !m_export.isRunning(); }
void MediaEditor::setPresented(bool value) { if (m_presented == value) return; m_presented = value; if (!value) m_player.pause(); emit presentedChanged(); }
void MediaEditor::setCropping(bool value) { if (m_cropping == value) return; m_cropping = value; m_player.pause(); requestPreview(); emit changed(); }
QVariantMap MediaEditor::state() const {
    QVariantMap a{{"brightness", m_adjustments.brightness}, {"contrast", m_adjustments.contrast}, {"saturation", m_adjustments.saturation}, {"temperature", m_adjustments.temperature}, {"tint", m_adjustments.tint},
        {"exposure", m_adjustments.exposure}, {"highlights", m_adjustments.highlights}, {"shadows", m_adjustments.shadows}, {"black", m_adjustments.levelsBlack}, {"white", m_adjustments.levelsWhite}, {"gamma", m_adjustments.levelsGamma},
        {"trimStart", m_adjustments.trimStart}, {"trimEnd", m_adjustments.trimEnd.value_or(m_metadata.value("duration").toDouble())}, {"rotation", m_adjustments.rotationQuarterTurns}, {"mirror", m_adjustments.mirrored}, {"filter", asset(m::mediaAssemblyFilterItem(m_adjustments.filter).id)}};
    for (int i = 0; i < 5; ++i) a["curve" + QString::number(i)] = m_adjustments.curve[i];
    return {{"metadata", m_metadata}, {"loaded", !m_metadata.isEmpty()}, {"video", m_metadata.value("kind") == "video"}, {"busy", m_open.isRunning()}, {"exporting", m_export.isRunning()}, {"progress", m_progress.load()},
        {"error", m_error}, {"message", m_message}, {"overwrite", m_pendingOverwrite}, {"adjustments", a}, {"zoom", m_viewport.zoom()}, {"playing", m_player.playbackState() == QMediaPlayer::PlayingState}, {"position", m_player.position() / 1000.}};
}
QVariantList MediaEditor::filters() const {
    QVariantList result;
    for (const auto &item : m::mediaAssemblyFilters()) result.append(QVariantMap{{"id", asset(item.id)}, {"title", tr(std::string(item.chinese).c_str())}, {"source", item.id == "none" ? QString() : "qrc:/assets/media-assembly/filter-icons/" + asset(item.id) + ".png"}});
    return result;
}
QVariantList MediaEditor::stickers() const {
    QVariantList result;
    for (const auto &item : m::mediaAssemblyStickers()) result.append(QVariantMap{{"id", asset(item.id)}, {"title", tr(std::string(item.chinese).c_str())}, {"source", "qrc:/assets/media-assembly/stickers/" + asset(item.id) + ".png"}});
    return result;
}
void MediaEditor::openSource(const QUrl &url) {
    if (m_export.isRunning() || m_open.isRunning()) return;
    m_error.clear(); m_message.clear(); m_pendingOverwrite.clear();
    m_open.setFuture(QtConcurrent::run([url] {
        auto data = inspectLocalMedia(url);
        if (!data.contains("error") && data.value("kind") != "video") {
            QImageReader reader(url.toLocalFile()); reader.setAutoTransform(true);
            if (reader.size().isValid()) reader.setScaledSize(reader.size().scaled(1152, 1152, Qt::KeepAspectRatio));
            const auto image = reader.read();
            if (image.isNull()) data["error"] = tr("无法生成媒体预览。"); else data["image"] = image;
        }
        return data;
    })); emit changed();
}
void MediaEditor::closeSource() {
    if (m_export.isRunning() || m_open.isRunning()) return;
    ++m_revision; m_player.stop(); m_player.setSource({}); m_metadata.clear(); m_original = {}; m_preview = {}; m_selected = -1;
    m_adjustments = {}; m_viewport.reset(); m_pendingOverwrite.clear(); m_error.clear(); m_message.clear(); update(); emit changed();
}
void MediaEditor::editChanged() { m_error.clear(); m_message.clear(); requestPreview(); emit changed(); update(); }
void MediaEditor::setAdjustment(const QString &key, double value) {
    if (!editable() || !std::isfinite(value)) return;
    auto a = m_adjustments;
    if (key == "brightness") a.brightness = std::clamp(value, -1., 1.);
    else if (key == "contrast") a.contrast = std::clamp(value, 0., 4.);
    else if (key == "saturation") a.saturation = std::clamp(value, 0., 2.);
    else if (key == "temperature") a.temperature = std::clamp(value, 2000., 12000.);
    else if (key == "tint") a.tint = std::clamp(value, -200., 200.);
    else if (key == "exposure") a.exposure = std::clamp(value, -4., 4.);
    else if (key == "black") a.levelsBlack = std::clamp(value, 0., a.levelsWhite - .01);
    else if (key == "white") a.levelsWhite = std::clamp(value, a.levelsBlack + .01, 1.);
    else if (key == "gamma") a.levelsGamma = std::clamp(value, .1, 4.);
    else if (key == "rotation") a.rotationQuarterTurns = (int(value) % 4 + 4) % 4;
    else if (key == "mirror") a.mirrored = value != 0;
    else if (key == "trimStart") a.trimStart = std::max(0., std::min(value, a.trimEnd.value_or(m_metadata.value("duration").toDouble()) - .0501));
    else if (key == "trimEnd") a.trimEnd = std::min(m_metadata.value("duration").toDouble(), std::max(value, a.trimStart + .0501));
    else if (key.startsWith("curve") && key.size() == 6 && key.back() >= '0' && key.back() <= '4') a.curve[key.back().digitValue()] = std::clamp(value, 0., 1.);
    else return;
    if (!a.valid()) return;
    m_adjustments = a; editChanged();
}
void MediaEditor::setFilter(const QString &id) { if (!editable()) return; const auto filter = m::mediaAssemblyFilter(id.toStdString()); if (!filter) return; m_adjustments.filter = *filter; editChanged(); }
void MediaEditor::addSticker(const QString &id) {
    if (!editable() || m_metadata.value("kind") == "video" || m_adjustments.stickers.size() >= 16) return;
    const auto kind = m::mediaAssemblySticker(id.toStdString()); if (!kind) return;
    m_adjustments.stickers.push_back({QUuid::createUuid().toString().toStdString(), *kind}); m_selected = int(m_adjustments.stickers.size() - 1); update(); emit changed();
}
void MediaEditor::reset(const QString &tool) {
    if (!editable()) return;
    const m::MediaAssemblyAdjustments defaults;
    if (tool.isEmpty()) { m_adjustments = defaults; m_selected = -1; m_viewport.reset(); }
    else if (tool == "crop") m_adjustments.crop = {};
    else if (tool == "curves") m_adjustments.curve = defaults.curve;
    else if (tool == "levels") { m_adjustments.levelsBlack = 0; m_adjustments.levelsWhite = 1; m_adjustments.levelsGamma = 1; }
    else if (tool == "trim") { m_adjustments.trimStart = 0; m_adjustments.trimEnd.reset(); }
    else if (tool == "adjust") { m_adjustments.brightness = 0; m_adjustments.contrast = 1; m_adjustments.saturation = 1; m_adjustments.temperature = 6500; m_adjustments.tint = 0; m_adjustments.exposure = 0; }
    editChanged();
}
void MediaEditor::requestPreview() { m_previewPending = true; if (!m_renderTimer.isActive()) m_renderTimer.start(); }
void MediaEditor::renderPreview() {
    if (m_render.isRunning() || m_original.isNull()) return;
    m_previewPending = false; auto a = m_adjustments; if (m_cropping) a.crop = {};
    m_renderRevision = m_revision;
    m_render.setFuture(QtConcurrent::run(renderMediaFrame, m_original, a, false, m_metadata.value("kind") == "video"));
}
c::Rect MediaEditor::imageRect() const {
    return m_preview.isNull() ? c::Rect{} : m_viewport.imageRect({double(m_preview.width()), double(m_preview.height())}, {0, 0, width(), height()});
}
void MediaEditor::zoomBy(double factor) { if (!std::isfinite(factor) || factor <= 0) return; m_viewport.zoomBy(factor, {double(m_preview.width()), double(m_preview.height())}, {0, 0, width(), height()}); update(); emit changed(); }
void MediaEditor::resetZoom() { m_viewport.reset(); update(); emit changed(); }
void MediaEditor::togglePlayback() { if (!m_presented || !editable() || m_cropping) return; if (m_player.playbackState() == QMediaPlayer::PlayingState) m_player.pause(); else { if (m_player.position() / 1000. < m_adjustments.trimStart) m_player.setPosition(qRound64(m_adjustments.trimStart * 1000)); m_player.play(); } }
void MediaEditor::seek(double seconds) { if (editable() && std::isfinite(seconds)) m_player.setPosition(qRound64(std::clamp(seconds, 0., m_metadata.value("duration").toDouble()) * 1000)); }
void MediaEditor::exportTo(const QUrl &url, bool overwrite) {
    if (!editable() || !url.isLocalFile()) return;
    const QFileInfo source(m_metadata.value("path").toString());
    if (source.size() != m_sourceBytes || source.lastModified().toMSecsSinceEpoch() != m_sourceModified) { m_error = tr("原文件已更改，请重新打开后再导出。"); emit changed(); return; }
    if (QFileInfo::exists(url.toLocalFile()) && !overwrite) { m_pendingOverwrite = url.toString(); emit changed(); return; }
    m_player.pause(); m_error.clear(); m_message.clear(); m_pendingOverwrite.clear(); m_cancel = false; m_progress = 0;
    const auto metadata = m_metadata; const auto a = m_adjustments;
    m_export.setFuture(QtConcurrent::run([this, metadata, a, url, overwrite] { return exportMedia(metadata, a, url.toLocalFile(), overwrite, m_cancel, m_progress); }));
    m_progressTimer.start(); emit changed();
}
void MediaEditor::cancelExport() { m_cancel = true; m_pendingOverwrite.clear(); emit changed(); }
void MediaEditor::paint(QPainter *p) {
    p->setRenderHint(QPainter::Antialiasing); p->setRenderHint(QPainter::SmoothPixmapTransform);
    p->fillRect(boundingRect(), QColor("#0C0C0C"));
    p->setPen(Qt::NoPen); p->setBrush(QColor("#2A2A2A"));
    for (int x = 8; x < width(); x += 16) for (int y = 8; y < height(); y += 16) p->drawEllipse(QPointF(x, y), 1.2, 1.2);
    const auto image = imageRect(); if (image.width <= 0 || image.height <= 0) return;
    p->drawImage(rect(image), m_preview);
    for (size_t i = 0; i < m_adjustments.stickers.size(); ++i) {
        const auto &s = m_adjustments.stickers[i];
        auto base = image;
        if (m_cropping) {
            const auto crop = m::MediaAssemblyViewport::displayCrop(m_adjustments.crop, m_adjustments.rotationQuarterTurns, m_adjustments.mirrored);
            base = {image.x + crop.x * image.width, image.y + crop.y * image.height, crop.width * image.width, crop.height * image.height};
        }
        const auto box = m::MediaAssemblyViewport::stickerRect(s, base);
        const QImage artwork(":/assets/media-assembly/stickers/" + asset(m::mediaAssemblyStickerItem(s.kind).id) + ".png");
        p->save(); p->translate(box.x + box.width / 2, box.y + box.height / 2); p->rotate(s.rotation); p->translate(-box.width / 2, -box.height / 2); p->drawImage(QRectF(0, 0, box.width, box.height), artwork);
        if (int(i) == m_selected && !m_cropping) {
            p->setPen(QPen(QColor("#FFFA00"), 1, Qt::DashLine)); p->setBrush(Qt::NoBrush); p->drawRect(QRectF(0, 0, box.width, box.height));
            for (const auto &[handle, center] : m::MediaAssemblyViewport::handleCentres({box.width, box.height})) {
                p->setPen(QPen(QColor("#555555"), 1));
                if (handle.kind == m::MediaAssemblyHandle::Kind::corner) { p->setBrush(Qt::white); p->drawRect(QRectF(center.x - 3.5, center.y - 3.5, 7, 7)); }
                else { p->setBrush(QColor(0, 0, 0, 217)); p->drawEllipse(QPointF(center.x, center.y), 9, 9); p->setPen(Qt::white); p->drawText(QRectF(center.x - 9, center.y - 9, 18, 18), Qt::AlignCenter, handle.kind == m::MediaAssemblyHandle::Kind::rotate ? "↻" : "×"); }
            }
        }
        p->restore();
    }
    if (m_cropping) {
        const auto crop = m::MediaAssemblyViewport::displayCrop(m_adjustments.crop, m_adjustments.rotationQuarterTurns, m_adjustments.mirrored);
        const QRectF box(image.x + crop.x * image.width, image.y + crop.y * image.height, crop.width * image.width, crop.height * image.height);
        QPainterPath mask; mask.addRect(boundingRect()); mask.addRect(box); p->fillPath(mask, QColor(0, 0, 0, 150));
        p->setPen(QPen(Qt::white, 1)); p->setBrush(Qt::NoBrush); p->drawRect(box);
        const auto corners = m::MediaAssemblyViewport::cropHandleOrigins(crop, image, {width(), height()});
        p->setPen(QPen(QColor("#FFFA00"), 3));
        for (int i = 0; i < 4; ++i) { const auto o = corners[i]; const double x = o.x + (i % 2 ? 14 : 0), y = o.y + (i >= 2 ? 14 : 0); p->drawLine(QPointF(x, o.y), QPointF(x, o.y + 14)); p->drawLine(QPointF(o.x, y), QPointF(o.x + 14, y)); }
    }
}
void MediaEditor::mousePressEvent(QMouseEvent *e) {
    if (!editable()) return;
    m_dragStart = m_previous = e->position(); m_cropCorner = -1; m_handle.reset(); const auto image = imageRect();
    if (image.width <= 0) return;
    if (m_cropping) {
        m_dragCrop = m::MediaAssemblyViewport::displayCrop(m_adjustments.crop, m_adjustments.rotationQuarterTurns, m_adjustments.mirrored);
        const auto corners = m::MediaAssemblyViewport::cropHandleOrigins(m_dragCrop, image, {width(), height()});
        for (int i = 0; i < 4; ++i) if (QRectF(corners[i].x - 6, corners[i].y - 6, 26, 26).contains(e->position())) { m_cropCorner = i; break; }
        return;
    }
    if (m_selected >= 0 && m_selected < int(m_adjustments.stickers.size())) {
        const auto &s = m_adjustments.stickers[m_selected]; m_handle = m::MediaAssemblyViewport::handle(point(e->position()), m::MediaAssemblyViewport::stickerRect(s, image), s.rotation);
        if (m_handle) {
            if (m_handle->kind == m::MediaAssemblyHandle::Kind::remove) { m_adjustments.stickers.erase(m_adjustments.stickers.begin() + m_selected); m_selected = -1; m_handle.reset(); update(); return; }
            m_dragSticker = s; return;
        }
    }
    m_selected = -1;
    for (int i = int(m_adjustments.stickers.size()) - 1; i >= 0; --i) { const auto &s = m_adjustments.stickers[i]; const auto box = m::MediaAssemblyViewport::stickerRect(s, image); const auto center = c::Point{box.x + box.width / 2, box.y + box.height / 2}; if (box.contains(m::MediaAssemblyViewport::unrotate(point(e->position()), center, s.rotation))) { m_selected = i; m_dragSticker = s; break; } }
    update();
}
void MediaEditor::mouseMoveEvent(QMouseEvent *e) {
    if (!editable() || !(e->buttons() & Qt::LeftButton)) return;
    const auto image = imageRect(); if (image.width <= 0 || image.height <= 0) return;
    if (m_cropping && m_cropCorner >= 0) {
        const auto crop = m::MediaAssemblyViewport::dragCrop(m_dragCrop, m_cropCorner, {(e->position().x() - image.x) / image.width, (e->position().y() - image.y) / image.height});
        m_adjustments.crop = m::MediaAssemblyViewport::sourceCrop(crop, m_adjustments.rotationQuarterTurns, m_adjustments.mirrored); update(); emit changed(); return;
    }
    if (!m_cropping && m_selected >= 0) {
        auto &s = m_adjustments.stickers[m_selected]; const auto box = m::MediaAssemblyViewport::stickerRect(m_dragSticker, image); const QPointF center(box.x + box.width / 2, box.y + box.height / 2);
        if (m_handle && m_handle->kind == m::MediaAssemblyHandle::Kind::rotate) {
            const auto old = m_dragStart - center, now = e->position() - center;
            s.rotation = std::remainder(m_dragSticker.rotation + (std::atan2(now.y(), now.x()) - std::atan2(old.y(), old.x())) * 180 / M_PI, 360.);
        } else if (m_handle && m_handle->kind == m::MediaAssemblyHandle::Kind::corner) {
            s.size = std::clamp(m_dragSticker.size * QLineF(center, e->position()).length() / std::max(1., QLineF(center, m_dragStart).length()), .02, 1.);
        } else { s.x = std::clamp(m_dragSticker.x + (e->position().x() - m_dragStart.x()) / image.width, 0., 1.); s.y = std::clamp(m_dragSticker.y + (e->position().y() - m_dragStart.y()) / image.height, 0., 1.); }
    } else m_viewport.move(point(e->position() - m_previous), {double(m_preview.width()), double(m_preview.height())}, {0, 0, width(), height()});
    m_previous = e->position(); update();
}
void MediaEditor::mouseReleaseEvent(QMouseEvent *) { m_cropCorner = -1; m_handle.reset(); emit changed(); }
void MediaEditor::wheelEvent(QWheelEvent *e) {
    if (!editable() || m_preview.isNull()) return;
    m_viewport.magnify(std::pow(1.12, e->angleDelta().y() / 120.), point(e->position()), {double(m_preview.width()), double(m_preview.height())}, {0, 0, width(), height()}); update(); emit changed(); e->accept();
}
