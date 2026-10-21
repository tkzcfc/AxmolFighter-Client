#pragma once

#include "ui/widgets/common/Dialog.h"

namespace gameui
{

class LoginDialog : public Dialog
{
public:
    LoginDialog()
    {
        m_options.hasBackground  = false;
        m_options.draggable      = false;
        m_options.fullscreen     = false;
        m_options.closeOnClickBg = false;
        m_playAnimation          = false;
    }

    virtual ~LoginDialog() {}

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/Screen_Login"}; }

    virtual GComponent* onCreateContent() override { return createCenteredComponent("Screen_Login", "LoginDialog"); }

    virtual void onCreate() override;

    void onClickLoginButton(EventContext* context);

    void onClickToRegisterButton(EventContext* context);

    void onLoginSuccess();

private:
    GTextInput* m_usernameInput = nullptr;
    GTextInput* m_passwordInput = nullptr;
};

}  // namespace gameui
