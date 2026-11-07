#include "accountcontroller.hpp"
#include "accountlogin.hpp"
#include "accountartwork.hpp"
#include "watchanimationcontroller.hpp"
#include "endfield/modules/hypergryph_account_cache.hpp"
#include <QBuffer>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFontDatabase>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace h=endfield::modules::hypergryph;
class AccountViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state MEMBER state NOTIFY changed)
public:
    QVariantMap state;
    Q_INVOKABLE void action(const QString &,const QVariant & = {}) {}
signals:
    void changed();
};
class AccountComponents : public QObject {
    Q_OBJECT
    QTemporaryDir files;
private slots:
    void initTestCase() {
        QVERIFY(files.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,files.path());
        for(const auto *weight:{"Regular","Medium","Bold"}) QVERIFY(QFontDatabase::addApplicationFont(QString(ACCOUNT_SOURCE_DIR)+"/assets/fonts/HarmonyOS_Sans_"+weight+".ttf")>=0);
        qmlRegisterType<ProfileArtwork>("HyprEndfield.Profile",1,0,"ProfileArtwork");
        qmlRegisterType<GameProfileArtwork>("HyprEndfield.Account",1,0,"GameProfileArtwork");
        qmlRegisterType<AccountGaugeArtwork>("HyprEndfield.Account",1,0,"AccountGaugeArtwork");
    }
    void independentProfilesAndLocks() {
        QTemporaryDir directory;
        ProfileController local(nullptr,directory.filePath("custom")),game(nullptr,directory.filePath("game"));
        QVERIFY(local.setField("name","Linux User")); QVERIFY(local.setField("permissionLevel",7));
        game.setLockedFields({"name","permissionLevel"});
        QVERIFY(!game.setField("name","manual override"));
        QVERIFY(game.applyGameFields({{"name","Endministrator"},{"permissionLevel",43},{"tag","1234"},{"playerID","500001"}}));
        QCOMPARE(local.state().value("name").toString(),QString("Linux User"));
        QCOMPARE(game.state().value("name").toString(),QString("Endministrator"));
        QVERIFY(!game.applyGameFields({{"name","invalid batch"},{"tag","12 3"}}));
        QCOMPARE(game.state().value("name").toString(),QString("Endministrator"));
        QVERIFY(game.applyGameFields({{"name","  Very long display name exceeding twenty  "},{"permissionLevel",99},{"explorationLevel",0}}));
        QCOMPARE(game.state().value("name").toString(),QString("Very long display na"));
        QCOMPARE(game.state().value("permissionLevel").toInt(),60);QCOMPARE(game.state().value("explorationLevel").toInt(),1);
        QVERIFY(game.applyGameFields({{"name",QString::fromUtf8("🙂").repeated(20)}}));
        QCOMPARE(game.state().value("name").toString(),QString::fromUtf8("🙂").repeated(20));
        game.setLockedFields({}); QVERIFY(game.setField("name","Editable"));
        QImage portrait(64,64,QImage::Format_RGB32);portrait.fill(Qt::cyan);QByteArray png;QBuffer output(&png);output.open(QIODevice::WriteOnly);QVERIFY(portrait.save(&output,"PNG"));
        QVERIFY(!game.importGameAvatar(png).isEmpty()); QVERIFY(local.image("avatar").isNull());
        ProfileController restored(nullptr,directory.filePath("game"));
        QCOMPARE(restored.state().value("name").toString(),QString("Editable"));QCOMPARE(restored.image("avatar").size(),QSize(64,64));
        AccountController account(&game,nullptr,directory.filePath("account"));account.setGameCard(false);QVERIFY(!account.gameCard());account.setGameCard(true);QVERIFY(account.gameCard());
        AccountController reloaded(&game,nullptr,directory.filePath("account"));QVERIFY(reloaded.gameCard());account.setGameCard(false);
    }
    void loginHidesHudWithoutCancelling() {
        QTemporaryDir directory;ProfileController game(nullptr,directory.filePath("game"));
        { AccountController account(&game,nullptr,directory.filePath("account"));
          QSignalSpy requested(&account,&AccountController::loginRequested);
          account.setVisible(true,true);account.action("connect");QCOMPARE(requested.count(),1);
          QVERIFY(account.state().value("busy").toBool());account.setVisible(false,false);
          QCoreApplication::processEvents();QVERIFY(account.state().value("busy").toBool());
        }
        QCoreApplication::processEvents(); // Deferred cancellation cannot outlive its Qt owner.
    }
    void cachedRolesKeepPageHeaderAndProfilesSeparate() {
        QTemporaryDir directory;
        const auto observed=h::fromUnix(QDateTime::currentMSecsSinceEpoch()/1000.);
        h::Role endfield;endfield.bindingUID="fixture-endfield";endfield.roleID="500001";endfield.name="Endministrator";
        h::Role arknights;arknights.game=h::Game::arknights;arknights.bindingUID="fixture-arknights";arknights.roleID="700001";arknights.name="Doctor";
        h::Snapshot endfieldData;endfieldData.role=endfield;endfieldData.name="Endministrator#1234";endfieldData.level=43;endfieldData.observedAt=observed;
        endfieldData.stamina=h::Stamina{135,240,observed+105*h::endfieldSecondsPerPoint,observed};
        h::Snapshot arknightsData;arknightsData.role=arknights;arknightsData.name="Doctor";arknightsData.observedAt=observed;
        arknightsData.stamina=h::Stamina{42,120,observed+78*h::arknightsSecondsPerPoint,observed};
        h::AccountCache cache;cache.header="arknights";cache.syncProfile=true;
        auto &record=cache.records[std::string(h::regionName(h::Region::mainland))];
        record.linked=true;record.requiresReconnect=true; // Offline cache only; never read credentials or call an API.
        record.roles={endfield,arknights};record.selectedRoleID=endfield.id();
        record.snapshots={{endfield.id(),endfieldData},{arknights.id(),arknightsData}};
        const auto bytes=QByteArray::fromStdString(h::encodeCacheBytes(cache));
        QFile file(directory.filePath("profile-cache.json"));QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(bytes),bytes.size());file.close();
        ProfileController local(nullptr,directory.filePath("custom")),game(nullptr,directory.filePath("game"));
        QVERIFY(local.setField("name","Linux User"));
        AccountController account(&game,nullptr,directory.path());account.setVisible(true,true);
        QCOMPARE(account.state().value("current").toString(),QString("135"));
        QCOMPARE(account.state().value("gauge").toString(),QString("42 / 120"));
        QCOMPARE(game.state().value("name").toString(),QString("Endministrator"));
        QCOMPARE(local.state().value("name").toString(),QString("Linux User"));
        QVERIFY(!game.setField("name","Locked"));account.action("name",false);QVERIFY(game.setField("name","Custom game name"));
        account.action("sync",true);QCOMPARE(game.state().value("name").toString(),QString("Custom game name"));
        account.action("header","workMode");QVERIFY(!account.state().value("gaugeVisible").toBool());
        QCOMPARE(account.state().value("current").toString(),QString("135"));
        account.action("role",QString::fromStdString(arknights.id()));
        QCOMPARE(account.state().value("current").toString(),QString("42"));
        QVERIFY(game.setField("permissionLevel",9));
        account.setVisible(false,false);
    }
    void consentCancellationAndSessionPolicy() {
        QObject owner;bool requested=false,finished=false,completed=false;
        auto login=account::makeLogin(&owner,[&]{requested=true;},[&]{finished=true;},[]{return endfield::core::Language::simplifiedChinese;});
        login->present(h::Region::mainland,[&](auto result,auto failure){completed=true;QVERIFY(!result);QCOMPARE(failure,h::LoginFailure::cancelled);});
        QVERIFY(requested);login->show();
        QDialog *consent=nullptr;for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QDialog *>(widget))if(dialog->windowTitle().contains("HyprEndfield"))consent=dialog;
        QVERIFY(consent);consent->reject();QTRY_VERIFY(finished);QVERIFY(completed);QVERIFY(!login->isPresenting());
        h::LoginSession session(h::Region::mainland,"synthetic-00000000-1111-2222-3333");
        QVERIFY(!session.allowNavigation("https://www.skland.com/",true,false));QVERIFY(session.consent(100));
        session.pageStarted(100);session.deadline(125);QCOMPARE(session.status(),std::string("failed"));QVERIFY(session.retry(130));
        QVERIFY(session.allowNavigation("https://www.skland.com/",true,false));
        QVERIFY(!session.allowNavigation("https://example.com/",true,false));
        QVERIFY(session.deadline(1000).has_value());
    }
    void originalCredentialScriptWithQtResultDelivery() {
        QJSEngine js;
        const auto setup=js.evaluate(R"(var window=this;window.top=window;var location={origin:'https://www.skland.com'};
          var values={SK_OAUTH_CRED_KEY:'SYNTHETIC-CRED',SK_TOKEN_CACHE_KEY:'synthetic-token',SK_SHUMEI_DEVICE_ID_KEY:'{"id":"synthetic-device"}'};
          window.localStorage={getItem:key=>values[key]||null};
          var document={documentElement:{}};var MutationObserver=function(){this.observe=function(){};this.disconnect=function(){}};
          var setTimeout=function(){return 1},clearTimeout=function(){};window.addEventListener=function(){};)" );
        QVERIFY2(!setup.isError(),qPrintable(setup.toString()));
        const auto script=account::credentialScript(h::Region::mainland,"synthetic-00000000-1111-2222-3333");
        const auto result=js.evaluate(script);QVERIFY2(!result.isError(),qPrintable(result.toString()));
        const auto message=QJsonDocument::fromJson(result.toString().toUtf8()).object();
        QCOMPARE(message.value("channel").toString(),QString("endfieldCommunityCredential"));
        QCOMPARE(message.value("body").toObject().value("cred").toString(),QString("SYNTHETIC-CRED"));
        QVERIFY(!js.evaluate(account::directScript(h::Region::mainland,"synthetic-00000000-1111-2222-3333")).isError());
        js.evaluate("location.origin='https://example.com'");QVERIFY(js.evaluate(script).isUndefined());
    }
    void qmlPagesAndOriginalCard() {
        QTemporaryDir directory;ProfileController local(nullptr,directory.filePath("custom")),game(nullptr,directory.filePath("game"));
        QVERIFY(local.setField("name","Arch User"));QVERIFY(game.applyGameFields({{"name","Endministrator"},{"permissionLevel",60},{"tag","1000"}}));
        AccountController account(&game,nullptr,directory.filePath("account"));account.setGameCard(false);
        AccountViewModel preview;preview.state=account.state();
        WatchAnimationController animation;QQmlEngine engine;
        engine.rootContext()->setContextProperty("profileController",&local);engine.rootContext()->setContextProperty("gameProfileController",&game);
        engine.rootContext()->setContextProperty("accountController",&account);engine.rootContext()->setContextProperty("sourceWatchAnimation",&animation);
        for(const auto *font:{"harmonyRegular","harmonyMedium","harmonyBold"})engine.rootContext()->setContextProperty(font,QVariantMap{{"name","HarmonyOS Sans"}});
        for(const auto *file:{"AccountPage.qml","ProfilePage.qml","GameProfileBadge.qml","AccountGauge.qml"}) {
            QQmlComponent component(&engine,QUrl::fromLocalFile(QString(ACCOUNT_SOURCE_DIR)+"/qml/"+file));
            QVariantMap props;
            if(QString(file)=="AccountPage.qml" || QString(file)=="AccountGauge.qml")props["controller"]=QVariant::fromValue(&preview);
            if(QString(file)=="GameProfileBadge.qml") props["shell"]=QVariantMap{{"animationPhase",0},{"sourceAnimationTime",animation.openingDuration()},{"sourceGyro",QVariant::fromValue(animation.neutralGyro())}};
            std::unique_ptr<QObject> page(component.createWithInitialProperties(props));QVERIFY2(page,qPrintable(component.errorString()));
            auto *item=qobject_cast<QQuickItem *>(page.get());QVERIFY(item);QQuickWindow window;window.setColor(QColor("#191B1D"));
            const bool badge=QString(file)=="GameProfileBadge.qml";window.resize(badge ? 1920 : 780,badge ? 1080 : 640);item->setParentItem(window.contentItem());if(!badge)item->setPosition({60,50});window.show();QTest::qWait(80);
            if(QString(file)=="ProfilePage.qml") {
                QVERIFY(QMetaObject::invokeMethod(item,"switchMode",Q_ARG(QVariant,QString("game"))));QVERIFY(account.gameCard());QCOMPARE(item->property("profile").toMap().value("name").toString(),QString("Endministrator"));
                QVERIFY(QMetaObject::invokeMethod(item,"switchMode",Q_ARG(QVariant,QString("custom"))));QVERIFY(!account.gameCard());
            }
            if(qEnvironmentVariableIsSet("HYPR_ACCOUNT_RENDER"))QVERIFY(window.grabWindow().save(QString(ACCOUNT_PREVIEW_DIR)+"/"+file+".png"));
            if(QString(file)=="AccountPage.qml") {
                preview.state.insert("linked",true);preview.state.insert("current","135");preview.state.insert("maximum","240");preview.state.insert("filled",14);
                preview.state.insert("next","00:05:12");preview.state.insert("full","12:36:42");preview.state.insert("lastSync","更新于 11/08 03:00:00");
                preview.state.insert("roles",QVariantList{QVariantMap{{"key","fixture"},{"label","Endministrator / 终末地"},{"title","Endministrator"}}});preview.state.insert("selectedRole","fixture");
                preview.state.insert("gaugeVisible",true);preview.state.insert("gauge","135 / 240");preview.state.insert("gaugeNext","00:05:12");preview.state.insert("gaugeFull","12:36:42");
                emit preview.changed();QTest::qWait(80);
                if(qEnvironmentVariableIsSet("HYPR_ACCOUNT_RENDER"))QVERIFY(window.grabWindow().save(QString(ACCOUNT_PREVIEW_DIR)+"/AccountLinked.png"));
            }
            item->setParentItem(nullptr);
        }
    }
};
QTEST_MAIN(AccountComponents)
#include "account_components.moc"
