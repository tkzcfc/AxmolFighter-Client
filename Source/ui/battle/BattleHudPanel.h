#pragma once

#include "ui/widgets/common/Panel.h"
#include "ui/widgets/common/VirtualJoystick.h"
#include "mugen/core/ecs/Entity.h"

#include <array>
#include <string>
#include <vector>

namespace gameui
{

class IBattleMode;

// 战斗内 HUD：玩家状态、技能按钮与冷却、摇杆、目标/首领血条、Buff 图标和暂停按钮。
// 每帧只读取战斗世界数据，输入通过 IBattleMode::setInput 提交，与键盘输入同一通路。
class BattleHudPanel : public Panel
{
public:
    explicit BattleHudPanel(IBattleMode& battle) : m_battle(battle) {}

protected:
    std::vector<std::string> getPackages() const override
    {
        return {"UI/Common_Base", "UI/Res_SkillIcons", "UI/Res_BuffIcons", "UI/Screen_Battle"};
    }

    GComponent* onCreatePanelContent() override
    {
        return UIPackage::createObject("Screen_Battle", "BattleHudPanel")->as<GComponent>();
    }

    void onCreate() override;
    void onUpdate(float dt) override;
    void onDestroy() override;

private:
    struct SkillButtonBinding
    {
        GButton* m_button = nullptr;
        GLoader* m_cdMask = nullptr;
        GTextField* m_cdText = nullptr;
        // 技能池槽位字符串（单个字母 A..Q），为空表示该按钮未绑定技能。
        std::string m_poolSlot;
        uint32_t m_controlKey = 0;
    };

    void bindSkillButtons();
    void bindSkillButton(SkillButtonBinding& binding);
    void bindRageHud();
    void bindJoystick();
    void onClickPause(EventContext* context);

    void updatePlayer(mugen::Entity player);
    void updateRage(mugen::Entity player);
    void updateSkills(mugen::Entity player);
    void updateBuffs(mugen::Entity player);
    void updateTarget(mugen::Entity player);

    IBattleMode& m_battle;

    GComponent* m_playerStatus = nullptr;
    GProgressBar* m_playerHp = nullptr;
    GProgressBar* m_playerMp = nullptr;
    GProgressBar* m_playerRage = nullptr;
    GComponent* m_targetBar = nullptr;
    GComponent* m_bossBar = nullptr;
    GList* m_buffList = nullptr;

    VirtualJoystick m_joystick;

    std::vector<SkillButtonBinding> m_skillButtons;
    SkillButtonBinding m_rageSkillButton;
    bool m_rageSkillEnabled = false;
    int32_t m_portraitRoleId = 0;
    int32_t m_targetRoleId = 0;
};

}  // namespace gameui
