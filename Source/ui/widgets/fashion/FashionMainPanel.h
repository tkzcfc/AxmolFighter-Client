#pragma once

#include "ui/widgets/common/Panel.h"
#include "fairygui/GLoader3D.h"
#include "mugen/conf/GameDef.h"
#include "mugen/avatar/render/Avatar.h"

#include <array>
#include <memory>
#include <utility>

namespace gameui
{

// 装扮主界面（Panel_Fashion / FashionMainPanel）：展示各部位入口与角色预览。
class FashionMainPanel : public Panel
{
public:
    FashionMainPanel() = default;

    // Called when FashionDressPanel closes so slots/avatar refresh.
    static void requestRefresh();

protected:
    std::vector<std::string> getPackages() const override { return {"UI/Res_GeneralIcons", "UI/Res_FashionIcons", "UI/Res_FashionSuitIcons", "UI/Panel_Fashion"}; }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Panel_Fashion", "FashionMainPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

    void onClickBack(EventContext* context);
    void onClickPart(EventContext* context, mugen::FashionPosition position);
    void onClickWalk(EventContext* context);

    void refreshSlots();
    void refreshAvatar();
    GComponent* container() const;

private:
    GLoader3D* m_avatarLoader = nullptr;
    mugen::Avatar* m_avatar   = nullptr;
    bool m_walkPose           = false;

    static std::weak_ptr<FashionMainPanel> s_active;
};

}  // namespace gameui
