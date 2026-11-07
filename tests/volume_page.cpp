#include "audiocontroller.hpp"
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QDebug>
#include <QFontDatabase>
#include <QDir>
#include <QFileInfo>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QtTest>

class Fixture final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool active MEMBER active NOTIFY activeChanged)
public:
    QVariantMap values = AudioController().state();
    bool active = false;
    int writes = 0;
    QString lastKey;
    QVariantMap state() const { return values; }
    Q_INVOKABLE bool setVolume(double, const QString &key) { ++writes; lastKey = key; return true; }
    Q_INVOKABLE bool setBalance(double, const QString &key) { ++writes; lastKey = key; return true; }
    Q_INVOKABLE bool setApplicationVolume(double, const QString &key) { ++writes; lastKey = key; return true; }
    Q_INVOKABLE bool setMuted(bool, const QString &) { return true; }
    Q_INVOKABLE bool selectDevice(const QString &key, bool) { lastKey = key; return true; }
signals:
    void stateChanged();
    void activeChanged();
};

int main(int argc, char **argv)
{
    const bool render = std::any_of(argv, argv + argc, [](const char *arg) { return QByteArray(arg) == "--render"; });
    if (render) {
        // Match the actual HUD; the offscreen platform otherwise chooses Qt's
        // software backend, which does not correctly clip these vector shapes.
        qputenv("QT_QUICK_BACKEND", "rhi");
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        QSurfaceFormat format; format.setSamples(4); QSurfaceFormat::setDefaultFormat(format);
    }
    QGuiApplication application(argc, argv);
    Fixture audio;
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("audioController", &audio);
    for (const auto &[name, weight] : QList<QPair<QString, QString>>{{"harmonyRegular", "Regular"}, {"harmonyMedium", "Medium"}, {"harmonyBold", "Bold"}}) {
        const QString path = QFileInfo(QStringLiteral(VOLUME_PAGE_PATH)).dir().filePath("../assets/fonts/HarmonyOS_Sans_" + weight + ".ttf");
        const int font = QFontDatabase::addApplicationFont(path);
        engine.rootContext()->setContextProperty(name, QVariantMap{{"name", font < 0 ? "Sans Serif" : QFontDatabase::applicationFontFamilies(font).first()}});
    }
    bool warning = false;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        warning = true; qWarning() << errors;
    });
    QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(VOLUME_PAGE_PATH)));
    QQuickWindow window;
    const std::unique_ptr<QObject> root(component.create());
    if (!root) { qCritical() << component.errors(); return 1; }
    auto *item = qobject_cast<QQuickItem *>(root.get());
    if (!item || item->width() != 590 || item->height() != 510 || audio.active) return 2;
    auto *master = root->findChild<QQuickItem *>("masterSlider");
    auto *balance = root->findChild<QQuickItem *>("balanceSlider");
    if (!master || !balance || master->width() != 338 || balance->width() != 376) return 3;
    if (master->isEnabled() || balance->isEnabled()) return 4;
    QVariant text;
    for (const auto &[number, expected] : QList<QPair<double, QString>>{{0, "C"}, {-50, "L 50"}, {50, "R 50"}}) {
        if (!QMetaObject::invokeMethod(balance, "formattedValue", Q_RETURN_ARG(QVariant, text), Q_ARG(QVariant, number)) || text != expected) return 5;
    }
    root->setProperty("reduceMotion", true);
    QMetaObject::invokeMethod(root.get(), "navigate", Q_ARG(QVariant, 1));
    if (root->property("tab").toInt() != 1) return 6;
    QVariantList devices;
    for (int i = 0; i < 9; ++i) devices.append(QVariantMap{{"key", QString::number(i)}, {"label", QString("测试输出设备 %1").arg(i + 1)}});
    audio.values["ready"] = true;
    audio.values["outputs"] = devices;
    audio.values["inputs"] = QVariantList{QVariantMap{{"key", "mic"}, {"label", "内置麦克风"}}};
    audio.values["outputKey"] = "0";
    audio.values["inputKey"] = "mic";
    audio.values["outputName"] = "内置音频 模拟立体声";
    audio.values["inputName"] = "内置麦克风";
    audio.values["volume"] = 42.0;
    audio.values["canBalance"] = true;
    QVariantList applications;
    for (int i = 0; i < 5; ++i) applications.append(QVariantMap{{"key", QString("app:%1").arg(i)},
        {"label", i == 0 ? "SPlayer" : QString("音频应用 %1").arg(i + 1)}, {"pid", QString::number(1234 + i)}, {"icon", ""}, {"volume", 70.0 + i * 5}});
    audio.values["applications"] = applications;
    emit audio.stateChanged();
    application.processEvents();
    window.resize(960, 720); window.setColor(QColor("#222222"));
    item->setParentItem(window.contentItem()); item->setPosition({50, 50}); item->setTransformOrigin(QQuickItem::TopLeft); item->setScale(1.4);
    window.show(); QTest::qWait(100);
    root->setProperty("presented", true);
    if (!audio.active || !master->isEnabled() || !balance->isEnabled()) return 8;
    if (render) {
        const auto image = window.grabWindow();
        if (image.isNull() || image.pixelColor(400, 540) != window.color()) return 17;
        image.save("/tmp/hyprendfield-volume-apps.png");
    }
    QMetaObject::invokeMethod(root.get(), "navigate", Q_ARG(QVariant, 0));
    QTest::qWait(100);
    if (render) {
        const auto image = window.grabWindow();
        if (image.isNull() || image.pixelColor(400, 540) != window.color()) return 18;
        image.save("/tmp/hyprendfield-volume-devices.png");
    }
    const auto position = master->mapToScene({50, 15}).toPoint();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, position);
    if (audio.writes != 1 || audio.lastKey != "0") return 9;
    audio.values["outputKey"] = "1";
    emit audio.stateChanged();
    QTest::mouseMove(&window, position + QPoint(50, 0));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, position + QPoint(50, 0));
    if (audio.writes != 1 || master->property("dragging").toBool()) return 10;
    audio.values["volume"] = 64.0;
    emit audio.stateChanged();
    if (master->property("value").toDouble() != 64) return 11;
    auto *dropdown = root->findChild<QQuickItem *>("outputSelector");
    dropdown->setProperty("opened", true);
    QTest::qWait(100);
    if (application.arguments().contains("--render")) window.grabWindow().save("/tmp/hyprendfield-volume-menu.png");
    audio.values["volume"] = 65.0;
    emit audio.stateChanged();
    if (!dropdown->property("opened").toBool()) return 12;
    QMetaObject::invokeMethod(dropdown, "move", Q_ARG(QVariant, 1));
    QMetaObject::invokeMethod(dropdown, "choose");
    if (audio.lastKey != "2" || dropdown->property("opened").toBool()) return 13;
    QMetaObject::invokeMethod(root.get(), "navigate", Q_ARG(QVariant, 1));
    QMetaObject::invokeMethod(root.get(), "nudge", Q_ARG(QVariant, 2));
    if (audio.writes != 2 || audio.lastKey != "app:0") return 14;
    QMetaObject::invokeMethod(root.get(), "scrollApps", Q_ARG(QVariant, 120), Q_ARG(QVariant, false));
    if (root->property("appOffset").toDouble() != 120) return 15;
    audio.values["applications"] = QVariantList{};
    emit audio.stateChanged();
    QTest::qWait(100);
    root->setProperty("presented", false);
    if (audio.active) return 16;
    if (warning) return 7;
    item->setParentItem(nullptr);
    qInfo() << "VolumePage: geometry, data binding, device-change drag cancellation, menus, application sliders and scrolling verified.";
}
#include "volume_page.moc"
