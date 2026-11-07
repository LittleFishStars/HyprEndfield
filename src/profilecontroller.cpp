#include "profilecontroller.hpp"
#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QtConcurrentRun>
#include <QtMath>
#include <cmath>

namespace {
QImage readImage(const QString &path) { QImageReader reader(path); reader.setAutoTransform(true); return reader.read(); }
bool ownedImage(const QString &name) { return QRegularExpression("^[0-9a-f-]{36}\\.[a-z0-9]{1,8}$").match(name).hasMatch(); }
QRectF crop(const QImage &image, QSizeF target, double zoom, double x, double y) {
    const double scale = std::max(target.width() / image.width(), target.height() / image.height()) * zoom;
    const QSizeF area(target.width() / scale, target.height() / scale);
    return {(image.width() - area.width()) * (x + 1) / 2, (image.height() - area.height()) * (y + 1) / 2, area.width(), area.height()};
}
}
ProfileController::ProfileController(QObject *parent, QString directory) : QObject(parent) {
    m_directory = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/HyprEndfield/Profile" : directory;
    m_path = m_directory + "/profile.json";
    const auto today = QDate::currentDate();
    m_values = {{"name", qEnvironmentVariable("USER", "Endministrator")}, {"tag", "0000"}, {"introduction", ""},
        {"playerID", QString::number(QRandomGenerator::global()->bounded(quint64(1'000'000'000), quint64(10'000'000'000)))},
        {"awakeningDate", today.toString(Qt::ISODate)}, {"birthday", today.toString("MM-dd")}, {"showsBirthday", false},
        {"permissionLevel", 1}, {"explorationLevel", 1}, {"operatorsCount", 0}, {"weaponsCount", 0}, {"archivesCount", 0},
        {"themeColor", ""}, {"avatar", ""}, {"background", ""}, {"avatarZoom", 1.}, {"avatarOffsetX", 0.}, {"avatarOffsetY", 0.},
        {"backgroundWidth", 600.}, {"backgroundZoom", 1.}, {"backgroundOffsetX", 0.}, {"backgroundOffsetY", 0.},
        {"thumbnailZoom", 1.}, {"thumbnailOffsetX", 0.}, {"thumbnailOffsetY", 0.}};
    QFile file(m_path);
    if (file.exists()) {
        m_readable = file.open(QIODevice::ReadOnly);
        if (m_readable) {
            m_persisted = file.read(1024 * 1024 + 1);
            const auto document = QJsonDocument::fromJson(m_persisted);
            m_readable = document.isObject() && document.object().value("version").toInt() == 1 && document.object().value("profile").isObject() && m_persisted.size() <= 1024 * 1024;
            if (m_readable) {
                const auto saved = document.object().value("profile").toObject().toVariantMap();
                for (auto i = saved.cbegin(); i != saved.cend(); ++i) {
                    if (i.key() == "avatar" || i.key() == "background") { if (ownedImage(i.value().toString())) m_values[i.key()] = i.value(); }
                    else setField(i.key(), i.value(), false);
                }
                for (const auto &kind : {QString("avatar"), QString("background")}) if (!m_values.value(kind).toString().isEmpty()) {
                    const auto image = readImage(m_directory + '/' + m_values.value(kind).toString());
                    if (kind == "avatar") m_avatar = image; else m_background = image;
                }
            }
        }
        if (!m_readable) m_error = tr("无法读取名片资料，原文件已保留。");
    }
    m_saved = m_values;
    if (!file.exists()) save();
    connect(&m_import, &QFutureWatcherBase::finished, this, [this] {
        const auto result = m_import.result(); m_error = result.value("error").toString();
        if (m_error.isEmpty()) {
            const auto previous = m_values.value(m_importKind).toString();
            m_values[m_importKind] = result.value("name");
            if (save()) {
                if (m_importKind == "avatar") m_avatar = result.value("image").value<QImage>(); else m_background = result.value("image").value<QImage>();
                if (ownedImage(previous)) QFile::remove(m_directory + '/' + previous);
            } else QFile::remove(m_pendingImage);
        }
        m_pendingImage.clear(); emit changed();
    });
}
ProfileController::~ProfileController() { m_import.waitForFinished(); if (!m_pendingImage.isEmpty()) QFile::remove(m_pendingImage); }
QVariantMap ProfileController::state() const { auto result = m_values; result["error"] = m_error; result["busy"] = m_import.isRunning(); return result; }
bool ProfileController::save() {
    QFile current(m_path);
    if (!m_readable || (current.exists() && (!current.open(QIODevice::ReadOnly) || current.read(1024 * 1024 + 1) != m_persisted)) || (!current.exists() && !m_persisted.isEmpty())) {
        m_error = tr("名片资料已在外部更改，请重新启动后再编辑。"); m_values = m_saved; emit changed(); return false;
    }
    QSaveFile output(m_path);
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"profile", QJsonObject::fromVariantMap(m_values)}}).toJson();
    if (!QDir().mkpath(m_directory) || !output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        m_error = tr("无法保存名片资料。"); m_values = m_saved; emit changed(); return false;
    }
    m_persisted = bytes; m_saved = m_values; m_error.clear(); return true;
}
bool ProfileController::setField(const QString &field, const QVariant &value, bool commit) {
    if (!m_values.contains(field) || field == "avatar" || field == "background") return false;
    QVariant accepted = value;
    bool valid = true;
    const QMap<QString, QPair<double, double>> ranges{{"permissionLevel", {1,60}}, {"explorationLevel", {1,7}}, {"operatorsCount", {0,99999}}, {"weaponsCount", {0,99999}}, {"archivesCount", {0,99999}},
        {"avatarZoom", {1,20}}, {"avatarOffsetX", {-1,1}}, {"avatarOffsetY", {-1,1}}, {"backgroundWidth", {400,900}}, {"backgroundZoom", {1,20}}, {"backgroundOffsetX", {-400,400}}, {"backgroundOffsetY", {-250,250}},
        {"thumbnailZoom", {1,20}}, {"thumbnailOffsetX", {-1,1}}, {"thumbnailOffsetY", {-1,1}}};
    if (ranges.contains(field)) {
        const double number = value.toDouble(&valid); const auto range = ranges.value(field);
        valid = valid && std::isfinite(number) && number >= range.first && number <= range.second;
        if (field.endsWith("Level") || field.endsWith("Count")) valid = valid && std::floor(number) == number;
        accepted = number;
    } else if (field == "showsBirthday") accepted = value.toBool();
    else {
        QString text = value.toString(); if (field != "introduction") text = text.trimmed();
        if (field == "name") valid = !text.isEmpty() && text.size() <= 20 && !text.contains(QRegularExpression("[\\r\\n]"));
        if (field == "tag") valid = !text.isEmpty() && text.size() <= 10 && !text.contains(QRegularExpression("[\\s#]"));
        if (field == "introduction") valid = text.size() <= 150;
        if (field == "playerID") valid = !text.isEmpty() && text.size() <= 64 && !text.contains(QRegularExpression("[\\r\\n]"));
        if (field == "awakeningDate") valid = QDate::fromString(text, Qt::ISODate).isValid() && text.size() == 10;
        if (field == "birthday") valid = QDate::fromString("2000-" + text, Qt::ISODate).isValid() && text.size() == 5;
        if (field == "themeColor") { valid = text.isEmpty() || QRegularExpression("^#[0-9a-fA-F]{6}$").match(text).hasMatch(); text = text.toUpper(); }
        accepted = text;
    }
    if (!valid) { m_error = tr("输入内容超出允许范围，或格式不正确。"); emit changed(); return false; }
    m_values[field] = accepted; m_error.clear(); const bool saved = !commit || save(); emit changed(); return saved;
}
void ProfileController::importImage(const QString &kind, const QUrl &url) {
    if ((kind != "avatar" && kind != "background") || !url.isLocalFile() || m_import.isRunning() || !m_readable) return;
    m_importKind = kind; m_error.clear(); const auto directory = m_directory;
    m_import.setFuture(QtConcurrent::run([this, directory, url] {
        QVariantMap result; const auto path = url.toLocalFile(); QFileInfo info(path);
        QImageReader reader(path); reader.setAutoTransform(true); const auto size = reader.size();
        if (!info.isFile() || info.size() > 128 * 1024 * 1024 || !size.isValid() || qint64(size.width()) * size.height() > 100'000'000) { result["error"] = tr("请选择小于 128 MB、像素不超过一亿的图片。"); return result; }
        const auto image = reader.read(); if (image.isNull()) { result["error"] = tr("请选择可读取的图片文件。"); return result; }
        const auto suffix = info.suffix().toLower();
        const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces) + '.' + (suffix.size() <= 8 && QRegularExpression("^[a-z0-9]+$").match(suffix).hasMatch() ? suffix : QString("img"));
        if (!QDir().mkpath(directory) || !QFile::copy(path, directory + '/' + name)) { result["error"] = tr("无法保存图片。"); return result; }
        m_pendingImage = directory + '/' + name;
        result["name"] = name; result["image"] = image; return result;
    })); emit changed();
}
bool ProfileController::resetImage(const QString &kind) {
    if ((kind != "avatar" && kind != "background") || m_import.isRunning()) return false;
    const auto previous = m_values.value(kind).toString(); m_values[kind] = "";
    if (!save()) return false;
    if (ownedImage(previous)) QFile::remove(m_directory + '/' + previous);
    if (kind == "avatar") m_avatar = {}; else m_background = {};
    emit changed(); return true;
}
ProfileArtwork::ProfileArtwork(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &ProfileArtwork::appearanceChanged, this, [this] { update(); });
    const auto resizeTexture = [this] { setTextureSize(QSize(qCeil(width() * 2), qCeil(height() * 2))); update(); };
    connect(this, &QQuickItem::widthChanged, this, resizeTexture); connect(this, &QQuickItem::heightChanged, this, resizeTexture);
}
void ProfileArtwork::setController(ProfileController *value) {
    if (m_controller == value) return;
    if (m_controller) disconnect(m_controller, nullptr, this, nullptr);
    m_controller = value;
    if (value) connect(value, &ProfileController::changed, this, [this] { update(); });
    emit appearanceChanged();
}
void ProfileArtwork::paint(QPainter *p) {
    if (!m_controller || width() <= 0 || height() <= 0) return;
    const auto state = m_controller->state(); p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    if (m_kind == "frame") {
        static const QImage original = [] { QFile f(":/assets/icons/profile-frame.bgra"); if (!f.open(QIODevice::ReadOnly)) return QImage(); const auto bytes = f.read(256 * 256 * 4); if (bytes.size() != 256 * 256 * 4) return QImage(); return QImage(reinterpret_cast<const uchar *>(bytes.constData()), 256, 256, QImage::Format_ARGB32).copy(1, 1, 254, 254).flipped(Qt::Vertical); }();
        QImage frame = original;
        for (int y = 0; y < frame.height(); ++y) for (int x = 0; x < frame.width(); ++x) { auto c = frame.pixelColor(x, y); c.setRedF(c.redF() * m_accent.redF()); c.setGreenF(c.greenF() * m_accent.greenF()); c.setBlueF(c.blueF() * m_accent.blueF()); frame.setPixelColor(x, y, c); }
        p->drawImage(boundingRect(), frame); return;
    }
    const bool avatar = m_kind == "avatar";
    const auto &image = m_controller->image(avatar ? "avatar" : "background");
    const QString prefix = avatar ? "avatar" : m_kind == "thumbnail" ? "thumbnail" : "background";
    const double zoom = state.value(prefix + "Zoom").toDouble();
    if (image.isNull()) {
        if (avatar) {
            p->save(); p->translate(width() / 2 + state.value("avatarOffsetX").toDouble() * width() * .5, height() / 2 + state.value("avatarOffsetY").toDouble() * height() * .5);
            p->scale(width() / 55 * zoom, height() / 55 * zoom); p->translate(-27.5, -27.5);
            QPainterPath silhouette; silhouette.addEllipse(QRectF(19.8, 11, 16.5, 17)); silhouette.moveTo(9.9,47.3); silhouette.cubicTo(13.2,24.75,42.35,24.75,45.65,47.3); silhouette.closeSubpath(); p->fillPath(silhouette, QColor(245,245,245,161)); p->restore();
        }
        return;
    }
    if (m_kind != "background") { p->drawImage(boundingRect(), image, crop(image, QSizeF(width(),height()), zoom, state.value(prefix + "OffsetX").toDouble(), state.value(prefix + "OffsetY").toDouble())); return; }
    QImage buffer(textureSize(), QImage::Format_ARGB32_Premultiplied); buffer.fill(Qt::transparent);
    QPainter art(&buffer); art.setRenderHint(QPainter::SmoothPixmapTransform); art.scale(buffer.width() / 950., buffer.height() / 860.);
    const double w = state.value("backgroundWidth").toDouble() * 1.58, h = w * .685;
    const QRectF box((950 - w) / 2 + state.value("backgroundOffsetX").toDouble() * 1.58, (650 - h) / 2 + state.value("backgroundOffsetY").toDouble() * 1.3 + 87, w, h);
    art.drawImage(box, image, crop(image, box.size(), zoom, 0, 0));
    QRadialGradient mask(QPointF(475,430), 430); mask.setColorAt(0, Qt::white); mask.setColorAt(.52, Qt::white); mask.setColorAt(.74, QColor(255,255,255,140)); mask.setColorAt(1, Qt::transparent);
    art.setCompositionMode(QPainter::CompositionMode_DestinationIn); art.fillRect(QRectF(0,0,950,860), mask); art.end(); p->drawImage(boundingRect(), buffer);
}
