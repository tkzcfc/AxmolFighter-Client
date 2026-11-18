#include "ui/battle/BattleHudPanel.h"

#include "mugen/conf/BattleConst.h"
#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/sim/view/HudView.h"
#include "ui/battle/BattleMode.h"
#include "ui/widgets/common/MessageDialog.h"

#include <algorithm>
#include <cmath>
#include <variant>

namespace gameui
{
namespace
{
using mugen::sim::ControlKey;

// Unwrap config icon variant; tables already store FairyGUI ui:// URLs (or int sentinel).
std::string configIconString(const std::variant<int32_t, std::string>& icon)
{
    if (const auto* path = std::get_if<std::string>(&icon))
        return *path;
    return {};
}

// 技能池槽位字母与 ControlKey 一一对应（AttackA..AttackO 对应 A..O，AttackQ 对应 P）。
bool poolSlotToControlKey(const std::string& poolSlot, uint32_t& key)
{
    if (poolSlot.size() != 1)
        return false;
    const char c = poolSlot[0];
    if (c >= 'A' && c <= 'O')
    {
        key = static_cast<uint32_t>(ControlKey::AttackA) + static_cast<uint32_t>(c - 'A');
        return true;
    }
    if (c == 'P' || c == 'Q')
    {
        key = static_cast<uint32_t>(ControlKey::AttackQ);
        return true;
    }
    return false;
}

void setProgress(GProgressBar* bar, mugen::Fixed value, mugen::Fixed max)
{
    bar->setMax(std::max(1.0, static_cast<double>(max.toFloatForRender())));
    bar->setValue(std::max(0.0, static_cast<double>(value.toFloatForRender())));
}

void styleRageBar(GProgressBar* bar, bool transformed)
{
    if (auto* ctrl = bar->getController("mode"))
        ctrl->setSelectedIndex(transformed ? 1 : 0);
    if (auto* barObj = bar->getChild("bar"))
        if (auto* barImg = dynamic_cast<GImage*>(barObj))
            barImg->setColor(transformed ? ax::Color3B(212, 107, 255) : ax::Color3B(255, 157, 46));
    if (auto* titleObj = bar->getChild("title"))
        if (auto* title = dynamic_cast<GTextField*>(titleObj))
            title->setColor(transformed ? ax::Color3B(243, 217, 255) : ax::Color3B(255, 229, 184));
}

}  // namespace

void BattleHudPanel::onCreate()
{
    m_playerStatus = getChild<GComponent>("playerStatus");
    m_playerHp     = m_playerStatus->getChild("hpBar")->as<GProgressBar>();
    m_playerMp     = m_playerStatus->getChild("mpBar")->as<GProgressBar>();

    auto* container = getChild<GLoader>("container")->getComponent();
    m_targetBar     = container->getChild("targetBar")->as<GComponent>();
    m_bossBar       = container->getChild("bossBar")->as<GComponent>();
    m_buffList      = container->getChild("buffList")->as<GList>();
    m_targetBar->setVisible(false);
    m_bossBar->setVisible(false);
    m_buffList->removeChildrenToPool();

    addClickListener(getChild<GButton>("pauseButton"), AX_CALLBACK_1(BattleHudPanel::onClickPause, this));
    bindSkillButtons();
    bindRageHud();
    bindJoystick();
}

void BattleHudPanel::onDestroy()
{
    m_joystick.release();
    for (const auto& binding : m_skillButtons)
        if (!binding.m_poolSlot.empty())
            m_battle.setInput(binding.m_controlKey, false);
    if (!m_rageSkillButton.m_poolSlot.empty())
        m_battle.setInput(m_rageSkillButton.m_controlKey, false);
}

void BattleHudPanel::bindSkillButtons()
{
    const auto& world = m_battle.world();
    const auto player = m_battle.localPlayer();
    const auto hud  = mugen::sim::view::readPlayerHud(world, player);
    const auto rows = mugen::sim::view::offlineKitRows(mugen::type_conversions::toCharacterClass(hud.roleId));
    auto findRow    = [&rows](mugen::EnumSkillSlot slot) -> const mugen::sim::view::OfflineKitRow* {
        for (const auto& row : rows)
            if (row.slot == slot)
                return &row;
        return nullptr;
    };

    using Slot = mugen::EnumSkillSlot;
    const std::array<std::pair<const char*, Slot>, 9> layout = {{{"attack", Slot::Attack},
                                                                 {"jump", Slot::Sprint},
                                                                 {"dodge", Slot::Dodge},
                                                                 {"skill0", Slot::A},
                                                                 {"skill1", Slot::B},
                                                                 {"skill2", Slot::C},
                                                                 {"skill3", Slot::EA},
                                                                 {"skill4", Slot::EB},
                                                                 {"skill5", Slot::SUA}}};
    auto* pad = getChild<GComponent>("skillPad");
    for (const auto& [name, slot] : layout)
    {
        SkillButtonBinding binding;
        binding.m_button = pad->getChild(name)->as<GButton>();
        binding.m_cdMask = binding.m_button->getChild("cdMask")->as<GLoader>();
        binding.m_cdText = binding.m_button->getChild("cd")->as<GTextField>();
        binding.m_cdMask->setVisible(false);
        binding.m_cdText->setText("");
        const auto* row = findRow(slot);
        if (row && poolSlotToControlKey(row->poolSlot, binding.m_controlKey))
        {
            binding.m_poolSlot = row->poolSlot;
            const auto* attack = mugen::conf::Config::instance().find<mugen::conf::battle::SkillDef>(row->skillId);
            const std::string url = attack ? configIconString(attack->icon) : std::string();
            // 闪避按钮在原版使用固定的 HUD 图标，配置图标缺失时保留组件默认图标。
            if (!url.empty())
                binding.m_button->setIcon(url);
        }
        else
        {
            binding.m_button->setGrayed(true);
            binding.m_button->setTouchable(false);
        }
        m_skillButtons.push_back(binding);
    }
    for (auto& binding : m_skillButtons)
        if (!binding.m_poolSlot.empty())
            bindSkillButton(binding);
}

void BattleHudPanel::bindSkillButton(SkillButtonBinding& binding)
{
    const uint32_t key = binding.m_controlKey;
    binding.m_button->addEventListener(UIEventType::TouchBegin, [this, key](EventContext* context) {
        context->captureTouch();
        m_battle.setInput(key, true);
    });
    binding.m_button->addEventListener(UIEventType::TouchEnd,
                                       [this, key](EventContext*) { m_battle.setInput(key, false); });
}

void BattleHudPanel::bindRageHud()
{
    auto* pad     = getChild<GComponent>("skillPad");
    auto* rageBar = dynamic_cast<GProgressBar*>(m_playerStatus->getChild("rageBar"));
    auto* rageBtn = dynamic_cast<GButton*>(pad->getChild("rageSkill"));
    if (!rageBar || !rageBtn)
    {
        static bool s_warnedMissingRageHud = false;
        if (!s_warnedMissingRageHud)
        {
            s_warnedMissingRageHud = true;
            AXLOGW("BattleHudPanel: rageBar or rageSkill is missing in published Screen_Battle.fui; skipping partner rage HUD");
        }
        if (rageBar)
            rageBar->setVisible(false);
        if (rageBtn)
            rageBtn->setVisible(false);
        return;
    }

    m_playerRage               = rageBar;
    m_rageSkillButton          = {};
    m_rageSkillButton.m_button = rageBtn;
    m_rageSkillButton.m_cdMask = dynamic_cast<GLoader*>(rageBtn->getChild("cdMask"));
    m_rageSkillButton.m_cdText = dynamic_cast<GTextField*>(rageBtn->getChild("cd"));
    if (m_rageSkillButton.m_cdMask)
        m_rageSkillButton.m_cdMask->setVisible(false);
    if (m_rageSkillButton.m_cdText)
        m_rageSkillButton.m_cdText->setText("");

    const auto& world = m_battle.world();
    const auto player = m_battle.localPlayer();
    const auto rageHud = mugen::sim::view::readPartnerRageHud(world, player);
    const bool hasRage = rageHud.hasRage;
    m_playerRage->setVisible(hasRage);
    rageBtn->setVisible(hasRage);
    if (!hasRage)
        return;

    const auto hud  = mugen::sim::view::readPlayerHud(world, player);
    const auto rows = mugen::sim::view::offlineKitRows(mugen::type_conversions::toCharacterClass(hud.roleId));
    for (const auto& row : rows)
    {
        if (row.slot != mugen::EnumSkillSlot::Anger)
            continue;
        if (poolSlotToControlKey(row.poolSlot, m_rageSkillButton.m_controlKey))
        {
            m_rageSkillButton.m_poolSlot = row.poolSlot;
            const auto* attack = mugen::conf::Config::instance().find<mugen::conf::battle::SkillDef>(row.skillId);
            const std::string url = attack ? configIconString(attack->icon) : std::string();
            if (!url.empty())
                rageBtn->setIcon(url);
        }
        break;
    }
    if (m_rageSkillButton.m_poolSlot.empty())
    {
        m_rageSkillButton.m_poolSlot   = "P";
        m_rageSkillButton.m_controlKey = static_cast<uint32_t>(ControlKey::AttackQ);
    }

    const uint32_t key = m_rageSkillButton.m_controlKey;
    rageBtn->addEventListener(UIEventType::TouchBegin, [this, key](EventContext* context) {
        if (!m_rageSkillEnabled)
            return;
        context->captureTouch();
        m_battle.setInput(key, true);
    });
    rageBtn->addEventListener(UIEventType::TouchEnd, [this, key](EventContext*) { m_battle.setInput(key, false); });

    updateRage(player);
}

void BattleHudPanel::bindJoystick()
{
    auto* joystick = dynamic_cast<GComponent*>(getContent()->getChild("joystick"));
    if (!m_joystick.bind(joystick, [this](ControlKey key, bool pressed) {
            m_battle.setInput(static_cast<uint32_t>(key), pressed);
        }))
    {
        static bool s_warnedMissingJoystick = false;
        if (!s_warnedMissingJoystick)
        {
            s_warnedMissingJoystick = true;
            AXLOGW("BattleHudPanel: joystick is missing in published Screen_Battle.fui; skipping virtual joystick");
        }
    }
}

void BattleHudPanel::onClickPause(EventContext* /*context*/)
{
    m_battle.setPaused(true);
    MessageDialog::show("游戏已暂停", [this]() { m_battle.setPaused(false); });
}

void BattleHudPanel::onUpdate(float /*dt*/)
{
    const auto player = m_battle.localPlayer();
    if (!mugen::sim::view::readPlayerHud(m_battle.world(), player).valid)
        return;
    updatePlayer(player);
    updateRage(player);
    updateSkills(player);
    updateBuffs(player);
    updateTarget(player);
}

void BattleHudPanel::updatePlayer(mugen::Entity player)
{
    const auto hud = mugen::sim::view::readPlayerHud(m_battle.world(), player);
    setProgress(m_playerHp, hud.hp, hud.maxHp);
    setProgress(m_playerMp, hud.mp, hud.maxMp);
    m_playerStatus->getChild("level")->setText(fmt::format("Lv.{}", hud.level));
    if (m_portraitRoleId == hud.roleId)
        return;
    m_portraitRoleId = hud.roleId;
    if (const auto* role = mugen::conf::Config::instance().find<mugen::conf::battle::RoleDef>(hud.roleId))
        m_playerStatus->getChild("portrait")->setIcon(role->headImage);
}

void BattleHudPanel::updateRage(mugen::Entity player)
{
    if (!m_playerRage || !m_rageSkillButton.m_button)
        return;

    const auto rage    = mugen::sim::view::readPartnerRageHud(m_battle.world(), player);
    const bool hasRage = rage.hasRage;
    m_playerRage->setVisible(hasRage);
    m_rageSkillButton.m_button->setVisible(hasRage);
    if (!hasRage)
    {
        if (m_rageSkillEnabled)
        {
            m_rageSkillEnabled = false;
            m_battle.setInput(m_rageSkillButton.m_controlKey, false);
        }
        return;
    }

    const bool draining = rage.freezeMs > mugen::Fixed{};
    if (draining)
        setProgress(m_playerRage, rage.freezeMs, rage.maxFreezeMs);
    else
        setProgress(m_playerRage, rage.anger, mugen::conf::kPartnerRageMaxAnger);
    styleRageBar(m_playerRage, draining || rage.transformed);

    const bool ready =
        !rage.transformed && !rage.recoverPending && rage.anger >= mugen::conf::kPartnerRageMaxAnger;
    if (!ready && m_rageSkillEnabled)
        m_battle.setInput(m_rageSkillButton.m_controlKey, false);
    m_rageSkillEnabled = ready;
    m_rageSkillButton.m_button->setGrayed(!ready);
    m_rageSkillButton.m_button->setTouchable(ready);
    if (auto* bgObj = m_rageSkillButton.m_button->getChild("bg"))
        if (auto* bgImg = dynamic_cast<GImage*>(bgObj))
            bgImg->setColor(ready ? ax::Color3B(255, 204, 77) : ax::Color3B::WHITE);
}

void BattleHudPanel::updateSkills(mugen::Entity player)
{
    for (auto& binding : m_skillButtons)
    {
        if (binding.m_poolSlot.empty())
            continue;
        const auto cooldown = mugen::sim::view::readSlotCooldown(m_battle.world(), player, binding.m_poolSlot);
        binding.m_cdMask->setVisible(cooldown.cooling);
        if (!cooldown.cooling)
        {
            binding.m_cdText->setText("");
            continue;
        }
        const float remainMs = cooldown.remainMs.toFloatForRender();
        const float maxMs    = std::max(1.0f, cooldown.maxMs.toFloatForRender());
        binding.m_cdMask->setFillAmount(std::clamp(remainMs / maxMs, 0.0f, 1.0f));
        binding.m_cdText->setText(remainMs >= 1000.0f ? fmt::format("{}", static_cast<int>(std::ceil(remainMs / 1000.0f)))
                                                      : fmt::format("{:.1f}", remainMs / 1000.0f));
    }
}

void BattleHudPanel::updateBuffs(mugen::Entity player)
{
    std::vector<mugen::sim::view::ActiveBuffView> buffs;
    mugen::sim::view::collectActiveBuffs(m_battle.world(), player, buffs);
    const auto& config = mugen::conf::Config::instance();
    const int count    = static_cast<int>(buffs.size());
    while (m_buffList->numChildren() < count)
        m_buffList->addItemFromPool();
    while (m_buffList->numChildren() > count)
        m_buffList->removeChildToPoolAt(m_buffList->numChildren() - 1);
    int index = 0;
    for (const auto& instance : buffs)
    {
        auto* item = m_buffList->getChildAt(index++)->as<GComponent>();
        if (const auto* buff = config.find<mugen::conf::battle::BuffDef>(instance.buffId))
            item->getChild("icon")->setIcon(configIconString(buff->presentation.icon));
        item->getChild("time")->setText(instance.stacks > 1 ? fmt::format("{}", instance.stacks) : std::string());
    }
}

void BattleHudPanel::updateTarget(mugen::Entity /*player*/)
{
    const auto target = m_battle.lastHurtTarget();
    const auto barView   = mugen::sim::view::readTargetBar(m_battle.world(), target);
    const bool hasTarget = barView.valid;
    const bool isBoss    = hasTarget && barView.roleType == static_cast<int32_t>(mugen::EntityRoleType::kBoss);
    m_targetBar->setVisible(hasTarget && !isBoss);
    m_bossBar->setVisible(isBoss);
    if (!hasTarget)
        return;
    GComponent* bar = isBoss ? m_bossBar : m_targetBar;
    setProgress(bar->getChild("hpBar")->as<GProgressBar>(), barView.hp, barView.maxHp);
    bar->getChild("level")->setText(fmt::format("Lv.{}", barView.level));
    if (m_targetRoleId == barView.roleId)
        return;
    m_targetRoleId = barView.roleId;
    bar->getChild("name")->setText(fmt::format("#{}", barView.roleId));
    if (isBoss)
        if (const auto* role = mugen::conf::Config::instance().find<mugen::conf::battle::RoleDef>(barView.roleId))
            bar->getChild("portrait")->setIcon(role->headImage);
}

}  // namespace gameui
