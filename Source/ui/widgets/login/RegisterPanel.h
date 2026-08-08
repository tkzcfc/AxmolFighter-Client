#pragma once

#include "ui/core/UIWidget.h"

namespace gameui
{

class RegisterPanel : public UIWidget
{
public:
    RegisterPanel()
    {
        m_options.hasBackground  = false;
        m_options.draggable      = false;
        m_options.fullscreen     = false;
        m_options.closeOnClickBg = false;
    }

    virtual ~RegisterPanel() = default;

protected:
    virtual std::vector<std::string> getPackages() const override { return {"UI/Login"}; }

    virtual GComponent* onCreateContent() override { return createCenteredComponent("Login", "RegisterPanel"); }

    virtual void onCreate() override;

    void onClickBackButton(EventContext* context);

    void onClickRegisterButton(EventContext* context);
};

}  // namespace gameui
