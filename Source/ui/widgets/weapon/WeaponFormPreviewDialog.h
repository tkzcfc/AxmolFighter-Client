#pragma once

#include "ui/widgets/common/Dialog.h"
#include "fairygui/GLoader3D.h"
#include "mugen/avatar/render/Avatar.h"

namespace gameui
{

// Popup: player Avatar wearing the selected armament weapon spine (heiyue UiGodWeaponRoleSpine).
class WeaponFormPreviewDialog : public Dialog
{
public:
    explicit WeaponFormPreviewDialog(int32_t armamentSpineId);

protected:
    std::vector<std::string> getPackages() const override { return {"UI/Common_Base", "UI/Panel_Weapon"}; }

    GComponent* onCreateContent() override;

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickClose(EventContext* context);
    void refreshAvatar();

    int32_t m_armamentSpineId = 0;
    GLoader3D* m_avatarLoader = nullptr;
    mugen::Avatar* m_avatar   = nullptr;
};

}  // namespace gameui
