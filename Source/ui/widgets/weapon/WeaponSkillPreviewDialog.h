#pragma once

#include "ui/widgets/common/Dialog.h"
#include "ui/UIMediaPlayer.h"

#include <string>

namespace gameui
{

// Popup that plays the armament skill preview video (heiyue UiGodWeaponPreview).
class WeaponSkillPreviewDialog : public Dialog
{
public:
    explicit WeaponSkillPreviewDialog(std::string videoPath);

protected:
    std::vector<std::string> getPackages() const override { return {"UI/Common_Base", "UI/Panel_Weapon"}; }

    GComponent* onCreateContent() override
    {
        return createCenteredComponent("Panel_Weapon", "WeaponSkillPreviewDialog");
    }

    void onCreate() override;
    void onShow() override;
    void onDestroy() override;

private:
    void onClickClose(EventContext* context);

    std::string m_videoPath;
    ax::ui::MediaPlayer* m_mediaPlayer = nullptr;
};

}  // namespace gameui
