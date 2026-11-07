#include "accountartwork.hpp"
#include "endfield/modules/profile_artwork.hpp"
#include "endfield/modules/hypergryph_account_sanity_gauge.hpp"
#include <QFile>
#include <QPainter>
#include <QPainterPath>
#include <cmath>

namespace p = endfield::modules;
namespace h = p::hypergryph;
namespace {
p::ProfileImage profileImage(const QImage &value) {
    const auto rgba = value.convertToFormat(QImage::Format_RGBA8888);
    return {unsigned(rgba.width()), unsigned(rgba.height()), std::vector<std::uint8_t>(rgba.constBits(), rgba.constBits() + rgba.sizeInBytes()), false};
}
QImage image(const p::ProfileImage &value) {
    if (!value.width || !value.height) return {};
    return QImage(value.rgba.data(), value.width, value.height, value.width * 4,
        value.premultiplied ? QImage::Format_RGBA8888_Premultiplied : QImage::Format_RGBA8888).copy();
}
QImage image(const h::GaugeImage &value) {
    return QImage(value.rgba.data(), value.width, value.height, value.width * 4, QImage::Format_RGBA8888_Premultiplied).copy();
}
QByteArray bytes(const char *file) { QFile input(file); return input.open(QIODevice::ReadOnly) ? input.readAll() : QByteArray{}; }
const p::ProfileImage &cardArtwork() {
    static const auto card = [] {
        const auto data = bytes(":/assets/profile-card/business-card.bgra");
        return p::profileSpriteFromMip({reinterpret_cast<const std::uint8_t *>(data.constData()), size_t(data.size())}, 532, 204, {0, 0, 530, 204});
    }();
    return card;
}
const h::GaugeAssets &gaugeAssets() {
    static const auto assets = [] {
        h::GaugeAssets result;
        auto numerals = h::AccountNumerals::parse(bytes(":/assets/account/account-numerals.json").toStdString());
        if (numerals) result.numerals = std::move(*numerals);
        const auto load = [](const char *path, unsigned w, unsigned h) {
            const auto data = bytes(path);
            if (data.size() != qsizetype(w * h * 4)) return h::GaugeImage{};
            return h::GaugeImage{w, h, std::vector<std::uint8_t>(data.cbegin(), data.cend())};
        };
        result.back = load(":/assets/account/wallet-back.rgba", 60, 50);
        result.deco = load(":/assets/account/wallet-deco.rgba", 60, 50);
        result.icon = load(":/assets/account/item-ap.rgba", 80, 80);
        result.silhouette = load(":/assets/account/wallet-silhouette.rgba", 60, 50);
        return result;
    }();
    return assets;
}
void sliced(QPainter *painter, const QImage &source, QSizeF size, QVariantList b) {
    if (b.size() != 4) { painter->drawImage(QRectF(QPointF(), size), source); return; }
    double l=b[0].toDouble(), t=b[1].toDouble(), r=b[2].toDouble(), btm=b[3].toDouble();
    const double sx=std::min(1., size.width()/std::max(1.,l+r)), sy=std::min(1.,size.height()/std::max(1.,t+btm));
    const double x[]{0,l*sx,size.width()-r*sx,size.width()}, y[]{0,t*sy,size.height()-btm*sy,size.height()};
    const double u[]{0,l,double(source.width())-r,double(source.width())}, v[]{0,t,double(source.height())-btm,double(source.height())};
    for (int row=0;row<3;++row) for(int col=0;col<3;++col)
        if(x[col+1]>x[col] && y[row+1]>y[row] && u[col+1]>u[col] && v[row+1]>v[row])
            painter->drawImage(QRectF(x[col],y[row],x[col+1]-x[col],y[row+1]-y[row]),source,QRectF(u[col],v[row],u[col+1]-u[col],v[row+1]-v[row]));
}
}
struct GameProfileArtwork::Cache {
    qint64 photoKey = 0;
    p::ProfileImage working, themed, scaled;
    QColor accent;
};
GameProfileArtwork::GameProfileArtwork(QQuickItem *parent) : ProfileImageItem(parent), m_cache(std::make_shared<Cache>()) {
    setAntialiasing(true);
    connect(this,&GameProfileArtwork::appearanceChanged,this,[this] { refreshImage(); update(); });
    const auto resize=[this] { setTextureSize(QSize(std::ceil(width()*3),std::ceil(height()*3))); refreshImage(); update(); };
    connect(this,&QQuickItem::widthChanged,this,resize); connect(this,&QQuickItem::heightChanged,this,resize);
}
void GameProfileArtwork::setController(ProfileController *value) {
    if (m_controller==value) return;
    if (m_controller) disconnect(m_controller,nullptr,this,nullptr);
    m_controller=value;
    clearImage();
    m_cache=std::make_shared<Cache>();
    if (value) {
        connect(value,&ProfileController::changed,this,&GameProfileArtwork::refreshImage);
        connect(value,&QObject::destroyed,this,[this] { m_controller=nullptr; clearImage(); });
    }
    emit appearanceChanged();
}
void GameProfileArtwork::refreshImage() {
    if (!m_controller || width()<=0 || height()<=0 || m_spec.value("fontSize").toDouble()>0) { clearImage(); return; }
    const auto state=m_controller->state(); const auto kind=m_spec.value("dynamic").toString();
    const int level=state.value("permissionLevel").toInt();
    if (m_level!=level) { m_level=level; if(m_spec.value("binding")=="levelSlider") update(); }
    const auto photo=m_recording || (kind!="card" && kind!="avatar") ? QImage{} : m_controller->image(kind=="avatar" ? "avatar" : "background");
    const auto prefix=kind=="avatar" ? QStringLiteral("avatar") : QStringLiteral("thumbnail");
    const double zoom=state.value(prefix+"Zoom").toDouble();
    const endfield::core::Point offset{state.value(prefix+"OffsetX").toDouble(),state.value(prefix+"OffsetY").toDouble()};
    auto color=m_spec.value("color").toList();
    if(color.size()==4 && m_spec.value("accent").toBool()) { color[0]=m_accent.redF();color[1]=m_accent.greenF();color[2]=m_accent.blueF(); }
    const auto file=m_spec.value("asset").toString();
    QVariantList key{kind};
    // IdCardBinding keeps independent avatar/background/hover keys. In
    // particular, moving the large editor background cannot invalidate these.
    if(kind=="card" || kind=="hover") key << QVariant::fromValue(m_accent);
    if(kind=="card" || kind=="avatar") {
        key << photo.cacheKey() << m_recording;
        if(!photo.isNull() || kind=="avatar") key << zoom << offset.x << offset.y;
    } else if(kind!="hover") key << file << QVariant(color);
    QVariantList source;
    if(kind=="card" || kind=="avatar") source={kind,photo.cacheKey(),m_recording};
    if(kind=="card") source << QVariant::fromValue(m_accent);
    const auto cache=m_cache; const auto accentColor=m_accent;
    requestImage(std::move(key),[cache,kind,photo,zoom,offset,accentColor,color,file]() -> QImage {
        const std::array<double,3> accent{accentColor.redF(),accentColor.greenF(),accentColor.blueF()};
        const auto working=[&]() -> const p::ProfileImage & {
            if(cache->photoKey!=photo.cacheKey()) {
                cache->working=p::profileWorkingCopy(profileImage(photo));
                cache->photoKey=photo.cacheKey();
            }
            return cache->working;
        };
        if (kind=="card" || kind=="hover") {
            if(kind=="hover") return image(p::idCardHoverArtwork(cardArtwork(),accent));
            if(cache->accent!=accentColor || !cache->themed.width) {
                cache->themed=p::idCardThemedArtwork(cardArtwork(),accent);
                cache->scaled={}; cache->accent=accentColor;
            }
            if(photo.isNull()) { cache->working={};cache->photoKey=0;return image(cache->themed); }
            const auto crop=p::profileRenderedImage(working(),1,{412,158},zoom,offset,2);
            if(cache->scaled.width!=crop.width || cache->scaled.height!=crop.height)
                cache->scaled=p::profileResampled(cache->themed,crop.width,crop.height);
            return image(p::idCardCompositedBackground(crop,cache->scaled));
        } else if (kind=="avatar") {
            if(photo.isNull()) { cache->working={};cache->photoKey=0;return {}; }
            return image(p::profileRenderedImage(working(),1,{136,136},zoom,offset,2));
        } else if (kind!="disabledGlow") {
            auto output=file.isEmpty() ? QImage(1,1,QImage::Format_RGBA8888) : QImage(":/assets/profile-card/"+file).convertToFormat(QImage::Format_RGBA8888);
            if (file.isEmpty()) output.fill(Qt::white);
            if(color.size()==4) {
                for(int y=0;y<output.height();++y) {
                    auto *row=output.scanLine(y);
                    for(int x=0;x<output.width();++x) for(int c=0;c<4;++c) row[x*4+c]=std::clamp(std::lround(row[x*4+c]*color[c].toDouble()),0L,255L);
                }
            }
            return output;
        }
        return {};
    },source);
}
void GameProfileArtwork::paint(QPainter *painter) {
    if (!m_controller || width()<=0 || height()<=0) return;
    const auto kind=m_spec.value("dynamic").toString();
    // Do not show a previously rendered personal image while recording-mode
    // replacement artwork is still being prepared.
    if(m_recording && (kind=="card" || kind=="avatar") && !imageIsCurrent()) return;
    const auto &picture=renderedImage();
    painter->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    if(kind=="avatar" && picture.isNull() && !m_recording) {
        const auto state=m_controller->state();
        painter->translate(width()/2+state.value("avatarOffsetX").toDouble()*width()/2,height()/2+state.value("avatarOffsetY").toDouble()*height()/2);
        const double zoom=state.value("avatarZoom").toDouble(); painter->scale(width()/55*zoom,height()/55*zoom); painter->translate(-27.5,-27.5);
        QPainterPath silhouette; silhouette.addEllipse(QRectF(19.8,11,16.5,17)); silhouette.moveTo(9.9,47.3);silhouette.cubicTo(13.2,24.75,42.35,24.75,45.65,47.3);silhouette.closeSubpath();painter->fillPath(silhouette,QColor(245,245,245,161));return;
    }
    if(m_spec.value("binding")=="levelSlider") painter->setClipRect(QRectF(0,0,width()*std::clamp(m_level,1,60)/60.,height()));
    if(!picture.isNull()) sliced(painter,picture,{width(),height()},m_spec.value("border").toList());
}
AccountGaugeArtwork::AccountGaugeArtwork(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setWidth(168);setHeight(54);setTextureSize({504,162});
    connect(this,&AccountGaugeArtwork::appearanceChanged,this,[this]{update();});
}
void AccountGaugeArtwork::paint(QPainter *painter) {
    const auto &assets=gaugeAssets(); if(!assets.back.width || !assets.icon.width) return;
    auto artwork=h::composeGaugeArtwork(assets,3);
    if(auto number=assets.numerals.render(m_value.toStdString(),121,30,3)) h::composeNumber(artwork,*number,3);
    painter->setRenderHint(QPainter::SmoothPixmapTransform); painter->drawImage(boundingRect(),image(artwork));
    if(m_highlighted) painter->drawImage(QRectF(0,12,168,30),image(h::composeHighlight(assets,3)));
}
