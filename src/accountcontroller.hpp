#pragma once
#include <QObject>
#include <QVariantMap>
#include <memory>
class ProfileController;

// Qt host for EndfieldHUD's unchanged account controller, API and local clock.
class AccountController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(bool gameCard READ gameCard WRITE setGameCard NOTIFY modeChanged)
public:
    explicit AccountController(ProfileController *gameProfile, QObject *parent = nullptr, QString directory = {});
    ~AccountController() override;
    QVariantMap state() const;
    bool gameCard() const;
    void setGameCard(bool value);
    Q_INVOKABLE void action(const QString &name, const QVariant &value = {});
    Q_INVOKABLE void setVisible(bool visible, bool accountPage);
    Q_INVOKABLE void showLogin();
    void setLanguage(int index);
signals:
    void changed();
    void modeChanged();
    void loginRequested();
    void loginFinished();
    void eventOccurred(const QString &action);
private:
    struct Data;
    std::unique_ptr<Data> d;
};
