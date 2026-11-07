#include "terminalsession.hpp"
#include "terminalview.hpp"
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class TerminalTests final : public QObject
{
    Q_OBJECT
private:
    static QByteArray output(const QSignalSpy &spy) {
        QByteArray bytes;
        for (const auto &entry : spy) bytes += entry.at(0).toByteArray();
        return bytes;
    }
    static void startBash(TerminalSession &session, const QString &directory) {
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("PS1", "test-prompt> "); env.insert("HISTFILE", "/dev/null");
        session.start("/usr/bin/bash", {"--noprofile", "--norc", "-i"}, directory, env);
    }
    static void command(TerminalSession &session, const QString &text) {
        session.paste(text); session.key(VTERM_KEY_ENTER, VTERM_MOD_NONE);
    }
private slots:
    void parserAndAlternateScreen() {
        TerminalSession session;
        session.resize(5, 20);
        session.receive("\x1b[38;2;120;200;35mA\x1b[0m\xe4\xb8\xad" "e\xcc\x81");
        const auto first = session.cell(0, 0); const auto rgb = session.rgb(first.fg);
        QCOMPARE(first.chars[0], uint32_t('A'));
        QCOMPARE(rgb.rgb.red, 120); QCOMPARE(rgb.rgb.green, 200); QCOMPARE(rgb.rgb.blue, 35);
        QCOMPARE(session.cell(0, 1).width, char(2));
        QCOMPARE(session.cell(0, 3).chars[1], uint32_t(0x301));
        session.receive("\x1b[?1049h\x1b[2J\x1b[Halternate");
        QVERIFY(session.alternateScreen()); QVERIFY(session.screenText().contains("alternate"));
        session.receive("\x1b[?1049l");
        QVERIFY(!session.alternateScreen()); QVERIFY(session.screenText().contains(QString::fromUtf8("A中é")));
        session.receive("\r\x1b[2Kreplacement");
        QVERIFY(session.screenText().startsWith("replacement"));
        session.receive("\x1b[?25l"); QVERIFY(!session.cursorVisible());
        session.receive("\x1b[?25h"); QVERIFY(session.cursorVisible());
    }
    void scrollbackAndResize() {
        TerminalSession session; session.resize(4, 10);
        for (int i = 0; i < 2100; ++i) session.receive(QByteArray::number(i) + "\r\n");
        QCOMPARE(session.historySize(), 2000);
        QVERIFY(session.screenText().contains("2099"));
        session.resize(8, 20); QCOMPARE(session.rows(), 8); QCOMPARE(session.columns(), 20);
        QVERIFY(session.screenText().contains("2099"));
        session.receive("\x1b[3J"); QCOMPARE(session.historySize(), 0);
    }
    void inputSequences() {
        TerminalSession session; QSignalSpy spy(&session, &TerminalSession::outputWritten);
        session.key(VTERM_KEY_ESCAPE, VTERM_MOD_NONE);
        session.character(U'c', VTERM_MOD_CTRL);
        session.key(VTERM_KEY_UP, VTERM_MOD_NONE);
        QCOMPARE(output(spy), QByteArray("\x1b\x03\x1b[A")); spy.clear();
        session.receive("\x1b[?1h"); session.key(VTERM_KEY_UP, VTERM_MOD_NONE);
        QCOMPARE(output(spy), QByteArray("\x1bOA")); spy.clear();
        session.receive("\x1b[?2004h"); session.paste(QString::fromUtf8("中文\nnext"));
        QCOMPARE(output(spy), QByteArray("\x1b[200~") + QString::fromUtf8("中文\rnext").toUtf8() + "\x1b[201~");
    }
    void realPty() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        TerminalSession session; startBash(session, directory.path());
        QTRY_VERIFY_WITH_TIMEOUT(session.screenText().contains("test-prompt>"), 5000);
        QVERIFY(session.running());
        command(session, "printf 'PTY_%s\\n' READY; pwd");
        QTRY_VERIFY(session.screenText().contains("PTY_READY"));
        QVERIFY(session.screenText().contains(directory.path()));
        session.resize(31, 97); command(session, "stty size");
        QTRY_VERIFY(session.screenText().contains("31 97"));
        command(session, "sleep 20"); QTest::qWait(80);
        session.character(U'c', VTERM_MOD_CTRL); command(session, "printf 'AFTER_%s\\n' INTERRUPT");
        QTRY_VERIFY(session.screenText().contains("AFTER_INTERRUPT"));
        command(session, "stty -echo -icanon min 1 time 0; printf 'READ_%s\\n' ESCAPE; od -An -tu1 -N1; stty sane");
        QTRY_VERIFY(session.screenText().contains("READ_ESCAPE"));
        session.key(VTERM_KEY_ESCAPE, VTERM_MOD_NONE);
        QTRY_VERIFY(session.screenText().contains(" 27"));
        command(session, "exit"); QTRY_VERIFY(!session.running());
        QVERIFY(session.status().contains(QStringLiteral("会话已结束")));
    }
    void pageInputAndLifetime() {
        TerminalSession session;
        QTemporaryDir directory; startBash(session, directory.path());
        QTRY_VERIFY(session.screenText().contains("test-prompt>"));
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("terminalSession", &session);
        for (const auto &weight : {"Regular", "Medium", "Bold"}) {
            const auto path = QFileInfo(QStringLiteral(TERMINAL_PAGE_PATH)).dir().filePath(QString("../assets/fonts/HarmonyOS_Sans_%1.ttf").arg(weight));
            const int id = QFontDatabase::addApplicationFont(path);
            engine.rootContext()->setContextProperty(QString("harmony%1").arg(weight), QVariantMap{{"name", QFontDatabase::applicationFontFamilies(id).first()}});
        }
        QSignalSpy warnings(&engine, &QQmlEngine::warnings);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(TERMINAL_PAGE_PATH)));
        QQuickWindow window; window.resize(960, 740); window.setColor(QColor("#222222"));
        std::unique_ptr<QObject> root(component.create());
        QVERIFY2(root != nullptr, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(root.get()); QVERIFY(item);
        QCOMPARE(item->size(), QSizeF(590, 510));
        item->setParentItem(window.contentItem()); item->setPosition({50, 10});
        item->setTransformOrigin(QQuickItem::TopLeft); item->setScale(1.4);
        auto *view = root->findChild<TerminalView *>("terminalView"); QVERIFY(view);
        window.show(); window.requestActivate();
        root->setProperty("presented", true); QTRY_VERIFY(view->hasActiveFocus());
        QSignalSpy spy(&session, &TerminalSession::outputWritten);
        QTest::keyClick(&window, Qt::Key_Escape);
        QCOMPARE(output(spy), QByteArray("\x1b")); QVERIFY(root->property("presented").toBool()); spy.clear();
        QTest::keyClick(&window, Qt::Key_C, Qt::ControlModifier);
        QTest::qWait(100); // Bash must finish handling SIGINT before more input.
        QVERIFY(output(spy).contains('\x03')); spy.clear();
        QInputMethodEvent input; input.setCommitString(QStringLiteral("中文输入"));
        QCoreApplication::sendEvent(view, &input);
        QCOMPARE(output(spy), QStringLiteral("中文输入").toUtf8()); spy.clear();
        QTest::keyClick(&window, Qt::Key_C, Qt::ControlModifier);
        QTest::qWait(100);
        command(session, "printf 'HIDDEN_%s\\n' ALIVE");
        root->setProperty("presented", false);
        QVERIFY(!view->hasActiveFocus()); QVERIFY(session.running());
        QTRY_VERIFY2(session.screenText().contains("HIDDEN_ALIVE"), qPrintable(session.screenText()));
        root->setProperty("presented", true); QTRY_VERIFY(view->hasActiveFocus());
        session.receive("\x1b[2J\x1b[H\x1b[38;2;255;250;0mHyprEndfield\x1b[0m  /  快捷终端\r\n\r\n$ printf 'Hello, Endfield!\\n'\r\nHello, Endfield!\r\n\r\n\x1b[32mBash · UTF-8 · truecolor · PTY\x1b[0m\r\n$ ");
        QTest::qWait(100);
        if (qEnvironmentVariableIsSet("HYPR_TERMINAL_RENDER")) {
            const auto image = window.grabWindow(); QVERIFY(!image.isNull());
            QVERIFY(image.save("/tmp/hyprendfield-terminal.png"));
        }
        QFont terminalFont("JetBrainsMono Nerd Font Mono"); terminalFont.setPixelSize(13);
        const qreal cellWidth = QFontMetricsF(terminalFont).horizontalAdvance('M');
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, view->mapToScene({1, 3}).toPoint());
        QTest::mouseMove(&window, view->mapToScene({11.5 * cellWidth, 3}).toPoint());
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, view->mapToScene({11.5 * cellWidth, 3}).toPoint());
        QTest::keyClick(&window, Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("HyprEndfield"));
        QGuiApplication::clipboard()->setText("paste_marker"); spy.clear();
        QTest::keyClick(&window, Qt::Key_V, Qt::ControlModifier | Qt::ShiftModifier);
        QVERIFY(output(spy).contains("paste_marker"));
        QVERIFY(warnings.isEmpty());
        item->setParentItem(nullptr);
    }
    void neovimEscape() {
        const QString nvim = QStandardPaths::findExecutable("nvim");
        if (nvim.isEmpty()) QSKIP("Neovim is optional; parser alternate-screen tests still run.");
        QTemporaryDir directory;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("XDG_STATE_HOME", directory.path()); env.insert("XDG_CACHE_HOME", directory.path());
        TerminalSession session;
        session.start(nvim, {"--clean", "-i", "NONE", "-n"}, directory.path(), env);
        QTRY_VERIFY(session.alternateScreen());
        session.character(U'i', VTERM_MOD_NONE);
        for (char32_t character : QStringLiteral("中文 Terminal").toUcs4()) session.character(character, VTERM_MOD_NONE);
        QTRY_VERIFY(session.screenText().contains(QStringLiteral("中文 Terminal")));
        session.key(VTERM_KEY_ESCAPE, VTERM_MOD_NONE);
        for (char32_t character : QStringLiteral(":q!").toUcs4()) session.character(character, VTERM_MOD_NONE);
        session.key(VTERM_KEY_ENTER, VTERM_MOD_NONE);
        QTRY_VERIFY(!session.running());
        QVERIFY(!session.alternateScreen());
    }
};

int main(int argc, char **argv)
{
    if (qEnvironmentVariableIsSet("HYPR_TERMINAL_RENDER")) {
        qputenv("QT_QUICK_BACKEND", "rhi");
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        QSurfaceFormat format; format.setSamples(4); QSurfaceFormat::setDefaultFormat(format);
    }
    QGuiApplication app(argc, argv);
    qmlRegisterType<TerminalView>("HyprEndfield.Terminal", 1, 0, "TerminalView");
    TerminalTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "terminal.moc"
