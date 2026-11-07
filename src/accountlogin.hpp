#pragma once
#include "endfield/modules/hypergryph_account_login.hpp"
#include <QString>
class QObject;
namespace account {
namespace h = endfield::modules::hypergryph;
QString credentialScript(h::Region region, std::string_view nonce);
QString directScript(h::Region region, std::string_view nonce);
class Login : public h::AccountLoginPresenter {
public:
    virtual void show() = 0;
};
std::unique_ptr<Login> makeLogin(QObject *owner, std::function<void()> requested,
    std::function<void()> finished, std::function<endfield::core::Language()> language);
}
