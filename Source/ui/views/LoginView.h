#pragma once

#include "ui/core/View.h"

namespace gameui
{

class LoginView : public View
{
public:
    std::vector<std::string> getPackages() const override { return {"UI/Login"}; }

    GComponent* onCreateContent() override { return UIPackage::createObject("Login", "LoginView")->as<GComponent>(); }

    virtual void onEnter() override;
};

}  // namespace gameui
