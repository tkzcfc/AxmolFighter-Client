#include "RegisterPanel.h"
#include "LoginPanel.h"
#include "ui/widgets/common/MessagePopup.h"
#include <net/client_game.pb.h>

namespace gameui
{

void RegisterPanel::onCreate()
{
    auto backButton     = this->getChild<GButton>("backButton");
    auto registerButton = this->getChild<GButton>("registerButton");

    this->addClickListener(backButton, AX_CALLBACK_1(RegisterPanel::onClickBackButton, this));
    this->addClickListener(registerButton, AX_CALLBACK_1(RegisterPanel::onClickRegisterButton, this));
}

void RegisterPanel::onClickBackButton(EventContext* context)
{
    getUIManager()->open<LoginPanel>();
    this->close();
}

void RegisterPanel::onClickRegisterButton(EventContext* context)
{
    auto usernameInput        = this->getChild<GComponent>("usernameInput")->getChild("input")->as<GTextInput>();
    auto passwordInput        = this->getChild<GComponent>("passwordInput")->getChild("input")->as<GTextInput>();
    auto confirmPasswordInput = this->getChild<GComponent>("confirmPasswordInput")->getChild("input")->as<GTextInput>();

    auto username        = usernameInput->getText();
    auto password        = passwordInput->getText();
    auto confirmPassword = confirmPasswordInput->getText();

    if (username.empty() || password.empty())
    {
        MessagePopup::show("请输入账号和密码");
        return;
    }

    if (password != confirmPassword)
    {
        MessagePopup::show("两次密码输入不一致");
        return;
    }

    AXLOGI("Register with username: {}", username);

    PB::Game::RegisterReq req;
    req.set_account(username);
    req.set_password(password);
    req.set_nickname(username);

    this->call(req, [this](const PB::Game::RegisterResp* resp, std::string_view error) {
        if (!resp)
        {
            MessagePopup::showNetErr(error);
            return;
        }

        if (resp->code() == 0)
        {
            AXLOGI("Register success, player_id={}", resp->player_id());
            MessagePopup::show("注册成功，请登录");
            getUIManager()->open<LoginPanel>();
            this->close();
        }
        else
        {
            AXLOGW("Register failed, code={}, message={}", resp->code(), resp->message());
            MessagePopup::show(resp->message().empty() ? "注册失败" : resp->message());
        }
    });
}

}  // namespace gameui
