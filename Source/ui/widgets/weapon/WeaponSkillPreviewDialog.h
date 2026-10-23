#pragma once

#include "ui/widgets/common/Dialog.h"
#include "ui/UIMediaPlayer.h"
#include "fairygui/GLoader3D.h"

#include <string>

namespace gameui
{

// Popup that plays the armament skill preview video (heiyue UiGodWeaponPreview).
// Mirrors CharacterCreationPanel: MediaPlayer hosted inside a FairyGUI GLoader3D.
class WeaponSkillPreviewDialog : public Dialog
{
public:
    explicit WeaponSkillPreviewDialog(std::string videoPath);

protected:
    std::vector<std::string> getPackages() const override { return {"UI/Common_Base", "UI/Panel_Weapon"}; }

    GComponent* onCreateContent() override;

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickClose(EventContext* context);
    void playVideo();

    std::string m_videoPath;
    GLoader3D* m_loaderVideo           = nullptr;
    ax::ui::MediaPlayer* m_mediaPlayer = nullptr;
};

}  // namespace gameui
