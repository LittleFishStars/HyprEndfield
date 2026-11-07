#include "orbipomcontroller.hpp"
#include <QFile>
#include <QImageReader>
#include <QPainter>
#include <QSettings>
#include <algorithm>
#include <cmath>
#include <numbers>

OrbiPomController::OrbiPomController(QObject *parent) : QObject(parent) {
    m_best = std::max(0, QSettings().value("orbipom/bestScore.v1", 0).toInt());
    m_clock.setTimerType(Qt::PreciseTimer);
    connect(&m_clock, &QTimer::timeout, this, [this] {
        const double elapsed = m_elapsed.nsecsElapsed() / 1e9; m_elapsed.restart();
        advance(elapsed);
    });
}
OrbiPomController::~OrbiPomController() { m_clock.stop(); saveBest(); if (m_engine) call("destroy"); }
QVariantMap OrbiPomController::state() const {
    auto result = m_state;
    if (result.isEmpty()) result = {{"state", "idle"}, {"score", 0}, {"energy", 0}, {"energyProgress", 0}, {"swapCharge", 0}, {"nextLevel", 1}, {"currentLevel", 1}, {"paused", true}};
    result["best"] = std::max({m_best, result.value("score").toInt(), result.value("highScore").toInt()});
    result["manuallyPaused"] = m_manualPause; result["error"] = m_error;
    return result;
}
bool OrbiPomController::ensureRuntime() {
    if (m_engine) return m_error.isEmpty();
    m_engine = std::make_unique<QJSEngine>();
    for (const auto *name : {"matter-0.20.0.js", "orbipom.js"}) {
        QFile source(QString(":/assets/orbipom/") + name);
        if (!source.open(QIODevice::ReadOnly) || m_engine->evaluate(QString::fromUtf8(source.readAll()), name).isError()) {
            m_error = tr("小游戏无法载入。"); emit changed(); return false;
        }
    }
    m_bridge = m_engine->globalObject().property("OrbiPom");
    return m_bridge.isObject();
}
QJSValue OrbiPomController::call(const QString &method, const QJSValueList &args) {
    if (!m_engine) return {};
    const auto result = m_bridge.property(method).callWithInstance(m_bridge, args);
    if (result.isError()) { m_error = tr("小游戏运行出错。"); m_clock.stop(); emit changed(); }
    return result;
}
void OrbiPomController::refresh() {
    m_state = call("snapshot").toVariant().toMap(); emit changed();
}
bool OrbiPomController::start(int seed) {
    const bool restarting = bool(m_engine);
    if (!ensureRuntime()) return false;
    m_error.clear(); m_manualPause = false;
    call("highScore", {m_best});
    call("start", {seed < 0 ? QJSValue(QJSValue::NullValue) : QJSValue(seed)});
    refresh(); reconcile();
    if (m_error.isEmpty()) emit actionFinished(restarting ? "restarted" : "started", true, {});
    return m_error.isEmpty();
}
void OrbiPomController::saveBest() {
    const int best = state().value("best").toInt();
    if (best > m_best) { m_best = best; QSettings().setValue("orbipom/bestScore.v1", best); }
}
void OrbiPomController::reconcile() {
    const bool run = m_active && !m_held && !m_manualPause && m_error.isEmpty() && m_state.value("state") == "playing";
    if (m_engine && m_state.value("paused").toBool() == run) { call("pause", {!run}); refresh(); }
    const int interval = m_lowPower ? 33 : 16;
    if (!run) { m_clock.stop(); saveBest(); }
    else if (!m_clock.isActive() || m_clock.interval() != interval) { m_elapsed.start(); m_clock.start(interval); }
}
void OrbiPomController::setActive(bool value) {
    if (m_active == value) return;
    m_active = value; if (value && !m_engine) start(); else reconcile(); emit activityChanged();
}
void OrbiPomController::setHeld(bool value) { if (m_held == value) return; m_held = value; reconcile(); emit activityChanged(); }
void OrbiPomController::setLowPower(bool value) { if (m_lowPower == value) return; m_lowPower = value; reconcile(); emit activityChanged(); }
void OrbiPomController::togglePause() {
    if (m_state.value("state") != "playing") return;
    m_manualPause = !m_manualPause; reconcile(); emit changed();
}
void OrbiPomController::advance(qreal seconds) {
    if (!m_active || m_held || m_manualPause || !m_error.isEmpty() || m_state.value("state") != "playing" || !std::isfinite(seconds) || seconds <= 0) return;
    m_state = call("advance", {seconds}).toVariant().toMap();
    if (m_state.value("state") == "over") { saveBest(); reconcile(); emit actionFinished("finished", true, {}); }
    emit changed();
}
void OrbiPomController::move(qreal x, qreal y) {
    if (!m_active || m_state.value("paused").toBool() || !std::isfinite(x) || !std::isfinite(y)) return;
    const auto interaction = call("move", {x, y}).toVariant().toMap();
    for (auto it = interaction.begin(); it != interaction.end(); ++it) m_state[it.key()] = it.value();
    emit changed();
}
void OrbiPomController::release(qreal x, qreal y) {
    if (!m_active || m_state.value("paused").toBool() || !std::isfinite(x) || !std::isfinite(y)) return;
    call("pointerUp", {x, y}); refresh();
}
void OrbiPomController::drop() { if (m_active && !m_state.value("paused").toBool()) { call("drop"); refresh(); } }
void OrbiPomController::skill(const QString &name) {
    if (!m_active || m_state.value("paused").toBool() || !QStringList{"clear", "wind", "shake", "swap"}.contains(name)) return;
    call(m_state.value("skill") == name ? "cancelSkill" : "activate", m_state.value("skill") == name ? QJSValueList{} : QJSValueList{name}); refresh();
}
void OrbiPomController::cancelSkill() { if (m_engine) { call("cancelSkill"); refresh(); } }

OrbiPomBoard::OrbiPomBoard(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true); setMipmap(true);
    connect(this, &OrbiPomBoard::appearanceChanged, this, [this] { update(); });
    const auto resize = [this] { setTextureSize(QSize(qCeil(width() * 2), qCeil(height() * 2))); };
    connect(this, &QQuickItem::widthChanged, this, resize); connect(this, &QQuickItem::heightChanged, this, resize);
    for (int i = 0; i < 11; ++i) {
        QImageReader reader(QString(":/assets/orbipom/level-%1.png").arg(i + 1));
        const auto size = reader.size(); if (size.width() > 512 || size.height() > 512) reader.setScaledSize(size.scaled(512,512,Qt::KeepAspectRatio));
        m_images[i] = reader.read();
    }
}
void OrbiPomBoard::setController(OrbiPomController *value) {
    if (m_controller == value) return;
    if (m_controller) disconnect(m_controller, nullptr, this, nullptr);
    m_controller = value;
    if (value) connect(value, &OrbiPomController::changed, this, [this] { update(); });
    emit appearanceChanged();
}
void OrbiPomBoard::paint(QPainter *p) {
    p->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    p->scale(width() / 276., height() / 375.); p->setClipRect(QRectF(0,0,276,375));
    p->fillRect(QRectF(0,0,276,375), QColor("#141414"));
    // HUDShape.tickLadder: source 20 px pitch, full-height 1 px ticks.
    for (double x = 0; x <= 275; x += 20) p->fillRect(QRectF(x,0,1,375), QColor(255,255,255,15));
    if (!m_controller) return;
    const auto s = m_controller->state();
    if (!s.value("windSurfaceY").isNull()) {
        const double y = (s.value("windSurfaceY").toDouble() + 32.5) * 1.2;
        p->setPen(QColor(34,166,242,115)); p->setBrush(QColor(34,166,242,26)); p->drawRect(QRectF(0,y,276,std::max(0.,375-y)));
    }
    const bool ghost = s.value("previewVisible").toBool() && !s.value("paused").toBool() && s.value("skill").isNull();
    if (ghost) { auto tint = m_accent; tint.setAlphaF(.25); p->fillRect(QRectF(s.value("previewX").toDouble()*1.2-.5,20,1,355),tint); }
    const bool danger = !s.value("dangerSeconds").isNull();
    QPen line(danger ? m_dangerInk : QColor(229,56,59,115), danger ? 1.5 : 1);
    if (!danger) line.setDashPattern({5,4}); p->setPen(line); p->drawLine(QPointF(0,48.6),QPointF(276,48.6));
    static constexpr double circles[][3]={{.4136,.4970,.6078},{.4185,.4976,.5897},{.3965,.4908,.6011},{.4546,.5463,.5289},{.4425,.5064,.5464},{.4373,.4904,.5411},{.4374,.4956,.5639},{.4410,.5012,.5516},{.3287,.4991,.5464},{.4510,.4942,.5473},{.4451,.4895,.5491}};
    const auto piece = [&](const QVariantMap &b, bool marked) {
        const int level = std::clamp(b.value("level").toInt(),1,11)-1; const double side = b.value("size").toDouble()*1.2;
        p->save(); p->translate(b.value("x").toDouble()*1.2,(b.value("y").toDouble()+32.5)*1.2);
        p->rotate(b.value("angle").toDouble()*180/std::numbers::pi); const double scale=b.value("scale",1).toDouble(); p->scale(scale,scale); p->setOpacity(b.value("opacity",1).toDouble());
        const auto &image=m_images[level]; const auto box=QSizeF(image.size()).scaled(QSizeF(side,side),Qt::KeepAspectRatio);
        p->drawImage(QRectF(-box.width()/2,-box.height()/2,box.width(),box.height()),image);
        if(marked){p->setPen(QPen(m_accent,1.5));p->setBrush(Qt::NoBrush);p->drawEllipse(QPointF(side*(circles[level][1]-.5),side*(circles[level][2]-.5)),side*circles[level][0],side*circles[level][0]);}
        p->restore();
    };
    const auto selected=s.value("selectedBodyIDs").toList();
    for(const auto &value:s.value("bodies").toList()){const auto b=value.toMap();piece(b,selected.contains(b.value("id")) || (!s.value("hoverBodyID").isNull() && s.value("hoverBodyID")==b.value("id")));}
    if(ghost)piece({{"level",s.value("currentLevel")},{"x",s.value("previewX")},{"y",s.value("previewY")},{"size",s.value("previewSize")},{"scale",s.value("previewScale")},{"opacity",.5}},false);
    p->setPen(QColor("#3A3A3A"));p->setBrush(Qt::NoBrush);p->drawRect(QRectF(.5,.5,275,374));
}
