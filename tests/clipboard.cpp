#include "clipboardcontroller.hpp"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
QByteArray snapshot()
{
    const QJsonArray items{
        QJsonObject{{"id", "text-id"}, {"row", 0}, {"kind", "text"}, {"preview", "一段文字"},
                    {"characters", 4}, {"imageMime", ""}, {"fileUris", QJsonArray{}},
                    {"createdAt", "2026-11-07T09:08:07.000+08:00"}, {"pinned", false}},
        QJsonObject{{"id", "link-id"}, {"row", 1}, {"kind", "url"}, {"preview", "https://example.com/docs"},
                    {"characters", 0}, {"imageMime", ""}, {"fileUris", QJsonArray{}},
                    {"createdAt", ""}, {"pinned", true}},
        QJsonObject{{"id", "image-id"}, {"row", 2}, {"kind", "image"}, {"preview", "图片"},
                    {"characters", 0}, {"imageMime", "image/png"}, {"fileUris", QJsonArray{}},
                    {"createdAt", ""}, {"pinned", false}},
        QJsonObject{{"id", "file-id"}, {"row", 3}, {"kind", "files"}, {"preview", "Document.pdf"},
                    {"characters", 0}, {"imageMime", ""}, {"fileUris", QJsonArray{"file:///tmp/Document.pdf"}},
                    {"createdAt", ""}, {"pinned", false}}
    };
    return QJsonDocument(QJsonObject{{"total", 4}, {"capacity", 20000}, {"removable", 3}, {"items", items}})
        .toJson(QJsonDocument::Compact);
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
    void parsesCopyQSnapshot()
    {
        ClipboardController controller(nullptr, QStringLiteral("/bin/false"), false);
        QVERIFY(controller.acceptSnapshot(snapshot()));
        const QVariantMap state = controller.state();
        QVERIFY(state.value("ready").toBool());
        QCOMPARE(state.value("total").toInt(), 4);
        QCOMPARE(state.value("capacity").toInt(), 20000);
        QCOMPARE(state.value("removable").toInt(), 3);
        const QVariantList items = state.value("items").toList();
        QCOMPARE(items.size(), 4);
        QCOMPARE(items[0].toMap().value("meta").toString(), QStringLiteral("4 字符"));
        QVERIFY(items[0].toMap().value("createdAt").toLongLong() > 0);
        QCOMPARE(items[1].toMap().value("meta").toString(), QStringLiteral("example.com"));
        QCOMPARE(items[2].toMap().value("format").toString(), QStringLiteral("PNG"));
        QCOMPARE(items[3].toMap().value("fileCount").toInt(), 1);
        QVERIFY(!controller.acceptSnapshot("{}"));
    }

    void pageContract()
    {
        ClipboardController controller(nullptr, QStringLiteral("/bin/false"), false);
        QVERIFY(controller.acceptSnapshot(snapshot()));
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
        root->setProperty("reduceMotion", true);
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
