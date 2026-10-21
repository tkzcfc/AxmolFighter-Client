#include "LoginDialog.h"
#include "RegisterDialog.h"
#include "AppContext.h"
#include "ui/screens/CharacterLobbyScreen.h"
#include "ui/widgets/common/MessageDialog.h"
#include <net/client_game.pb.h>

namespace gameui
{

void LoginDialog::onCreate()
{
    auto usernameInput = this->getChild<GComponent>("usernameInput")->getChild("input")->as<GTextInput>();
    auto passwordInput = this->getChild<GComponent>("passwordInput")->getChild("input")->as<GTextInput>();

    usernameInput->setText(ax::UserDefault::getInstance()->getStringForKey("account", "").data());
    passwordInput->setText(ax::UserDefault::getInstance()->getStringForKey("password", "").data());

    m_usernameInput = usernameInput;
    m_passwordInput = passwordInput;

    auto loginButton      = this->getChild<GButton>("loginButton");
    auto toRegisterButton = this->getChild<GButton>("toRegisterButton");

    this->addClickListener(loginButton, AX_CALLBACK_1(LoginDialog::onClickLoginButton, this));
    this->addClickListener(toRegisterButton, AX_CALLBACK_1(LoginDialog::onClickToRegisterButton, this));
}

void LoginDialog::onClickLoginButton(EventContext* context)
{
    auto username = m_usernameInput->getText();
    auto password = m_passwordInput->getText();

    if (username.empty() || password.empty())
    {
        MessageDialog::show("请输入账号和密码");
        return;
    }

    AXLOGI("Login with username: {}", username);

    PB::Game::LoginReq req;
    req.set_account(username);
    req.set_password(password);

    this->call(req, [this](const PB::Game::LoginResp* resp, std::string_view error) {
        if (!resp)
        {
            MessageDialog::showNetErr(error);
            return;
        }

        if (resp->code() == 0)
        {
            if (auto* session = AppContext::get().gameSession())
            {
                session->setFromLoginResp(*resp, m_usernameInput->getText());
            }

            AXLOGI("Login success, player_id={}, nickname={}", resp->player_id(), resp->nickname());
            onLoginSuccess();
        }
        else
        {
            AXLOGW("Login failed, code={}, message={}", resp->code(), resp->message());
            MessageDialog::show(resp->message().empty() ? "登录失败" : resp->message());
        }
    });
}

void LoginDialog::onClickToRegisterButton(EventContext* context)
{
    getUIManager()->open<RegisterDialog>();
    this->close();
}

void LoginDialog::onLoginSuccess()
{
    ax::UserDefault::getInstance()->setStringForKey("account", m_usernameInput->getText());
    ax::UserDefault::getInstance()->setStringForKey("password", m_passwordInput->getText());

    getUIManager()->getScreenManager()->switchScreen<CharacterLobbyScreen>();
}

}  // namespace gameui
