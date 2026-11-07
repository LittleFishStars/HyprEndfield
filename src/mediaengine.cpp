#include "mediaengine.hpp"
#include <QColorSpace>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryFile>
#include <cmath>

namespace m = endfield::modules;
namespace {
std::span<const std::uint8_t> bytes(const QByteArray &v) { return {reinterpret_cast<const std::uint8_t *>(v.constData()), size_t(v.size())}; }
std::span<const std::uint8_t> pixels(const QImage &v) { return {v.constBits(), size_t(v.sizeInBytes())}; }
QString tr(const char *s) { return QCoreApplication::translate("HyprEndfield", s); }
QByteArray lookup(const m::MediaAssemblyAdjustments &a) {
    if (a.filter == m::MediaAssemblyFilter::none) return {};
    QFile cube(":/assets/media-assembly/luts/" + QString::fromStdString(std::string(m::mediaAssemblyFilterItem(a.filter).id)) + ".rgb8");
    if (!cube.open(QIODevice::ReadOnly)) throw std::runtime_error("Missing original filter cube");
    return cube.readAll();
}
class FrameRenderer {
    QByteArray lut;
    QList<QImage> images;
    std::vector<m::MediaAssemblyStickerArtwork> artwork;
    std::unique_ptr<m::MediaAssemblyFrameProcessor> processor;
public:
    FrameRenderer(const QSize &size, const m::MediaAssemblyAdjustments &a, bool stickers, bool video) : lut(lookup(a)) {
        if (stickers) for (const auto &sticker : a.stickers) {
            const auto id = QString::fromStdString(std::string(m::mediaAssemblyStickerItem(sticker.kind).id));
            QImage image(":/assets/media-assembly/stickers/" + id + ".png");
            if (image.isNull()) throw std::runtime_error("Missing original sticker");
            // Keep the authored canvas, including transparent borders.
            images.append(image.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
        }
        for (const auto &image : images) artwork.push_back({unsigned(image.width()), unsigned(image.height()), pixels(image)});
        processor = std::make_unique<m::MediaAssemblyFrameProcessor>(size.width(), size.height(), a, bytes(lut), stickers, artwork, video);
    }
    QImage render(QImage image, bool video) const {
        image = image.convertToFormat(QImage::Format_RGBA8888);
        std::shared_ptr<const m::MediaAssemblySourceColour> colour;
        if (video) colour = m::mediaAssemblyVideoColour();
        else if (image.colorSpace().isValid()) {
            const auto profile = image.colorSpace().iccProfile(); const auto original = m::mediaAssemblyProfile(bytes(profile));
            if (original.kind == m::MediaAssemblyProfileKind::converted) colour = original.colour;
            else if (original.kind == m::MediaAssemblyProfileKind::unsupported) image.convertToColorSpace(QColorSpace::SRgb);
        }
        m::MediaAssemblyPixels source{unsigned(image.width()), unsigned(image.height()), size_t(image.bytesPerLine()), pixels(image), false, colour.get()};
        const auto data = processor->render(source, m::MediaAssemblyOutputAlpha::straight);
        QImage result(data.data(), processor->width(), processor->height(), processor->width() * 4, QImage::Format_RGBA8888);
        auto owned = result.copy(); owned.setColorSpace(QColorSpace::SRgb); return owned;
    }
};
bool transferFile(const QString &source, const QString &destination, std::atomic_bool &cancel) {
    QFile input(source); QSaveFile output(destination);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) return false;
    while (!input.atEnd()) {
        if (cancel) return false;
        const auto chunk = input.read(1024 * 1024);
        if (input.error() != QFile::NoError || output.write(chunk) != chunk.size()) return false;
    }
    return !cancel && output.commit();
}
}
QImage renderMediaFrame(const QImage &image, const m::MediaAssemblyAdjustments &a, bool stickers, bool video) {
    if (image.isNull()) return {};
    try { return FrameRenderer(image.size(), a, stickers, video).render(image, video); } catch (...) { return {}; }
}
QString exportMedia(const QVariantMap &meta, const m::MediaAssemblyAdjustments &a, const QString &destination,
                    bool overwrite, std::atomic_bool &cancel, std::atomic<double> &progress) {
    const auto failed = [] { return tr("导出失败，原文件已保留。"); };
    if (!overwrite && QFileInfo::exists(destination)) return tr("目标文件已存在，请确认覆盖或选择其他名称。");
    const auto suffix = QFileInfo(destination).suffix().toLower();
    const auto path = meta.value("path").toString();
    try {
        if (meta.value("kind").toString() != "video") {
            const QByteArray format = suffix == "jpg" ? QByteArray("jpeg") : suffix.toLatin1();
            if (!QList<QByteArray>{"png", "jpeg", "tiff", "tif", "webp"}.contains(format)) return tr("请选择 PNG、JPEG、TIFF 或 WebP 格式。");
            QImageReader reader(path); reader.setAutoTransform(true); const auto original = reader.read();
            if (original.isNull()) return failed();
            progress = .15; const auto image = renderMediaFrame(original, a, true); progress = .9;
            if (image.isNull()) return failed();
            if (cancel) return tr("导出已取消。");
            QSaveFile output(destination); if (!output.open(QIODevice::WriteOnly)) return failed();
            QImageWriter writer(&output, format == "tif" ? "tiff" : format); writer.setQuality(95);
            if (!writer.write(image) || cancel || !output.commit()) return cancel ? tr("导出已取消。") : failed();
            progress = 1; return {};
        }
        if (suffix != "mp4") return tr("视频导出请选择 MP4 格式。");
        const QString transfer = meta.value("transfer").toString();
        const QString primaries = meta.value("primaries").toString();
        if (transfer.contains("2084") || transfer.contains("b67") || primaries.contains("2020")) return tr("暂不支持 HDR 视频导出。");
        const auto range = a.timeRange(meta.value("duration").toDouble());
        if (!range) return tr("请选择有效的裁剪、调整或剪辑范围。");
        const double start = double(range->startTicks) / 600., duration = double(range->durationTicks) / 600.;
        const double fps = meta.value("fps", 30.).toDouble();
        if (!(fps > 0 && fps <= 240)) return failed();
        const QSize sourceSize(meta.value("width").toInt(), meta.value("height").toInt());
        FrameRenderer renderer(sourceSize, a, false, true);
        QTemporaryFile temporary(QFileInfo(destination).absolutePath() + "/.hyprendfield-export-XXXXXX.mp4");
        if (!temporary.open()) return failed();
        const auto tempPath = temporary.fileName(); temporary.close();
        QProcess decoder, encoder;
        const QString begin = QString::number(start, 'f', 6), length = QString::number(duration, 'f', 6), rate = QString::number(fps, 'g', 12);
        decoder.start("ffmpeg", {"-v", "error", "-nostdin", "-ss", begin, "-i", path, "-t", length, "-map", "0:v:0", "-an", "-vf", "fps=" + rate, "-f", "rawvideo", "-pix_fmt", "rgba", "pipe:1"});
        if (!decoder.waitForStarted(3000)) return failed();
        const qint64 frameBytes = qint64(sourceSize.width()) * sourceSize.height() * 4;
        QByteArray buffer; qint64 frames = 0; QElapsedTimer idle; idle.start();
        bool encoding = false; QString failure;
        while (!cancel) {
            if (buffer.size() < frameBytes) {
                if (decoder.bytesAvailable() == 0) decoder.waitForReadyRead(250);
                const auto chunk = decoder.read(frameBytes - buffer.size());
                if (!chunk.isEmpty()) { buffer.append(chunk); idle.restart(); }
                if (buffer.size() < frameBytes) {
                    if (decoder.state() == QProcess::NotRunning) break;
                    if (idle.elapsed() > 20'000) { failure = failed(); break; }
                    continue;
                }
            }
            QImage frame(reinterpret_cast<const uchar *>(buffer.constData()), sourceSize.width(), sourceSize.height(), sourceSize.width() * 4, QImage::Format_RGBA8888);
            auto image = renderer.render(frame, true); buffer.clear();
            // Match the source's final even-dimension resample, not a new crop.
            const auto plan = m::mediaAssemblyPixelPlan(sourceSize.width(), sourceSize.height(), a, {}, true);
            if (image.size() != QSize(plan.outputWidth, plan.outputHeight)) {
                QImage even(plan.outputWidth, plan.outputHeight, QImage::Format_RGBA8888_Premultiplied);
                const auto associated = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
                m::MediaAssemblyPixels pixelsIn{unsigned(associated.width()), unsigned(associated.height()), size_t(associated.bytesPerLine()), pixels(associated), true, nullptr};
                m::mediaAssemblyResample(pixelsIn, even.width(), even.height(), {even.bits(), size_t(even.sizeInBytes())}, even.bytesPerLine());
                image = even.convertToFormat(QImage::Format_RGBA8888);
            }
            if (!encoding) {
                encoder.start("ffmpeg", {"-v", "error", "-nostdin", "-y", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", QString::number(image.width()) + 'x' + QString::number(image.height()), "-r", rate,
                    "-i", "pipe:0", "-ss", begin, "-i", path, "-t", length, "-map", "0:v:0", "-map", "1:a:0?", "-c:v", "libx264", "-preset", "veryfast", "-crf", "18", "-pix_fmt", "yuv420p", "-c:a", "aac", "-movflags", "+faststart", tempPath});
                if (!encoder.waitForStarted(3000)) { failure = failed(); break; } encoding = true;
            }
            if (encoder.write(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()) < 0) { failure = failed(); break; }
            idle.restart();
            while (!cancel && encoder.bytesToWrite() > 1024 * 1024) {
                encoder.waitForBytesWritten(250);
                if (encoder.state() == QProcess::NotRunning || idle.elapsed() > 30'000) { failure = failed(); break; }
            }
            if (!failure.isEmpty()) break;
            ++frames; progress = std::min(.95, frames / (duration * fps) * .95);
        }
        if (cancel || !failure.isEmpty()) { decoder.kill(); encoder.kill(); decoder.waitForFinished(); encoder.waitForFinished(); return cancel ? tr("导出已取消。") : failure; }
        decoder.waitForFinished(3000); encoder.closeWriteChannel();
        idle.restart();
        while (encoder.state() != QProcess::NotRunning && !cancel && idle.elapsed() < 60'000) encoder.waitForFinished(250);
        if (cancel || encoder.state() != QProcess::NotRunning) { encoder.kill(); encoder.waitForFinished(); return cancel ? tr("导出已取消。") : failed(); }
        if (!frames || decoder.exitCode() || encoder.exitCode() || !buffer.isEmpty()) return failed();
        if (!overwrite && QFileInfo::exists(destination)) return tr("目标文件已存在，请确认覆盖或选择其他名称。");
        if (!transferFile(tempPath, destination, cancel)) return cancel ? tr("导出已取消。") : failed();
        progress = 1; return {};
    } catch (...) { return failed(); }
}
