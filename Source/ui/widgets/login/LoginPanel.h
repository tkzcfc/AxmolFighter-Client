#pragma once

#include "ui/core/UIWidget.h"

namespace gameui
{

class LoginPanel : public UIWidget
{
public:
    LoginPanel()
    {
        m_options.hasBackground  = false;
        m_options.draggable      = false;
        m_options.fullscreen     = false;
        m_options.closeOnClickBg = false;
    }

    virtual ~LoginPanel() {}

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/Login"}; }

    virtual GComponent* onCreateContent() override { return createCenteredComponent("Login", "LoginPanel"); }

    virtual void onCreate() override;

    void onClickLoginButton(EventContext* context);

    void onClickToRegisterButton(EventContext* context);

    void onLoginSuccess();

private:
    GTextInput* m_usernameInput = nullptr;
    GTextInput* m_passwordInput = nullptr;
};

}  // namespace gameui
