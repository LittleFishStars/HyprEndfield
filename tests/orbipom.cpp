#include "orbipomcontroller.hpp"
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class OrbiPomTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString mismatch;
    bool compare(const QJsonValue &actual, const QJsonValue &expected, const QString &path) {
        if (actual.type() != expected.type()) { mismatch=path+": type";return false; }
        if(expected.isObject()) { const auto a=actual.toObject(),e=expected.toObject(); if(a.size()!=e.size()){mismatch=path+": fields";return false;} for(auto i=e.begin();i!=e.end();++i)if(!compare(a.value(i.key()),i.value(),path+'/'+i.key()))return false;return true; }
        if(expected.isArray()) { const auto a=actual.toArray(),e=expected.toArray();if(a.size()!=e.size()){mismatch=path+": size";return false;}for(int i=0;i<e.size();++i)if(!compare(a[i],e[i],path+'/'+QString::number(i)))return false;return true; }
        if(expected.isDouble()) { if(std::abs(actual.toDouble()-expected.toDouble())<=1e-8*std::max(1.,std::abs(expected.toDouble())))return true; }
        else if(actual==expected)return true;
        mismatch=path+": "+actual.toVariant().toString()+" != "+expected.toVariant().toString();return false;
    }
private slots:
    void initTestCase() {
        QVERIFY(directory.isValid());QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
    }
    void originalSingleStepFixtures() {
        QFile file(QString(ORBIPOM_SOURCE_DIR)+"/tests/fixtures/orbipom-step-source.json");QVERIFY(file.open(QIODevice::ReadOnly));
        const auto cases=QJsonDocument::fromJson(file.readAll()).object().value("cases").toArray();QVERIFY(cases.size()>10);
        for(const auto &value:cases) {
            QSettings().clear();const auto fixture=value.toObject();OrbiPomController game;QVERIFY(game.start(fixture["seed"].toInt()));game.setActive(true);
            const auto setup=game.m_engine->evaluate(fixture["setup"].toString());QVERIFY2(!setup.isError(),qPrintable(setup.toString()));game.refresh();
            QVERIFY2(compare(QJsonObject::fromVariantMap(game.m_state),fixture["before"],fixture["name"].toString()+"/before"),qPrintable(mismatch));
            game.advance(1./60);
            QVERIFY2(compare(QJsonObject::fromVariantMap(game.m_state),fixture["after"],fixture["name"].toString()+"/after"),qPrintable(mismatch));
        }
    }
    void pauseAndPersistence() {
        QSettings().clear();OrbiPomController game;QVERIFY(game.start(12345));game.setActive(true);game.drop();game.advance(.1);
        const auto time=game.state().value("simulationTime");game.setActive(false);game.advance(1.);QCOMPARE(game.state().value("simulationTime"),time);QVERIFY(!game.m_clock.isActive());
        game.setActive(true);game.togglePause();game.setActive(false);game.setActive(true);QVERIFY(game.state().value("manuallyPaused").toBool());QVERIFY(!game.m_clock.isActive());
        game.togglePause();game.setHeld(true);QVERIFY(!game.m_clock.isActive());game.setHeld(false);QVERIFY(game.m_clock.isActive());
        game.setLowPower(true);QCOMPARE(game.m_clock.interval(),33);game.setLowPower(false);QCOMPARE(game.m_clock.interval(),16);
        game.call("highScore",{123});game.refresh();game.setActive(false);QCOMPARE(QSettings().value("orbipom/bestScore.v1").toInt(),123);
    }
    void pageAndArtwork() {
        QQmlEngine engine;OrbiPomController game;engine.rootContext()->setContextProperty("orbipomController",&game);
        for(const auto &weight:{"Regular","Medium","Bold"}){const int id=QFontDatabase::addApplicationFont(QString(ORBIPOM_SOURCE_DIR)+"/assets/fonts/HarmonyOS_Sans_"+weight+".ttf");QVERIFY(id>=0);engine.rootContext()->setContextProperty(QString("harmony")+weight,QVariantMap{{"name",QFontDatabase::applicationFontFamilies(id).first()}});}
        QSignalSpy warnings(&engine,&QQmlEngine::warnings);QQmlComponent component(&engine,QUrl::fromLocalFile(QString(ORBIPOM_SOURCE_DIR)+"/qml/OrbiPomPage.qml"));
        std::unique_ptr<QObject> page(component.create());QVERIFY2(page!=nullptr,qPrintable(component.errorString()));
        QQuickWindow window;window.setColor(QColor("#181818"));window.resize(690,650);auto *item=qobject_cast<QQuickItem *>(page.get());item->setParentItem(window.contentItem());item->setPosition({50,40});window.show();page->setProperty("presented",true);QTest::qWait(70);
        QVERIFY(game.start(12345));game.setActive(true);game.drop();for(int i=0;i<30;++i)game.advance(1./60);QTest::qWait(30);
        if(qEnvironmentVariableIsSet("HYPR_ORBIPOM_RENDER"))QVERIFY(window.grabWindow().save(QString(ORBIPOM_PREVIEW_DIR)+"/orbipom-page.png"));
        page->setProperty("rules",true);QTest::qWait(30);QVERIFY(game.held());
        if(qEnvironmentVariableIsSet("HYPR_ORBIPOM_RENDER"))QVERIFY(window.grabWindow().save(QString(ORBIPOM_PREVIEW_DIR)+"/orbipom-rules.png"));
        page->setProperty("rules",false);page->setProperty("confirmRestart",true);QTest::qWait(30);QVERIFY(game.held());
        if(!warnings.isEmpty())for(const auto &warning:warnings)qWarning()<<warning;QVERIFY(warnings.isEmpty());item->setParentItem(nullptr);
    }
};
int main(int argc,char **argv){QGuiApplication app(argc,argv);QCoreApplication::setOrganizationName("HyprEndfieldTest");QCoreApplication::setApplicationName("OrbiPomTest");qmlRegisterType<OrbiPomBoard>("HyprEndfield.OrbiPom",1,0,"OrbiPomBoard");OrbiPomTests tests;return QTest::qExec(&tests,argc,argv);}
#include "orbipom.moc"
