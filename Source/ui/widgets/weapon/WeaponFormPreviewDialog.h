#pragma once

#include "ui/widgets/common/Dialog.h"
#include "fairygui/GLoader3D.h"
#include "mugen/avatar/render/Avatar.h"
#include "mugen/render/spine/MgSkeletonAnimation.h"

namespace gameui
{

// Popup: player Avatar + selected armament 神器 spine as sibling (heiyue UiGodWeaponRoleSpine /
// CreateUIRoleWeapon). Does not stuff spineId into FashionPosition::kWeapon — that spine is the
// artifact companion entity, not the occupational hand-weapon mesh.
class WeaponFormPreviewDialog : public Dialog
{
public:
    explicit WeaponFormPreviewDialog(int32_t armamentSpineId, std::string spineAnim = "stand");

protected:
    std::vector<std::string> getPackages() const override { return {"UI/Common_Base", "UI/Panel_Weapon"}; }

    GComponent* onCreateContent() override;

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickClose(EventContext* context);
    void refreshPreview();
    void clearPreview();

    int32_t m_armamentSpineId = 0;
    std::string m_spineAnim;
    GLoader3D* m_avatarLoader                 = nullptr;
    mugen::Avatar* m_avatar                   = nullptr;
    mugen::MgSkeletonAnimation* m_artifactSpine = nullptr;
};

}  // namespace gameui
