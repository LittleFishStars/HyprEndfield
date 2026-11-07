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
#include <QTextBoundaryFinder>
#include <QUuid>
#include <QtConcurrentRun>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {
QImage readImage(const QString &path) { QImageReader reader(path); reader.setAutoTransform(true); return reader.read(); }
bool ownedImage(const QString &name) { return QRegularExpression("^[0-9a-f-]{36}\\.[a-z0-9]{1,8}$").match(name).hasMatch(); }
QString characterPrefix(const QString &text, int count) {
    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int end = 0;
    for (int i = 0; i < count; ++i) {
        const int next = finder.toNextBoundary();
        if (next < 0) return text;
        end = next;
    }
    return text.left(end);
}
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
QVariantMap ProfileController::state() const { auto result = m_values; result["error"] = m_error; result["busy"] = m_import.isRunning(); result["lockedFields"] = m_locked; return result; }
void ProfileController::setLockedFields(const QStringList &fields) { if (m_locked != fields) { m_locked = fields; emit changed(); } }
bool ProfileController::applyGameFields(const QVariantMap &fields) {
    if (!m_readable) return false;
    const auto previous = m_values;
    m_applyingGame = true;
    // UserProfile.normalizedEditableValues / applyGameProfileUpdate in the
    // source: normalize incoming game values before validating the transaction.
    for (auto i = fields.cbegin(); i != fields.cend(); ++i) {
        QVariant value = i.value();
        if (i.key() == "name" || i.key() == "tag")
            value = characterPrefix(value.toString().trimmed(), i.key() == "name" ? 20 : 10);
        if (i.key() == "permissionLevel" || i.key() == "explorationLevel") {
            bool ok = false; const auto level = value.toLongLong(&ok);
            if (ok) value = std::clamp<qlonglong>(level, 1, i.key() == "permissionLevel" ? 60 : 7);
        }
        if (!setField(i.key(), value, false)) {
            m_values = previous; m_applyingGame = false; emit changed(); return false;
        }
    }
    m_applyingGame = false;
    const bool saved = save(); emit changed(); return saved;
}
QString ProfileController::importGameAvatar(const QByteArray &png) {
    if (!m_readable || m_import.isRunning() || png.size() > 4 * 1024 * 1024) return {};
    const auto image = QImage::fromData(png, "PNG");
    if (image.isNull() || image.width() > 512 || image.height() > 512) return {};
    const auto name = QUuid::createUuid().toString(QUuid::WithoutBraces) + ".png";
    QSaveFile file(m_directory + '/' + name);
    if (!QDir().mkpath(m_directory) || !file.open(QIODevice::WriteOnly) || file.write(png) != png.size() || !file.commit()) return {};
    const auto previous = m_values.value("avatar").toString(); m_values["avatar"] = name;
    if (!save()) { QFile::remove(m_directory + '/' + name); return {}; }
    m_avatar = image; if (ownedImage(previous)) QFile::remove(m_directory + '/' + previous);
    emit changed(); return name;
}
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
    if (!m_applyingGame && m_locked.contains(field)) return false;
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
        if (field == "name") valid = !text.isEmpty() && characterPrefix(text, 20) == text && !text.contains(QRegularExpression("[\\r\\n]"));
        if (field == "tag") valid = !text.isEmpty() && characterPrefix(text, 10) == text && !text.contains(QRegularExpression("[\\s#]"));
        if (field == "introduction") valid = text.size() <= 150;
        if (field == "playerID") valid = !text.isEmpty() && text.size() <= 64 && !text.contains(QRegularExpression("[\\r\\n]"));
        if (field == "awakeningDate") valid = QDate::fromString(text, Qt::ISODate).isValid() && text.size() == 10;
        if (field == "birthday") valid = QDate::fromString("2000-" + text, Qt::ISODate).isValid() && text.size() == 5;
        if (field == "themeColor") { valid = text.isEmpty() || QRegularExpression("^#[0-9a-fA-F]{6}$").match(text).hasMatch(); text = text.toUpper(); }
        accepted = text;
    }
    if (!valid) { m_error = tr("输入内容超出允许范围，或格式不正确。"); emit changed(); return false; }
    const bool changedValue = m_values.value(field) != accepted;
    if (!changedValue && !commit && m_error.isEmpty()) return true;
    m_values[field] = accepted; m_error.clear(); const bool saved = !commit || save(); emit changed();
    if (saved && commit && changedValue && (field.startsWith("avatar") || field.startsWith("thumbnail") || field.startsWith("background")))
        emit cropChanged(field.startsWith("background") ? "background" : "thumbnail");
    return saved;
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
struct ProfileArtwork::Cache { QImage mask; };
ProfileArtwork::ProfileArtwork(QQuickItem *parent) : ProfileImageItem(parent), m_cache(std::make_shared<Cache>()) {
    setAntialiasing(true);
    connect(this, &ProfileArtwork::appearanceChanged, this, &ProfileArtwork::refreshImage);
    const auto resizeTexture = [this] { setTextureSize(QSize(qCeil(width() * 2), qCeil(height() * 2))); refreshImage(); update(); };
    connect(this, &QQuickItem::widthChanged, this, resizeTexture); connect(this, &QQuickItem::heightChanged, this, resizeTexture);
}
void ProfileArtwork::setController(ProfileController *value) {
    if (m_controller == value) return;
    if (m_controller) disconnect(m_controller, nullptr, this, nullptr);
    m_controller = value;
    clearImage();
    if (value) {
        connect(value, &ProfileController::changed, this, &ProfileArtwork::refreshImage);
        connect(value, &QObject::destroyed, this, [this] { m_controller=nullptr; clearImage(); });
    }
    emit appearanceChanged();
}
void ProfileArtwork::refreshImage() {
    if (!m_controller || width()<=0 || height()<=0) { clearImage(); return; }
    if (m_lastKind!=m_kind) { clearImage();m_lastKind=m_kind; }
    if (m_kind == "frame") {
        const auto accent=m_accent;
        requestImage({m_kind,QVariant::fromValue(accent)},[accent] {
            static const QImage original = [] { QFile f(":/assets/icons/profile-frame.bgra"); if (!f.open(QIODevice::ReadOnly)) return QImage(); const auto bytes = f.read(256 * 256 * 4); if (bytes.size() != 256 * 256 * 4) return QImage(); return QImage(reinterpret_cast<const uchar *>(bytes.constData()), 256, 256, QImage::Format_ARGB32).copy(1, 1, 254, 254).flipped(Qt::Vertical); }();
            QImage frame=original;
            for(int y=0;y<frame.height();++y) for(int x=0;x<frame.width();++x) {
                auto c=frame.pixelColor(x,y);c.setRedF(c.redF()*accent.redF());c.setGreenF(c.greenF()*accent.greenF());c.setBlueF(c.blueF()*accent.blueF());frame.setPixelColor(x,y,c);
            }
            return frame;
        });
        return;
    }
    const auto state=m_controller->state();
    const auto photo=m_controller->image(m_kind=="avatar" ? "avatar" : "background");
    const QString prefix=m_kind=="avatar" ? "avatar" : m_kind=="thumbnail" ? "thumbnail" : "background";
    m_zoom=state.value(prefix+"Zoom").toDouble();
    m_offsetX=state.value(prefix+"OffsetX").toDouble();m_offsetY=state.value(prefix+"OffsetY").toDouble();
    if(m_kind!="background") {
        // The small portrait retains its source pixels; panning and zooming
        // only change the draw rectangle, as in the source page's layer transform.
        const QVariantList geometry{m_zoom,m_offsetX,m_offsetY};
        if(geometry!=m_geometry) { m_geometry=geometry;update(); }
        requestImage({m_kind,photo.cacheKey()},[photo] { return photo; });
        return;
    }
    const auto size=textureSize();
    const double zoom=m_zoom,w=state.value("backgroundWidth").toDouble()*1.58,h=w*.685;
    const QRectF box((950-w)/2+m_offsetX*1.58,(650-h)/2+m_offsetY*1.3+87,w,h);
    const auto cache=m_cache;
    requestImage({m_kind,photo.cacheKey(),size,zoom,box},[photo,size,zoom,box,cache] {
        if(photo.isNull()) return QImage{};
        // The radial fade is fixed in page coordinates; cache it independently
        // of the photo's position, zoom and size. Keep the original pixels.
        if(cache->mask.size()!=size) {
            cache->mask=QImage(size,QImage::Format_ARGB32_Premultiplied);cache->mask.fill(Qt::transparent);
            QPainter maskPainter(&cache->mask);maskPainter.scale(size.width()/950.,size.height()/860.);
            QRadialGradient mask(QPointF(475,430),430);mask.setColorAt(0,Qt::white);mask.setColorAt(.52,Qt::white);mask.setColorAt(.74,QColor(255,255,255,140));mask.setColorAt(1,Qt::transparent);
            maskPainter.fillRect(QRectF(0,0,950,860),mask);
        }
        QImage buffer(size,QImage::Format_ARGB32_Premultiplied);buffer.fill(Qt::transparent);
        QPainter art(&buffer);art.setRenderHint(QPainter::SmoothPixmapTransform);art.scale(size.width()/950.,size.height()/860.);
        art.drawImage(box,photo,crop(photo,box.size(),zoom,0,0));
        art.resetTransform();art.setCompositionMode(QPainter::CompositionMode_DestinationIn);art.drawImage(QPoint(),cache->mask);art.end();
        return buffer;
    },{m_kind,photo.cacheKey(),size});
}
void ProfileArtwork::paint(QPainter *p) {
    if(!m_controller || width()<=0 || height()<=0) return;
    p->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    const auto &picture=renderedImage();
    if(m_kind=="frame" || m_kind=="background") {
        if(!picture.isNull()) p->drawImage(boundingRect(),picture);
        return;
    }
    if(picture.isNull()) {
        if(m_kind=="avatar") {
            p->save();p->translate(width()/2+m_offsetX*width()*.5,height()/2+m_offsetY*height()*.5);
            p->scale(width()/55*m_zoom,height()/55*m_zoom);p->translate(-27.5,-27.5);
            QPainterPath silhouette; silhouette.addEllipse(QRectF(19.8, 11, 16.5, 17)); silhouette.moveTo(9.9,47.3); silhouette.cubicTo(13.2,24.75,42.35,24.75,45.65,47.3); silhouette.closeSubpath(); p->fillPath(silhouette, QColor(245,245,245,161)); p->restore();
        }
        return;
    }
    p->drawImage(boundingRect(),picture,crop(picture,QSizeF(width(),height()),m_zoom,m_offsetX,m_offsetY));
}
