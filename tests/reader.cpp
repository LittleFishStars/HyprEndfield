#include "fileshelfcontroller.hpp"
#include "readercontroller.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPainter>
#include <QPdfWriter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <zip.h>

namespace {
void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    QCOMPARE(file.write(bytes), bytes.size());
}

void addZipFile(zip_t *archive, const QByteArray &name, const QByteArray &bytes, bool stored = false)
{
    void *copy = std::malloc(size_t(std::max<qsizetype>(1, bytes.size())));
    QVERIFY(copy);
    if (!bytes.isEmpty()) std::memcpy(copy, bytes.constData(), size_t(bytes.size()));
    zip_source_t *source = zip_source_buffer(archive, copy, zip_uint64_t(bytes.size()), 1);
    QVERIFY(source);
    const auto index = zip_file_add(archive, name.constData(), source, ZIP_FL_ENC_UTF_8 | ZIP_FL_OVERWRITE);
    QVERIFY(index >= 0);
    if (stored) QCOMPARE(zip_set_file_compression(archive, zip_uint64_t(index), ZIP_CM_STORE, 0), 0);
}

QString makeEpub(const QString &directory)
{
    const QString path = directory + QStringLiteral("/fixture.epub");
    int error = 0;
    zip_t *archive = zip_open(QFile::encodeName(path).constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (!archive) qFatal("Unable to create EPUB fixture");
    addZipFile(archive, "mimetype", "application/epub+zip", true);
    addZipFile(archive, "META-INF/container.xml", QByteArrayLiteral("<?xml version=\"1.0\"?>"
        "<container><rootfiles><rootfile full-path=\"OEBPS/content.opf\"/></rootfiles></container>"));
    addZipFile(archive, "OEBPS/content.opf", QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<package xmlns=\"http://www.idpf.org/2007/opf\"><metadata><title>测试 EPUB</title></metadata>"
        "<manifest><item id=\"chapter\" href=\"chapter.xhtml\" media-type=\"application/xhtml+xml\"/></manifest>"
        "<spine><itemref idref=\"chapter\"/></spine></package>"));
    QByteArray chapter = QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?><html><body><h1>章节</h1>");
    for (int index = 0; index < 180; ++index) chapter += "<p>这是 EPUB 阅读器的分页测试文字。Endfield Industries.</p>";
    chapter += "</body></html>";
    addZipFile(archive, "OEBPS/chapter.xhtml", chapter);
    if (zip_close(archive) != 0) qFatal("Unable to close EPUB fixture");
    return path;
}

QString makePdf(const QString &directory)
{
    const QString path = directory + QStringLiteral("/fixture.pdf");
    QPdfWriter writer(path);
    writer.setTitle(QStringLiteral("测试 PDF"));
    writer.setPageSize(QPageSize(QPageSize::A5));
    QPainter painter(&writer);
    if (!painter.isActive()) qFatal("Unable to create PDF fixture");
    painter.setPen(Qt::black);
    painter.setFont(QFont(QStringLiteral("Sans Serif"), 24));
    painter.drawText(QRect(0, 0, writer.width(), writer.height()), Qt::AlignCenter, QStringLiteral("第一页"));
    writer.newPage();
    painter.drawText(QRect(0, 0, writer.width(), writer.height()), Qt::AlignCenter, QStringLiteral("第二页"));
    painter.end();
    return path;
}
}

class ReaderTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_directory.isValid());
        m_textPath = m_directory.path() + QStringLiteral("/fixture.txt");
        QString text;
        for (int index = 0; index < 450; ++index)
            text += QStringLiteral("第 %1 段：阅读器需要保留进度、分页和中文文字。\n").arg(index + 1);
        writeFile(m_textPath, text.toUtf8());
        m_pdfPath = makePdf(m_directory.path());
        m_epubPath = makeEpub(m_directory.path());
        m_provider = new ReaderImageProvider;
        m_controller = std::make_unique<ReaderController>(nullptr, m_provider, m_directory.path() + QStringLiteral("/reader"));
        m_controller->setActive(true);
    }

    void cleanupTestCase()
    {
        m_controller->setActive(false);
        m_controller.reset();
        delete m_provider;
        m_provider = nullptr;
    }

    void txtImportReadingAndPersistence()
    {
        QSignalSpy actions(m_controller.get(), &ReaderController::actionFinished);
        QVERIFY(m_controller->importUrl(QUrl::fromLocalFile(m_textPath)));
        QTRY_VERIFY_WITH_TIMEOUT(!m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("key")).toString().isEmpty(), 15000);
        QCOMPARE(m_controller->state().value(QStringLiteral("books")).toList().size(), 1);
        QCOMPARE(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("format")).toString(), QStringLiteral("TXT"));
        const QString first = m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("key")).toString();
        m_controller->turn(1);
        QTRY_VERIFY_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("key")).toString() != first, 15000);
        m_controller->toggleBookmark();
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("bookmarks")).toList().size(), 1, 5000);
        m_controller->setPreference(QStringLiteral("fontSize"), 12);
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("preferences")).toMap().value(QStringLiteral("fontSize")).toDouble(), 12.0, 5000);
        QVERIFY(!actions.isEmpty());

        m_controller->setActive(false);
        ReaderImageProvider secondProvider;
        ReaderController second(nullptr, &secondProvider, m_directory.path() + QStringLiteral("/reader"));
        QCOMPARE(second.state().value(QStringLiteral("books")).toList().size(), 1);
        QCOMPARE(second.state().value(QStringLiteral("preferences")).toMap().value(QStringLiteral("fontSize")).toDouble(), 12.0);
        m_controller->setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(!m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("key")).toString().isEmpty(), 15000);
        QCOMPARE(m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("illustration")).toBool(), false);
        m_controller->zoomAt(2, 294, 174.5);
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("zoom")).toDouble(), 2.0, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("detailReady")).toBool(), true, 15000);
        const QString source = m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("source")).toString();
        const QString key = source.mid(QStringLiteral("image://reader-pages/").size());
        const QImage zoomPreview = m_provider->requestImage(key, nullptr, {});
        QVERIFY(!zoomPreview.isNull());
        QVERIFY(zoomPreview.save(QStringLiteral(READER_ZOOM_PREVIEW_PATH)));
        m_controller->panBy(20, -10);
        QTRY_VERIFY_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("panX")).toDouble() > 0, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("panY")).toDouble() < 0, 5000);
        m_controller->resetZoom();
        QCOMPARE(m_controller->state().value(QStringLiteral("zoom")).toDouble(), 1.0);
    }

    void pdfAndEpubImport()
    {
        QTRY_VERIFY_WITH_TIMEOUT(!m_controller->state().value(QStringLiteral("busy")).toBool(), 15000);
        QVERIFY(m_controller->importUrl(QUrl::fromLocalFile(m_pdfPath)));
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("format")).toString(), QStringLiteral("PDF"), 15000);
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("total")).toInt(), 2, 15000);
        QCOMPARE(m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("illustration")).toBool(), true);
        m_controller->setImageView(2, 10, -5);
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("zoom")).toDouble(), 2.0, 5000);
        QTest::qWait(300);

        QVERIFY(m_controller->importUrl(QUrl::fromLocalFile(m_epubPath)));
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("format")).toString(), QStringLiteral("EPUB"), 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("key")).toString().isEmpty(), 15000);
        QCOMPARE(m_controller->state().value(QStringLiteral("books")).toList().size(), 3);
    }

    void missingRelinkAndRemoveReference()
    {
        const QString epubId = m_controller->state().value(QStringLiteral("selected")).toString();
        const QString original = m_directory.path() + QStringLiteral("/lifecycle.txt");
        const QString replacement = m_directory.path() + QStringLiteral("/lifecycle-relinked.txt");
        writeFile(original, QByteArrayLiteral("Temporary reader reference.\nSecond line."));
        writeFile(replacement, QByteArrayLiteral("Relinked reader reference.\nSecond line."));

        QVERIFY(m_controller->importUrl(QUrl::fromLocalFile(original)));
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("books")).toList().size(), 4, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_controller->state().value(QStringLiteral("current")).toMap().value(QStringLiteral("key")).toString().isEmpty(), 15000);
        const QString id = m_controller->state().value(QStringLiteral("selected")).toString();
        m_controller->toggleBookmark();
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("bookmarks")).toList().size(), 1, 5000);

        QVERIFY(QFile::remove(original));
        m_controller->setActive(false);
        m_controller->setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(!m_controller->state().value(QStringLiteral("busy")).toBool(), 15000);
        QVERIFY(m_controller->state().value(QStringLiteral("current")).toMap().isEmpty());
        QCOMPARE(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("available")).toBool(), false);

        QVERIFY(m_controller->relinkBook(id, QUrl::fromLocalFile(replacement)));
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("path")).toString(), replacement, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("available")).toBool(), true, 15000);
        QCOMPARE(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("bookmarks")).toList().size(), 1);
        QVERIFY(m_controller->removeBook(id));
        QCOMPARE(m_controller->state().value(QStringLiteral("books")).toList().size(), 3);
        QVERIFY(QFileInfo::exists(replacement));
        QVERIFY(m_controller->selectBook(epubId));
        QTRY_COMPARE_WITH_TIMEOUT(m_controller->state().value(QStringLiteral("book")).toMap().value(QStringLiteral("format")).toString(), QStringLiteral("EPUB"), 15000);
    }

    void qmlPageLoadsAndRenders()
    {
        m_controller->setTab(QStringLiteral("library"));
        FileShelfController shelf(nullptr, m_directory.path() + QStringLiteral("/shelf"));
        auto *visualProvider = new ReaderImageProvider;
        ReaderController visualController(nullptr, visualProvider, m_directory.path() + QStringLiteral("/reader"));
        QQuickView view;
        view.setColor(QColor(QStringLiteral("#151515")));
        view.setResizeMode(QQuickView::SizeRootObjectToView);
        view.resize(590, 510);
        view.rootContext()->setContextProperty(QStringLiteral("readerController"), &visualController);
        view.rootContext()->setContextProperty(QStringLiteral("fileShelfController"), &shelf);
        const QVariantMap regular{{QStringLiteral("name"), QStringLiteral("HarmonyOS Sans")}};
        view.rootContext()->setContextProperty(QStringLiteral("harmonyRegular"), regular);
        view.rootContext()->setContextProperty(QStringLiteral("harmonyMedium"), regular);
        view.rootContext()->setContextProperty(QStringLiteral("harmonyBold"), regular);
        view.engine()->addImageProvider(QStringLiteral("reader-pages"), visualProvider);
        view.setSource(QUrl::fromLocalFile(QStringLiteral(READER_QML_DIR) + QStringLiteral("/ReaderPage.qml")));
        QVERIFY2(view.status() == QQuickView::Ready, qPrintable(view.errors().isEmpty() ? QStringLiteral("ReaderPage failed") : view.errors().constFirst().toString()));
        QVERIFY(view.rootObject()->findChild<QObject *>(QStringLiteral("readerTabs")));
        QVERIFY(view.rootObject()->findChild<QObject *>(QStringLiteral("readerLibrary")));
        QVERIFY(view.rootObject()->findChild<QObject *>(QStringLiteral("readerPaper")));
        QVERIFY(view.rootObject()->findChild<QObject *>(QStringLiteral("readerResetZoom")));
        QVERIFY(view.rootObject()->findChild<QObject *>(QStringLiteral("readerSettings")));
        QVERIFY(view.rootObject()->findChild<QObject *>(QStringLiteral("readerMarks")));
        view.rootObject()->setProperty("presented", true);
        view.show();
        QTest::qWait(250);
        const QImage preview = view.grabWindow();
        QVERIFY(!preview.isNull());
        QVERIFY(preview.save(QStringLiteral(READER_PREVIEW_PATH)));

        view.rootObject()->setProperty("tab", 2);
        visualController.setTab(QStringLiteral("reading"));
        QTRY_VERIFY_WITH_TIMEOUT(!visualController.state().value(QStringLiteral("current")).toMap().value(QStringLiteral("key")).toString().isEmpty(), 15000);
        QTest::qWait(500);
        auto *verticalColumn = view.rootObject()->findChild<QQuickItem *>(QStringLiteral("readerVerticalColumn"));
        QVERIFY(verticalColumn);
        QVERIFY(verticalColumn->isVisible());
        QTest::wheelEvent(&view, QPointF(294, 295), QPoint(0, 120), QPoint(), Qt::ControlModifier);
        QTRY_VERIFY_WITH_TIMEOUT(visualController.state().value(QStringLiteral("zoom")).toDouble() > 1, 5000);
        visualController.resetZoom();
        QTRY_VERIFY_WITH_TIMEOUT(verticalColumn->isVisible(), 5000);
        QTest::wheelEvent(&view, QPointF(294, 295), QPoint(), QPoint(0, 8), Qt::ControlModifier, Qt::ScrollBegin);
        QTest::wheelEvent(&view, QPointF(294, 295), QPoint(), QPoint(0, 48), Qt::ControlModifier, Qt::ScrollUpdate);
        QTRY_VERIFY_WITH_TIMEOUT(visualController.state().value(QStringLiteral("zoom")).toDouble() > 1, 5000);
        visualController.resetZoom();
        const QImage readingPreview = view.grabWindow();
        QVERIFY(!readingPreview.isNull());
        QVERIFY(readingPreview.save(QStringLiteral(READER_READING_PREVIEW_PATH)));
        view.rootObject()->setProperty("presented", false);
    }

private:
    QTemporaryDir m_directory;
    QString m_textPath, m_pdfPath, m_epubPath;
    ReaderImageProvider *m_provider = nullptr;
    std::unique_ptr<ReaderController> m_controller;
};

QTEST_MAIN(ReaderTests)
#include "reader.moc"
