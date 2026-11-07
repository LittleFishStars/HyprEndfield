#include "clipboardcontroller.hpp"

#include <QBuffer>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QtTest>

namespace {
QByteArray testImage()
{
    QImage image(12, 8, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("#fffa00"));
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void populate(ClipboardController &controller)
{
    QVERIFY(controller.captureForTest(QStringLiteral("text/plain;charset=utf-8"), QStringLiteral("一段文字").toUtf8()));
    QVERIFY(controller.captureForTest(QStringLiteral("text/plain"), QByteArrayLiteral("https://example.com/docs")));
    QVERIFY(controller.captureForTest(QStringLiteral("image/png"), testImage()));
    QVERIFY(controller.captureForTest(QStringLiteral("text/uri-list"), QByteArrayLiteral("file:///tmp/Document.pdf\r\n")));
    const QVariantList items = controller.state().value("items").toList();
    for (const auto &value : items) {
        const QVariantMap item = value.toMap();
        if (item.value("kind").toString() == QStringLiteral("url")) {
            QVERIFY(controller.togglePin(item.value("id").toString()));
            break;
        }
    }
}

QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) if (auto *found = findItem(child, name)) return found;
    return nullptr;
}
}

class ClipboardTests final : public QObject
{
    Q_OBJECT
private slots:
    void connectsToWaylandClipboard()
    {
        if (!qEnvironmentVariableIsSet("HYPR_CLIPBOARD_LIVE")) QSKIP("live Wayland check is opt-in");
        ClipboardController controller;
        const QVariantMap state = controller.state();
        QVERIFY2(state.value("ready").toBool(), qPrintable(state.value("status").toString()));
        QCOMPARE(state.value("total").toInt(), 0);
    }

    void capturesWaylandOffers()
    {
        ClipboardController controller(nullptr, false);
        QVERIFY(!controller.captureForTest(QStringLiteral("text/plain"), QByteArrayLiteral("secret"), true));
        populate(controller);
        QVariantMap state = controller.state();
        QVERIFY(state.value("ready").toBool());
        QCOMPARE(state.value("total").toInt(), 4);
        QCOMPARE(state.value("capacity").toInt(), 10);
        QCOMPARE(state.value("removable").toInt(), 3);
        const QVariantList items = state.value("items").toList();
        QCOMPARE(items[0].toMap().value("kind").toString(), QStringLiteral("files"));
        QCOMPARE(items[1].toMap().value("kind").toString(), QStringLiteral("image"));
        QCOMPARE(items[1].toMap().value("meta").toString(), QStringLiteral("PNG   12 × 8"));
        QVERIFY(items[1].toMap().value("thumbnail").toString().startsWith(QStringLiteral("data:image/png;base64,")));
        QCOMPARE(items[2].toMap().value("kind").toString(), QStringLiteral("url"));
        QCOMPARE(items[2].toMap().value("meta").toString(), QStringLiteral("example.com"));
        QVERIFY(items[2].toMap().value("pinned").toBool());
        QCOMPARE(items[3].toMap().value("meta").toString(), QStringLiteral("4 字符"));
        QVERIFY(items[3].toMap().value("createdAt").toLongLong() > 0);

        QVERIFY(controller.captureForTest(QStringLiteral("text/plain;charset=utf-8"), QStringLiteral("一段文字").toUtf8()));
        QCOMPARE(controller.state().value("total").toInt(), 4);
        QVERIFY(controller.clearUnpinned());
        state = controller.state();
        QCOMPARE(state.value("total").toInt(), 1);
        QCOMPARE(state.value("removable").toInt(), 0);
        QVERIFY(state.value("items").toList().constFirst().toMap().value("pinned").toBool());
    }

    void pageContract()
    {
        ClipboardController controller(nullptr, false);
        populate(controller);
        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("clipboardController"), &controller);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const int id = QFontDatabase::addApplicationFont(QStringLiteral(CLIPBOARD_QML_DIR)
                + "/../assets/fonts/HarmonyOS_Sans_" + weight + ".ttf");
            engine.rootContext()->setContextProperty(QString("harmony%1").arg(weight),
                QVariantMap{{"name", QFontDatabase::applicationFontFamilies(id).constFirst()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(CLIPBOARD_QML_DIR) + "/ClipboardPage.qml"));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto *page = qobject_cast<QQuickItem *>(root.get()); QVERIFY(page);
        QCOMPARE(page->size(), QSizeF(590, 510));
        QQuickWindow window; window.resize(900, 650); window.setColor(QColor("#202020"));
        page->setParentItem(window.contentItem()); page->setPosition({100, 45}); page->setScale(1.12);
        window.show(); root->setProperty("reduceMotion", true); root->setProperty("presented", true);
        auto *rows = findItem(page, QStringLiteral("clipboardRows")); QVERIFY(rows);
        QCOMPARE(rows->property("count").toInt(), 4);
        QTest::qWait(100);
        if (qEnvironmentVariableIsSet("HYPR_CLIPBOARD_RENDER"))
            QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-clipboard.png")));
        QVERIFY(QMetaObject::invokeMethod(root.get(), "selectFilter", Q_ARG(QVariant, 1)));
        QCOMPARE(root->property("selectedFilter").toInt(), 1);
        QCOMPARE(rows->property("count").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(root.get(), "askClear"));
        auto *confirm = findItem(page, QStringLiteral("clipboardConfirm")); QVERIFY(confirm); QVERIFY(confirm->isVisible());
        if (qEnvironmentVariableIsSet("HYPR_CLIPBOARD_RENDER"))
            QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-clipboard-confirm.png")));
        if (!warnings.isEmpty()) for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty());
        root->setProperty("presented", false); page->setParentItem(nullptr);
    }
};

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsSet("HYPR_CLIPBOARD_RENDER")) {
        qputenv("QT_QUICK_BACKEND", QByteArrayLiteral("rhi"));
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        QSurfaceFormat format; format.setSamples(4); QSurfaceFormat::setDefaultFormat(format);
    }
    QGuiApplication application(argc, argv);
    ClipboardTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "clipboard.moc"
