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
GameProfileArtwork::GameProfileArtwork(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this,&GameProfileArtwork::appearanceChanged,this,[this] { m_dirty=true; update(); });
    const auto resize=[this] { setTextureSize(QSize(std::ceil(width()*3),std::ceil(height()*3))); update(); };
    connect(this,&QQuickItem::widthChanged,this,resize); connect(this,&QQuickItem::heightChanged,this,resize);
}
void GameProfileArtwork::setController(ProfileController *value) {
    if (m_controller==value) return;
    if (m_controller) disconnect(m_controller,nullptr,this,nullptr);
    m_controller=value;
    if (value) connect(value,&ProfileController::changed,this,&GameProfileArtwork::appearanceChanged);
    emit appearanceChanged();
}
void GameProfileArtwork::paint(QPainter *painter) {
    if (!m_controller || width()<=0 || height()<=0) return;
    const auto state=m_controller->state(); const auto kind=m_spec.value("dynamic").toString();
    const std::array<double,3> accent{m_accent.redF(),m_accent.greenF(),m_accent.blueF()};
    if (m_dirty) {
        m_dirty=false; m_image={};
        if (kind=="card" || kind=="hover") {
            auto artwork=kind=="hover" ? p::idCardHoverArtwork(cardArtwork(),accent) : p::idCardThemedArtwork(cardArtwork(),accent);
            const auto &photo=m_controller->image("background");
            if(kind=="card" && !m_recording && !photo.isNull()) {
                const auto crop=p::profileRenderedImage(p::profileWorkingCopy(profileImage(photo)),1,{412,158},state.value("thumbnailZoom").toDouble(),{state.value("thumbnailOffsetX").toDouble(),state.value("thumbnailOffsetY").toDouble()},2);
                artwork=p::idCardCompositedBackground(crop,artwork);
            }
            m_image=image(artwork);
        } else if (kind=="avatar") {
            const auto &photo=m_controller->image("avatar");
            if(!m_recording && !photo.isNull()) m_image=image(p::profileRenderedImage(p::profileWorkingCopy(profileImage(photo)),1,{136,136},state.value("avatarZoom").toDouble(),{state.value("avatarOffsetX").toDouble(),state.value("avatarOffsetY").toDouble()},2));
        } else if (kind!="disabledGlow") {
            const auto file=m_spec.value("asset").toString();
            m_image=file.isEmpty() ? QImage(1,1,QImage::Format_RGBA8888) : QImage(":/assets/profile-card/"+file).convertToFormat(QImage::Format_RGBA8888);
            if (file.isEmpty()) m_image.fill(Qt::white);
            auto color=m_spec.value("color").toList();
            if(color.size()==4) {
                if(m_spec.value("accent").toBool()) { color[0]=accent[0];color[1]=accent[1];color[2]=accent[2]; }
                for(int y=0;y<m_image.height();++y) {
                    auto *row=m_image.scanLine(y);
                    for(int x=0;x<m_image.width();++x) for(int c=0;c<4;++c) row[x*4+c]=std::clamp(std::lround(row[x*4+c]*color[c].toDouble()),0L,255L);
                }
            }
        }
    }
    painter->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
    if(kind=="avatar" && m_image.isNull() && !m_recording) {
        painter->translate(width()/2+state.value("avatarOffsetX").toDouble()*width()/2,height()/2+state.value("avatarOffsetY").toDouble()*height()/2);
        const double zoom=state.value("avatarZoom").toDouble(); painter->scale(width()/55*zoom,height()/55*zoom); painter->translate(-27.5,-27.5);
        QPainterPath silhouette; silhouette.addEllipse(QRectF(19.8,11,16.5,17)); silhouette.moveTo(9.9,47.3);silhouette.cubicTo(13.2,24.75,42.35,24.75,45.65,47.3);silhouette.closeSubpath();painter->fillPath(silhouette,QColor(245,245,245,161));return;
    }
    if(m_spec.value("binding")=="levelSlider") painter->setClipRect(QRectF(0,0,width()*std::clamp(state.value("permissionLevel").toInt(),1,60)/60.,height()));
    if(!m_image.isNull()) sliced(painter,m_image,{width(),height()},m_spec.value("border").toList());
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
