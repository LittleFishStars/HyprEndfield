#include "accountcontroller.hpp"
#include "accountlogin.hpp"
#include "profilecontroller.hpp"
#include "endfield/modules/hypergryph_account_controller.hpp"
#include "endfield/modules/hypergryph_account_sanity_gauge.hpp"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrentRun>
#include <stdexcept>
#include <algorithm>
#include <climits>
#include <cmath>
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")

namespace h = endfield::modules::hypergryph;
namespace {
QString q(std::string_view s) { return QString::fromUtf8(s.data(), s.size()); }
double now() { return h::fromUnix(QDateTime::currentMSecsSinceEpoch() / 1000.); }
class Request final : public h::AccountRequest {
public:
    QPointer<QNetworkReply> reply;
    bool cancelled = false;
    void cancel() override { cancelled = true; if (reply) reply->abort(); }
};
// All API calls are read-only. The imported signer controls their exact bytes.
class Transport final : public QObject, public h::HttpTransport, public h::AccountAvatarLoader {
    QNetworkAccessManager network;
public:
    void post(std::function<void()> callback) override { QTimer::singleShot(0, this, std::move(callback)); }
    h::RequestHandle get(const QNetworkRequest &request, qsizetype limit, std::function<void(h::HttpOutcome)> done) {
        auto handle = std::make_shared<Request>(); handle->reply = network.get(request);
        auto bytes = std::make_shared<QByteArray>(); auto tooLarge = std::make_shared<bool>(false);
        auto *reply = handle->reply.data(); reply->setReadBufferSize(64 * 1024);
        const auto read = [reply, bytes, tooLarge, limit] {
            const auto length = reply->header(QNetworkRequest::ContentLengthHeader);
            if (length.isValid() && length.toLongLong() > limit) *tooLarge = true;
            bytes->append(reply->read(std::max<qint64>(0, limit + 1 - bytes->size())));
            if (bytes->size() > limit) *tooLarge = true;
            if (*tooLarge && !reply->isFinished()) reply->abort();
        };
        connect(reply, &QNetworkReply::readyRead, reply, read);
        connect(reply, &QNetworkReply::metaDataChanged, reply, read);
        auto *deadline = new QTimer(reply); deadline->setSingleShot(true); deadline->start(20000);
        connect(deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
        connect(reply, &QNetworkReply::finished, this, [handle, bytes, tooLarge, read, done = std::move(done), reply] {
            read(); h::HttpOutcome outcome; outcome.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (handle->cancelled) outcome.failure = h::Error{h::ErrorKind::cancelled};
            else if (*tooLarge) outcome.failure = h::Error{h::ErrorKind::responseTooLarge};
            else if (reply->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) outcome.failure = h::Error{h::ErrorKind::unsafeRedirect};
            else if (reply->error() != QNetworkReply::NoError && outcome.status == 0) outcome.failure = h::Error{h::ErrorKind::transport};
            else outcome.body = bytes->toStdString();
            handle->reply = nullptr; reply->deleteLater(); if (!handle->cancelled) done(std::move(outcome));
        });
        return handle;
    }
    h::RequestHandle start(h::HttpRequest request, std::function<void(h::HttpOutcome)> done) override {
        const bool host = request.host == h::apiHost(h::Region::mainland) || request.host == h::apiHost(h::Region::global);
        if (!host || !h::allowedPath(request.path)) { post([done] { done({h::Error{h::ErrorKind::unsafeRedirect}, 0, {}}); }); return std::make_shared<Request>(); }
        QNetworkRequest req(QUrl(q(request.url())));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setTransferTimeout(15000);
        for (const auto &[key, value] : request.headers) req.setRawHeader(QByteArray::fromStdString(key), QByteArray::fromStdString(value));
        return get(req, h::maximumResponseBytes, std::move(done));
    }
    h::RequestHandle load(const std::string &url, std::function<void(std::optional<std::vector<std::uint8_t>>)> done) override {
        if (!h::avatarURLAllowed(url)) { post([done] { done({}); }); return std::make_shared<Request>(); }
        QNetworkRequest req{QUrl(q(url))}; req.setTransferTimeout(15000);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        return get(req, 4 * 1024 * 1024, [done](h::HttpOutcome outcome) {
            if (outcome.failure || outcome.status != 200) { done({}); return; }
            QByteArray bytes = QByteArray::fromStdString(outcome.body); QBuffer input(&bytes); input.open(QIODevice::ReadOnly);
            QImageReader reader(&input); reader.setAutoTransform(true); const QSize size = reader.size();
            if (!size.isValid() || qint64(size.width()) * size.height() > 100'000'000) { done({}); return; }
            if (size.width() > 512 || size.height() > 512) reader.setScaledSize(size.scaled(512, 512, Qt::KeepAspectRatio));
            auto image = reader.read(); if (image.isNull()) { done({}); return; }
            image = image.scaled(image.size().boundedTo(QSize(512, 512)), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QByteArray png; QBuffer output(&png); output.open(QIODevice::WriteOnly);
            if (!image.save(&output, "PNG")) { done({}); return; }
            done(std::vector<std::uint8_t>(png.cbegin(), png.cend()));
        });
    }
};
const SecretSchema accountSchema = [] {
    SecretSchema schema{}; schema.name = "com.octoberprayrain.HyprEndfield.account"; schema.flags = SECRET_SCHEMA_NONE;
    schema.attributes[0].name = "region"; schema.attributes[0].type = SECRET_SCHEMA_ATTRIBUTE_STRING; return schema;
}();
class Vault final : public h::AccountCredentialVault {
    static void check(GError *error) { if (error) { g_error_free(error); throw std::runtime_error("Secret Service unavailable"); } }
public:
    std::optional<h::Credentials> load(h::Region region) override {
        const auto key = std::string(h::regionName(region)); GError *error = nullptr;
        gchar *saved = secret_password_lookup_sync(&accountSchema, nullptr, &error, "region", key.c_str(), nullptr); check(error);
        if (!saved) return {};
        const std::string bytes(saved); secret_password_free(saved);
        const auto record = h::Json::parse(bytes, 16384);
        h::Credentials credentials{record["cred"].string(), record["signingToken"].string(), {}};
        if (record["deviceID"].isString()) credentials.deviceID = record["deviceID"].string();
        if (!h::validSecret(credentials.cred) || !h::validSecret(credentials.signingToken) || (credentials.deviceID && !h::validSecret(*credentials.deviceID))) throw std::runtime_error("Invalid saved session");
        return credentials;
    }
    void save(const h::Credentials &credentials, h::Region region) override {
        h::Json record = h::Json::Object{{"cred", credentials.cred}, {"signingToken", credentials.signingToken}};
        if (credentials.deviceID) record["deviceID"] = *credentials.deviceID;
        const auto bytes = record.encode(16384), key = std::string(h::regionName(region)); GError *error = nullptr;
        const bool ok = secret_password_store_sync(&accountSchema, SECRET_COLLECTION_DEFAULT, "HyprEndfield community account", bytes.c_str(), nullptr, &error, "region", key.c_str(), nullptr);
        check(error); if (!ok) throw std::runtime_error("Session could not be saved");
    }
    void remove(h::Region region) override {
        const auto key = std::string(h::regionName(region)); GError *error = nullptr;
        secret_password_clear_sync(&accountSchema, nullptr, &error, "region", key.c_str(), nullptr); check(error);
    }
};
class WorkQueue final : public QObject, public h::AccountWorkQueue {
    QThreadPool pool;
public:
    WorkQueue() { pool.setMaxThreadCount(1); }
    ~WorkQueue() override { pool.waitForDone(); }
    void run(std::function<void()> work, std::function<void(std::exception_ptr)> done) override {
        auto *watcher = new QFutureWatcher<std::exception_ptr>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [watcher, done = std::move(done)] { const auto error = watcher->result(); watcher->deleteLater(); done(error); });
        watcher->setFuture(QtConcurrent::run(&pool, [work = std::move(work)] { try { work(); return std::exception_ptr{}; } catch (...) { return std::current_exception(); } }));
    }
};
class Cache final : public h::AccountCacheFile {
    QString path; QByteArray previous; bool readable = true;
public:
    explicit Cache(QString directory) : path(std::move(directory) + "/profile-cache.json") {}
    bool exists() override { return QFile::exists(path); }
    std::optional<std::string> read() override {
        QFile file(path); if (!file.exists()) return {};
        readable = file.open(QIODevice::ReadOnly); if (!readable) throw std::runtime_error("Account cache unreadable");
        previous = file.read(h::maximumCacheBytes + 1); if (previous.size() > qsizetype(h::maximumCacheBytes)) { readable = false; throw std::runtime_error("Account cache too large"); }
        return previous.toStdString();
    }
    void write(const std::string &bytes) override {
        QFile current(path);
        if (!readable || (current.exists() && (!current.open(QIODevice::ReadOnly) || current.read(h::maximumCacheBytes + 1) != previous)) || (!current.exists() && !previous.isEmpty())) throw std::runtime_error("Account cache changed externally");
        QSaveFile file(path); const auto data = QByteArray::fromStdString(bytes);
        if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly)) throw std::runtime_error("Account cache write failed");
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        if (file.write(data) != data.size() || !file.commit()) throw std::runtime_error("Account cache write failed");
        previous = data;
    }
};
class ProfileSink final : public h::AccountProfileSink {
    ProfileController *profile;
public:
    explicit ProfileSink(ProfileController *value) : profile(value) {}
    void applyGameProfile(const h::GameProfileUpdate &update) override {
        QVariantMap fields;
        const auto text = [&](const char *key, const std::optional<std::string> &value) { if (value) fields[key] = q(*value); };
        const auto number = [&](const char *key, const std::optional<std::int64_t> &value) { if (value) fields[key] = qlonglong(*value); };
        text("playerID", update.gamePlayerID); text("name", update.name); text("tag", update.tag);
        number("permissionLevel", update.permissionLevel); number("explorationLevel", update.explorationLevel); number("operatorsCount", update.operatorsCount); number("weaponsCount", update.weaponsCount); number("archivesCount", update.archivesCount);
        if (update.awakeningDate) fields["awakeningDate"] = QDateTime::fromMSecsSinceEpoch(qRound64(h::toUnix(*update.awakeningDate) * 1000)).date().toString(Qt::ISODate);
        if (!profile->applyGameFields(fields)) throw std::runtime_error("Personal profile could not be saved");
    }
    void setProfileSyncLocked(ehud::data::ProfileSyncFields fields) override {
        QStringList locked;
        for (auto f : ehud::data::profileSyncFieldOrder) if (fields.contains(f)) {
            using F = ehud::data::ProfileSyncField;
            switch (f) {
                case F::name: locked << "name"; break; case F::tag: locked << "tag"; break; case F::uid: locked << "playerID"; break;
                case F::awakening: locked << "awakeningDate"; break;
                case F::levels: locked << "permissionLevel" << "explorationLevel"; break;
                case F::counts: locked << "operatorsCount" << "weaponsCount" << "archivesCount"; break;
            }
        }
        profile->setLockedFields(locked);
    }
    std::optional<std::string> importGameAvatar(const std::vector<std::uint8_t> &png) override {
        const auto name = profile->importGameAvatar(QByteArray(reinterpret_cast<const char *>(png.data()), png.size()));
        if (name.isEmpty()) throw std::runtime_error("Avatar could not be saved");
        return name.toStdString();
    }
};
QString countdown(std::optional<double> deadline) {
    return q(h::gaugeCountdown(deadline, now(), true));
}
}
struct AccountController::Data {
    AccountController *owner;
    Transport transport; Vault vault; WorkQueue queue; Cache cache; ProfileSink sink;
    h::HypergryphAccountClient service;
    std::unique_ptr<account::Login> login;
    std::unique_ptr<h::HypergryphAccountController> core;
    endfield::core::Language language = endfield::core::Language::simplifiedChinese;
    QTimer timer; QSettings settings;
    bool visible = false, page = false, loginActive = false;
    Data(AccountController *o, ProfileController *profile, const QString &directory)
        : owner(o), cache(directory), sink(profile), service(transport, now) {
        login = account::makeLogin(owner, [this] {
            loginActive = true;
            emit owner->loginRequested();
        }, [this] {
            loginActive = false;
            emit owner->loginFinished();
            core->setVisible(visible, page); schedule();
        }, [this] { return language; });
        h::AccountControllerOptions options;
        options.now = now; options.language = language; options.wording = h::AccountWording::linuxDesktop;
        options.localDate = [](double date) { const auto value = QDateTime::fromMSecsSinceEpoch(qRound64(h::toUnix(date) * 1000)); const auto d = value.date(); const auto t = value.time(); return h::LocalDateFields{d.month(), d.day(), t.hour(), t.minute(), t.second()}; };
        options.changed = [this] { QTimer::singleShot(0, owner, [this] { schedule(); emit owner->changed(); }); };
        options.onEvent = [this](std::string_view action) { emit owner->eventOccurred(q(action)); };
        const auto avatar = profile->state().value("avatar").toString(); if (!avatar.isEmpty()) options.initialAvatarFilename = avatar.toStdString();
        core = std::make_unique<h::HypergryphAccountController>(service, vault, queue, &cache, &sink, login.get(), &transport, options);
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, owner, [this] { core->tick(); emit owner->changed(); schedule(); });
        QObject::connect(profile, &ProfileController::changed, owner, [this, profile] {
            const auto avatar = profile->state().value("avatar").toString();
            if (core) core->profileAvatarChanged(avatar.isEmpty() ? std::nullopt : std::optional(avatar.toStdString()));
        });
    }
    void schedule() {
        timer.stop(); if (!core) return;
        auto deadline = core->nextWakeTime();
        const auto presentation = core->presentation();
        if (visible && (core->sanityPresentation() || (page && presentation.snapshot && presentation.snapshot->stamina)))
            deadline = std::min(deadline.value_or(now() + 1), now() + 1);
        if (deadline) timer.start(int(std::clamp((*deadline - now()) * 1000., 50., double(INT_MAX))));
    }
};
AccountController::AccountController(ProfileController *gameProfile, QObject *parent, QString directory) : QObject(parent) {
    if (directory.isEmpty()) directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Account";
    d = std::make_unique<Data>(this, gameProfile, directory);
}
AccountController::~AccountController() = default;
bool AccountController::gameCard() const { return d->settings.value("profile/mode", "custom").toString() == "game"; }
void AccountController::setGameCard(bool value) {
    if (value == gameCard()) return;
    d->settings.setValue("profile/mode", value ? "game" : "custom"); emit modeChanged();
}
void AccountController::showLogin() { d->login->show(); }
void AccountController::setVisible(bool visible, bool accountPage) {
    d->visible = visible; d->page = accountPage;
    // The native login window is part of the account session. Concealing the
    // overlay for it must not trigger the source controller's cancel-on-hide.
    d->core->setVisible(visible || d->loginActive, accountPage && visible); d->schedule();
}
void AccountController::setLanguage(int index) {
    if (index == 0) { const QLocale locale = QLocale::system(); index = locale.language() == QLocale::Chinese ? (locale.script() == QLocale::TraditionalHanScript ? 3 : 2) : locale.language() == QLocale::Japanese ? 4 : locale.language() == QLocale::Korean ? 5 : 1; }
    d->language = static_cast<endfield::core::Language>(std::clamp(index, 1, 5)); d->core->setLanguage(d->language); emit changed();
}
void AccountController::action(const QString &name, const QVariant &value) {
    h::AccountAction action; using K = h::AccountAction::Kind;
    if (name == "connect") { action.kind = K::connect; action.region = d->core->presentation().region; }
    else if (name == "refresh") action.kind = K::refresh;
    else if (name == "disconnect") action.kind = K::disconnect;
    else if (name == "region") { action.kind = K::selectRegion; action.region = value == "global" ? h::AccountPresentation::RegionChoice::global : h::AccountPresentation::RegionChoice::china; }
    else if (name == "role") { action.kind = K::selectRole; action.roleID = value.toString().toStdString(); }
    else if (name == "header") { action.kind = K::selectHeaderMode; const auto mode = h::headerModeNamed(value.toString().toStdString()); if (!mode) return; action.headerMode = *mode; }
    else if (name == "sync") { action.kind = K::setSyncProfile; action.enabled = value.toBool(); }
    else if (name == "avatar") { action.kind = K::setSyncAvatar; action.enabled = value.toBool(); }
    else { const auto field = ehud::data::profileSyncFieldNamed(name.toStdString()); if (!field) return; action.kind = K::setSyncField; action.syncField = *field; action.enabled = value.toBool(); }
    d->core->perform(action); d->schedule(); emit changed();
}
QVariantMap AccountController::state() const {
    const auto p = d->core->presentation();
    const auto sanity = p.snapshot ? p.snapshot->sanityPresentation(now()) : std::nullopt;
    const auto headerSanity = d->core->sanityPresentation();
    QVariantList roles, fields, headers;
    for (const auto &role : p.roles) roles.append(QVariantMap{{"key", q(role.id)}, {"label", q(role.title + (role.subtitle.empty() ? "" : " / " + role.subtitle))}, {"title", q(role.title)}});
    for (auto field : ehud::data::profileSyncFieldOrder) fields.append(QVariantMap{{"key", q(ehud::data::profileSyncFieldName(field))}, {"title", q(h::syncFieldTitle(field, d->language))}, {"enabled", p.syncFields.contains(field)}});
    fields.append(QVariantMap{{"key", "avatar"}, {"title", tr("头像")}, {"enabled", p.syncAvatar}});
    for (auto mode : {h::AccountPresentation::HeaderMode::workMode, h::AccountPresentation::HeaderMode::endfield, h::AccountPresentation::HeaderMode::arknights, h::AccountPresentation::HeaderMode::hidden}) headers.append(QVariantMap{{"key", q(h::headerModeName(mode))}, {"label", q(h::headerModeTitle(mode, d->language))}});
    const auto gauge = d->core->gaugeValue({});
    return {{"linked", p.isLinked}, {"busy", p.busy()}, {"status", q(p.statusMessage)}, {"region", q(h::regionChoiceName(p.region))},
        {"roles", roles}, {"selectedRole", p.selectedRoleID ? q(*p.selectedRoleID) : ""}, {"headers", headers}, {"header", q(h::headerModeName(p.headerMode))},
        {"sync", p.syncProfile}, {"fields", fields}, {"lastSync", q(p.lastSync)}, {"reconnect", p.needsReconnect},
        {"current", sanity ? QString::number(sanity->current) : QString("—")}, {"maximum", sanity ? QString::number(sanity->maximum) : QString("—")},
        {"filled", sanity && sanity->maximum > 0 ? std::clamp(qRound(double(sanity->current) / sanity->maximum * 24), 0, 24) : 0},
        {"next", countdown(sanity ? sanity->nextRecoveryAt : std::nullopt)}, {"full", countdown(sanity ? sanity->fullRecoveryAt : std::nullopt)},
        {"hasSanity", bool(sanity)}, {"gauge", q(gauge.value)}, {"gaugeVisible", gauge.visible && headerSanity.has_value()},
        {"gaugeNext", countdown(headerSanity ? headerSanity->nextRecoveryAt : std::nullopt)},
        {"gaugeFull", countdown(headerSanity ? headerSanity->fullRecoveryAt : std::nullopt)}};
}
