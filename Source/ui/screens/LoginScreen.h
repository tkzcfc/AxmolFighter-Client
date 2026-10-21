#pragma once

#include "ui/core/Screen.h"

namespace gameui
{

class LoginScreen : public Screen
{
public:
    std::vector<std::string> getPackages() const override { return {"UI/Screen_Login"}; }

    GComponent* onCreateContent() override { return UIPackage::createObject("Screen_Login", "LoginScreen")->as<GComponent>(); }

    virtual void onEnter() override;
};

}  // namespace gameui
