#pragma once

#include "ui/widgets/common/Dialog.h"

namespace gameui
{

class RegisterDialog : public Dialog
{
public:
    RegisterDialog()
    {
        m_options.hasBackground  = false;
        m_options.draggable      = false;
        m_options.fullscreen     = false;
        m_options.closeOnClickBg = false;
        m_playAnimation          = false;
    }

    virtual ~RegisterDialog() = default;

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/Screen_Login"}; }

    virtual GComponent* onCreateContent() override { return createCenteredComponent("Screen_Login", "RegisterDialog"); }

    virtual void onCreate() override;

    void onClickBackButton(EventContext* context);

    void onClickRegisterButton(EventContext* context);
};

}  // namespace gameui
