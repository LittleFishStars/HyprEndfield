#include "nowplayingcontroller.hpp"
#include "splayerlyrics.hpp"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QDebug>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    NowPlayingController player;
    SPlayerLyrics lyrics(&player);
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("nowPlayingController", &player);
    engine.rootContext()->setContextProperty("splayerLyrics", &lyrics);
    for (const auto *font : {"harmonyRegular", "harmonyMedium", "harmonyBold"})
        engine.rootContext()->setContextProperty(font, QVariantMap{{"name", "Sans Serif"}});
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(PLAYER_PAGE_PATH)));
    const std::unique_ptr<QObject> root(component.create());
    if (!root) { qCritical() << component.errors(); return 1; }
    auto *item = qobject_cast<QQuickItem *>(root.get());
    if (!item || item->width() != 590 || item->height() != 510) return 2;
    if (player.active()) return 3; // Hidden pages must not contact real players.
    QVariant time;
    if (!QMetaObject::invokeMethod(root.get(), "time", Q_RETURN_ARG(QVariant, time), Q_ARG(QVariant, 125.0))
        || time.toString() != "2:05") return 4;
    qInfo() << "NowPlayingPage loads; original content dimensions and time formatting verified.";
}
