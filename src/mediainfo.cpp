#include "mediainfo.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QProcess>

QVariantMap inspectLocalMedia(const QUrl &url) {
    const QFileInfo file(url.toLocalFile());
    const auto error = [] { return QVariantMap{{"error", QCoreApplication::translate("HyprEndfield", "只支持本机图片和视频。")}}; };
    if (!url.isLocalFile() || !file.isFile() || !file.isReadable()) return error();
    QVariantMap result{{"source", QUrl::fromLocalFile(file.absoluteFilePath())}, {"path", file.absoluteFilePath()}, {"name", file.fileName()}};
    QImageReader image(file.absoluteFilePath()); image.setAutoTransform(true);
    if (image.canRead()) {
        auto size = image.size();
        if (!size.isValid() || qint64(size.width()) * size.height() > 100'000'000) return error();
        if (image.transformation() & QImageIOHandler::TransformationRotate90) size.transpose();
        result["width"] = size.width(); result["height"] = size.height();
        result["kind"] = image.supportsAnimation() ? "gif" : "image"; return result;
    }
    if (!QMimeDatabase().mimeTypeForFile(file).name().startsWith("video/")) return error();
    QProcess probe;
    probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0", "-show_entries", "stream=width,height,avg_frame_rate,duration,color_transfer,color_primaries,color_space:stream_side_data=rotation:format=duration", "-of", "json", file.absoluteFilePath()});
    if (!probe.waitForStarted(2000) || !probe.waitForFinished(6000)) { probe.kill(); probe.waitForFinished(); return error(); }
    const auto object = QJsonDocument::fromJson(probe.readAllStandardOutput()).object();
    const auto streams = object["streams"].toArray();
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() || streams.isEmpty()) return error();
    const auto stream = streams.first().toObject(); int w = stream["width"].toInt(), h = stream["height"].toInt();
    if (w <= 0 || h <= 0 || qint64(w) * h > 100'000'000) return error();
    for (const auto &side : stream["side_data_list"].toArray()) if (qAbs(side.toObject()["rotation"].toInt()) % 180 == 90) { std::swap(w, h); break; }
    const auto rate = stream["avg_frame_rate"].toString().split('/');
    const double fps = rate.size() == 2 && rate[1].toDouble() > 0 ? rate[0].toDouble() / rate[1].toDouble() : 30.;
    const double duration = object["format"].toObject()["duration"].toString().toDouble();
    result["width"] = w; result["height"] = h; result["kind"] = "video"; result["duration"] = duration;
    result["fps"] = fps; result["transfer"] = stream["color_transfer"].toString(); result["primaries"] = stream["color_primaries"].toString();
    return result;
}
