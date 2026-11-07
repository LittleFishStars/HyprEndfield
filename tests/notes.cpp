#include "notescontroller.hpp"

#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems())
        if (auto *found = findItem(child, name)) return found;
    return nullptr;
}

QVariantMap noteByKind(const QVariantMap &state, const QString &kind)
{
    for (const auto &value : state.value(QStringLiteral("notes")).toList()) {
        const QVariantMap note = value.toMap();
        if (note.value(QStringLiteral("kind")).toString() == kind) return note;
    }
    return {};
}

void installFonts(QQmlEngine &engine)
{
    for (const auto &weight : {"Regular", "Medium", "Bold"}) {
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(NOTES_QML_DIR)
            + QStringLiteral("/../assets/fonts/HarmonyOS_Sans_") + weight + QStringLiteral(".ttf"));
        QVERIFY(id >= 0);
        engine.rootContext()->setContextProperty(QStringLiteral("harmony") + weight,
            QVariantMap{{QStringLiteral("name"), QFontDatabase::applicationFontFamilies(id).constFirst()}});
    }
}
}

class NotesTests final : public QObject
{
    Q_OBJECT

private slots:
    void persistenceAndEditing()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        NotesController controller(nullptr, directory.path());
        QVERIFY(controller.state().value(QStringLiteral("ready")).toBool());
        QCOMPARE(controller.state().value(QStringLiteral("total")).toInt(), 0);

        QVERIFY(controller.setCentreTab(QStringLiteral("text")));
        QVERIFY(controller.setCentreFolded(true));
        QVERIFY(controller.setCentreText(QStringLiteral("中央便笺\n第二行")));
        QVERIFY(controller.setCentreStyle(QStringLiteral("size"), 20));
        QVERIFY(controller.setCentreStyle(QStringLiteral("bold"), true));
        QVERIFY(controller.setCentreStyle(QStringLiteral("color"), QStringLiteral("#22A6F2")));
        QVERIFY(controller.addCentreTask(QStringLiteral("第一项")));
        QVERIFY(controller.addCentreTask(QStringLiteral("第二项")));
        QVariantList centreTasks = controller.state().value(QStringLiteral("centre")).toMap()
            .value(QStringLiteral("tasks")).toList();
        QCOMPARE(centreTasks.size(), 2);
        const QString firstTask = centreTasks[0].toMap().value(QStringLiteral("id")).toString();
        QVERIFY(controller.toggleCentreTask(firstTask));
        QVERIFY(controller.toggleCentreFlag(firstTask));
        QVERIFY(controller.moveCentreTask(firstTask, 1));
        QVERIFY(controller.updateCentreTask(firstTask, QStringLiteral("已经修改")));

        const QString textId = controller.createNote(QStringLiteral("text"), 10'000, -40);
        QVERIFY(!textId.isEmpty());
        QVERIFY(controller.updateNoteText(textId, QStringLiteral("桌面文字便笺")));
        QVERIFY(controller.updateNoteGeometry(textId, 80, 90, 260, 180));
        QVERIFY(controller.toggleNotePin(textId));

        const QString todoId = controller.createNote(QStringLiteral("todo"), 700, 300);
        QVERIFY(!todoId.isEmpty());
        QVERIFY(controller.addNoteTask(todoId, QStringLiteral("检查终端")));
        QVariantMap todo = noteByKind(controller.state(), QStringLiteral("todo"));
        const QVariantList todoTasks = todo.value(QStringLiteral("tasks")).toList();
        QCOMPARE(todoTasks.size(), 1);
        const QString todoTaskId = todoTasks.constFirst().toMap().value(QStringLiteral("id")).toString();
        QVERIFY(controller.toggleNoteTask(todoId, todoTaskId));
        QVERIFY(controller.updateNoteTask(todoId, todoTaskId, QStringLiteral("检查便笺")));

        const QString drawingId = controller.createNote(QStringLiteral("drawing"), 900, 500);
        QVERIFY(!drawingId.isEmpty());
        const QVariantList points{QVariantMap{{"x", 0.1}, {"y", 0.2}},
                                  QVariantMap{{"x", 0.5}, {"y", 0.6}},
                                  QVariantMap{{"x", 0.8}, {"y", 0.3}}};
        QVERIFY(controller.appendStroke(drawingId, points, 8, QStringLiteral("#FFFA00")));
        QCOMPARE(noteByKind(controller.state(), QStringLiteral("drawing"))
            .value(QStringLiteral("strokes")).toList().size(), 1);

        NotesController reloaded(nullptr, directory.path());
        const QVariantMap state = reloaded.state();
        QCOMPARE(state.value(QStringLiteral("total")).toInt(), 3);
        const QVariantMap centre = state.value(QStringLiteral("centre")).toMap();
        QCOMPARE(centre.value(QStringLiteral("tab")).toString(), QStringLiteral("text"));
        QVERIFY(centre.value(QStringLiteral("folded")).toBool());
        QCOMPARE(centre.value(QStringLiteral("text")).toString(), QStringLiteral("中央便笺\n第二行"));
        QCOMPARE(centre.value(QStringLiteral("style")).toMap().value(QStringLiteral("size")).toDouble(), 20.0);
        const QVariantMap text = noteByKind(state, QStringLiteral("text"));
        QVERIFY(text.value(QStringLiteral("pinned")).toBool());
        QCOMPARE(text.value(QStringLiteral("x")).toDouble(), 80.0);
        QCOMPARE(text.value(QStringLiteral("text")).toString(), QStringLiteral("桌面文字便笺"));

        QVERIFY(reloaded.eraseDrawingPath(drawingId,
            QVariantList{QVariantMap{{"x", 0.51}, {"y", 0.59}}}, 0.05));
        QCOMPARE(noteByKind(reloaded.state(), QStringLiteral("drawing"))
            .value(QStringLiteral("strokes")).toList().size(), 0);
        QVERIFY(reloaded.removeNote(todoId));
        QCOMPARE(reloaded.state().value(QStringLiteral("total")).toInt(), 2);
    }

    void imageImportAndExternalChange()
    {
        QTemporaryDir directory;
        QTemporaryDir source;
        QImage image(640, 360, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(QStringLiteral("#FFFA00")));
        const QString sourcePath = source.path() + QStringLiteral("/sample.png");
        QVERIFY(image.save(sourcePath));

        NotesController controller(nullptr, directory.path());
        const QString id = controller.importImage(QUrl::fromLocalFile(sourcePath), 200, 250);
        QVERIFY(!id.isEmpty());
        const QVariantMap imported = noteByKind(controller.state(), QStringLiteral("image"));
        QVERIFY(imported.value(QStringLiteral("imageAvailable")).toBool());
        QVERIFY(imported.value(QStringLiteral("imageUrl")).toString().startsWith(QStringLiteral("file:")));
        QVERIFY(imported.value(QStringLiteral("width")).toDouble() > 100);

        NotesController reloaded(nullptr, directory.path());
        QCOMPARE(reloaded.state().value(QStringLiteral("total")).toInt(), 1);
        QFile file(directory.path() + QStringLiteral("/notes.json"));
        QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write(" "), qint64(1));
        file.close();
        QVERIFY(!reloaded.setCentreText(QStringLiteral("不得覆盖外部修改")));
        QVERIFY(reloaded.state().value(QStringLiteral("error")).toString().contains(QStringLiteral("其他程序")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().endsWith(' '));
    }

    void pageAndPinnedWorkspace()
    {
        QTemporaryDir directory;
        NotesController controller(nullptr, directory.path());
        QVERIFY(controller.addCentreTask(QStringLiteral("测试待办")));
        const QString unpinned = controller.createNote(QStringLiteral("text"), 500, 350);
        const QString pinned = controller.createNote(QStringLiteral("drawing"), 900, 420);
        QVERIFY(controller.updateNoteText(unpinned, QStringLiteral("桌面文字便笺\n双击内容即可编辑。")));
        QVERIFY(controller.appendStroke(pinned,
            QVariantList{QVariantMap{{"x", 0.08}, {"y", 0.75}},
                         QVariantMap{{"x", 0.32}, {"y", 0.20}},
                         QVariantMap{{"x", 0.65}, {"y", 0.72}},
                         QVariantMap{{"x", 0.92}, {"y", 0.26}}},
            8, QStringLiteral("#FFFA00")));
        QVERIFY(controller.toggleNotePin(pinned));

        QQmlEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("notesController"), &controller);
        installFonts(engine);
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);

        QQmlComponent pageComponent(&engine, QUrl::fromLocalFile(QStringLiteral(NOTES_QML_DIR) + QStringLiteral("/NotesPage.qml")));
        std::unique_ptr<QObject> pageObject(pageComponent.create());
        QVERIFY2(pageObject != nullptr, qPrintable(pageComponent.errorString()));
        auto *page = qobject_cast<QQuickItem *>(pageObject.get());
        QVERIFY(page);
        QCOMPARE(page->size(), QSizeF(590, 510));
        page->setProperty("reduceMotion", true);
        page->setProperty("presented", true);
        QVERIFY(findItem(page, QStringLiteral("notesCentreTasks")));
        auto *addBar = findItem(page, QStringLiteral("notesAddBar"));
        QVERIFY(addBar);
        QVERIFY(addBar->y() > 80);
        QVERIFY(QMetaObject::invokeMethod(pageObject.get(), "selectTab", Q_ARG(QVariant, QStringLiteral("text"))));
        QCOMPARE(controller.state().value(QStringLiteral("centre")).toMap().value(QStringLiteral("tab")).toString(), QStringLiteral("text"));

        QQmlComponent workspaceComponent(&engine, QUrl::fromLocalFile(QStringLiteral(NOTES_QML_DIR) + QStringLiteral("/NotesWorkspace.qml")));
        std::unique_ptr<QObject> workspaceObject(workspaceComponent.create());
        QVERIFY2(workspaceObject != nullptr, qPrintable(workspaceComponent.errorString()));
        auto *workspace = qobject_cast<QQuickItem *>(workspaceObject.get());
        QVERIFY(workspace);
        QCOMPARE(workspace->size(), QSizeF(1920, 1080));
        workspace->setProperty("reduceMotion", true);
        workspace->setProperty("presented", true);
        workspace->setProperty("selectedModule", QStringLiteral("calendar"));
        QCoreApplication::processEvents();
        auto *pinnedCard = findItem(workspace, QStringLiteral("noteCard-") + pinned);
        auto *normalCard = findItem(workspace, QStringLiteral("noteCard-") + unpinned);
        QVERIFY(pinnedCard && normalCard);
        QVERIFY(pinnedCard->isVisible());
        QVERIFY(!normalCard->isVisible());
        workspace->setProperty("selectedModule", QStringLiteral("notes"));
        QCoreApplication::processEvents();
        QVERIFY(normalCard->isVisible());

        if (qEnvironmentVariableIsSet("HYPR_NOTES_RENDER")) {
            QQuickWindow window;
            window.resize(1100, 760);
            page->setParentItem(window.contentItem());
            page->setPosition({80, 60});
            page->setScale(1.12);
            window.setColor(QColor(QStringLiteral("#202020")));
            window.show();
            QTest::qWait(120);
            QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-notes.png")));
            QVERIFY(QMetaObject::invokeMethod(pageObject.get(), "selectTab", Q_ARG(QVariant, QStringLiteral("todo"))));
            QTest::qWait(60);
            QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-notes-todo.png")));
            page->setParentItem(nullptr);
            workspace->setParentItem(window.contentItem());
            workspace->setScale(0.55);
            workspace->setPosition({20, 20});
            QTest::qWait(100);
            QVERIFY(window.grabWindow().save(QStringLiteral("/tmp/hyprendfield-notes-workspace.png")));
            workspace->setParentItem(nullptr);
        }

        if (!warnings.isEmpty()) for (const auto &warning : warnings) qWarning() << warning;
        QVERIFY(warnings.isEmpty());
    }
};

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    NotesTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "notes.moc"
