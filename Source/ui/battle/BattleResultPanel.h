#pragma once

#include "ui/battle/BattleBootParams.h"
#include "ui/widgets/common/Panel.h"

#include <functional>

namespace gameui
{

// 副本结算：胜负、副本、用时、连击、评价，以及返回城镇。失败时可以再打同一场。
class BattleResultPanel : public Panel
{
public:
    BattleResultPanel(DungeonBattleResult result, std::function<void()> onReturn, std::function<void()> onRetry);

protected:
    std::vector<std::string> getPackages() const override { return {"UI/Common_Base", "UI/Screen_Battle"}; }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Screen_Battle", "BattleResultPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onShow() override;
    void onStopAnimations() override;

private:
    void onClickReturn(EventContext* context);
    void onClickRetry(EventContext* context);

    DungeonBattleResult m_result;
    std::function<void()> m_onReturn;
    std::function<void()> m_onRetry;
};

}  // namespace gameui
