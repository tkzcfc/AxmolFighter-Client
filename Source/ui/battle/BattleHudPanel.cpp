#include "ui/battle/BattleHudPanel.h"

#include "mugen/conf/ReflectedConfig.h"
#include "mugen/conf/TypeConversions.h"
#include "mugen/sim/buff/BuffPool.h"
#include "mugen/sim/input/ControlManagerCore.h"
#include "mugen/sim/skill/Skill.h"
#include "ui/battle/LocalBattleMode.h"
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

// 摇杆死区占半径的比例。
constexpr float kJoystickDeadZone = 0.25f;

// 技能池槽位字母与 ControlKey 一一对应（AttackA..AttackO, AttackQ；没有 P）。
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
    if (c == 'Q')
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

mugen::Entity localPlayer(const mugen::sim::World& world)
{
    const auto& slots = world.ecs().singleton<mugen::sim::PlayerSlots>().entities;
    const auto it     = slots.find(0);
    return it == slots.end() ? mugen::Entity{} : it->second;
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
    bindJoystick();
}

void BattleHudPanel::onDestroy()
{
    releaseJoystick();
    for (const auto& binding : m_skillButtons)
        if (!binding.m_poolSlot.empty())
            m_battle.setInput(binding.m_controlKey, false);
}

void BattleHudPanel::bindSkillButtons()
{
    const auto& world  = m_battle.world();
    const auto player  = localPlayer(world);
    const auto& ident  = world.ecs().get<mugen::sim::RoleIdentity>(player);
    const auto rows    = mugen::sim::skill::SkillAssembly::offlineKitRows(
        mugen::type_conversions::toCharacterClass(ident.roleId));
    auto findRow = [&rows](mugen::EnumSkillSlot slot) -> const mugen::sim::skill::OfflineKitRow* {
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
            const auto* attack = mugen::conf::Config::instance().find<mugen::conf::SkillAttackConfig>(row->skillId);
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

void BattleHudPanel::bindJoystick()
{
    m_joystick      = getChild<GComponent>("joystick");
    m_joystickThumb = m_joystick->getChild("thumb");
    m_thumbOrigin   = m_joystickThumb->getPosition();
    auto* area      = m_joystick->getChild("touchArea");
    area->addEventListener(UIEventType::TouchBegin, [this](EventContext* context) {
        if (m_joystickTouchId != -1)
            return;
        m_joystickTouchId = context->getInput()->getTouchId();
        context->captureTouch();
        updateJoystick(context->getInput()->getPosition());
    });
    area->addEventListener(UIEventType::TouchMove, [this](EventContext* context) {
        if (context->getInput()->getTouchId() == m_joystickTouchId)
            updateJoystick(context->getInput()->getPosition());
    });
    area->addEventListener(UIEventType::TouchEnd, [this](EventContext* context) {
        if (context->getInput()->getTouchId() == m_joystickTouchId)
            releaseJoystick();
    });
}

void BattleHudPanel::updateJoystick(const ax::Vec2& stagePos)
{
    const ax::Vec2 center = m_joystick->localToGlobal(m_thumbOrigin + m_joystickThumb->getSize() * 0.5f);
    const float radius    = m_joystick->getWidth() * 0.5f;
    ax::Vec2 delta        = m_joystick->globalToLocal(stagePos) - m_joystick->globalToLocal(center);
    if (delta.length() > radius)
        delta = delta.getNormalized() * radius;
    m_joystickThumb->setPosition(m_thumbOrigin.x + delta.x, m_thumbOrigin.y + delta.y);
    const float dead = radius * kJoystickDeadZone;
    // FairyGUI 的 y 轴向下，向上推对应 Up。
    setDirection(delta.x < -dead, delta.x > dead, delta.y < -dead, delta.y > dead);
}

void BattleHudPanel::releaseJoystick()
{
    m_joystickTouchId = -1;
    m_joystickThumb->setPosition(m_thumbOrigin.x, m_thumbOrigin.y);
    setDirection(false, false, false, false);
}

void BattleHudPanel::setDirection(bool left, bool right, bool up, bool down)
{
    const std::array<bool, 4> next = {left, right, up, down};
    const std::array<ControlKey, 4> keys = {ControlKey::Left, ControlKey::Right, ControlKey::Up, ControlKey::Down};
    for (size_t i = 0; i < next.size(); ++i)
    {
        if (next[i] == m_direction[i])
            continue;
        m_direction[i] = next[i];
        m_battle.setInput(static_cast<uint32_t>(keys[i]), next[i]);
    }
}

void BattleHudPanel::onClickPause(EventContext* /*context*/)
{
    m_battle.setPaused(true);
    MessageDialog::show("游戏已暂停", [this]() { m_battle.setPaused(false); });
}

void BattleHudPanel::onUpdate(float /*dt*/)
{
    const auto player = localPlayer(m_battle.world());
    if (!m_battle.world().ecs().alive(player))
        return;
    updatePlayer(player);
    updateSkills(player);
    updateBuffs(player);
    updateTarget(player);
}

void BattleHudPanel::updatePlayer(mugen::Entity player)
{
    const auto& ecs    = m_battle.world().ecs();
    const auto& vitals = ecs.get<mugen::sim::Vitals>(player);
    const auto& ident  = ecs.get<mugen::sim::RoleIdentity>(player);
    setProgress(m_playerHp, vitals.hp, vitals.maxHp);
    setProgress(m_playerMp, vitals.mp, vitals.maxMp);
    m_playerStatus->getChild("level")->setText(fmt::format("Lv.{}", ident.level));
    if (m_portraitRoleId == ident.roleId)
        return;
    m_portraitRoleId = ident.roleId;
    if (const auto* role = mugen::conf::Config::instance().find<mugen::conf::RoleConfig>(ident.roleId))
        m_playerStatus->getChild("portrait")->setIcon(role->headImage);
}

void BattleHudPanel::updateSkills(mugen::Entity player)
{
    const auto& pool = m_battle.world().ecs().get<mugen::sim::SkillPool>(player);
    for (auto& binding : m_skillButtons)
    {
        if (binding.m_poolSlot.empty())
            continue;
        const int32_t index = mugen::sim::skill::getGivenSkill(pool, binding.m_poolSlot);
        const bool cooling  = index != mugen::sim::kNoSkill && pool.m_skills.at(index).isCooling();
        binding.m_cdMask->setVisible(cooling);
        if (!cooling)
        {
            binding.m_cdText->setText("");
            continue;
        }
        const auto& skill    = pool.m_skills.at(index);
        const float remainMs = skill.m_cd.toFloatForRender();
        const float maxMs    = std::max(1.0f, skill.m_coldMaxTime.toFloatForRender());
        binding.m_cdMask->setFillAmount(std::clamp(remainMs / maxMs, 0.0f, 1.0f));
        binding.m_cdText->setText(remainMs >= 1000.0f ? fmt::format("{}", static_cast<int>(std::ceil(remainMs / 1000.0f)))
                                                      : fmt::format("{:.1f}", remainMs / 1000.0f));
    }
}

void BattleHudPanel::updateBuffs(mugen::Entity player)
{
    const auto& pool   = m_battle.world().ecs().get<mugen::sim::BuffPool>(player);
    const auto& config = mugen::conf::Config::instance();
    int count          = 0;
    for (const auto& instance : pool.m_instances)
        if (instance.m_lifecycle.m_active && !instance.m_lifecycle.m_exit)
            ++count;
    while (m_buffList->numChildren() < count)
        m_buffList->addItemFromPool();
    while (m_buffList->numChildren() > count)
        m_buffList->removeChildToPoolAt(m_buffList->numChildren() - 1);
    int index = 0;
    for (const auto& instance : pool.m_instances)
    {
        if (!instance.m_lifecycle.m_active || instance.m_lifecycle.m_exit)
            continue;
        auto* item = m_buffList->getChildAt(index++)->as<GComponent>();
        if (const auto* buff = config.find<mugen::conf::BuffConfig>(instance.m_id))
            item->getChild("icon")->setIcon(configIconString(buff->icon));
        item->getChild("time")->setText(instance.m_lifecycle.m_repeat > 1
                                            ? fmt::format("{}", instance.m_lifecycle.m_repeat)
                                            : std::string());
    }
}

void BattleHudPanel::updateTarget(mugen::Entity /*player*/)
{
    const auto& ecs     = m_battle.world().ecs();
    const auto target   = m_battle.lastHurtTarget();
    const auto* vitals  = ecs.tryGet<mugen::sim::Vitals>(target);
    const auto* ident   = ecs.tryGet<mugen::sim::RoleIdentity>(target);
    const bool hasTarget = vitals && ident && !vitals->isDead();
    const bool isBoss    = hasTarget && ident->roleType == static_cast<int32_t>(mugen::EntityRoleType::kBoss);
    m_targetBar->setVisible(hasTarget && !isBoss);
    m_bossBar->setVisible(isBoss);
    if (!hasTarget)
        return;
    GComponent* bar = isBoss ? m_bossBar : m_targetBar;
    setProgress(bar->getChild("hpBar")->as<GProgressBar>(), vitals->hp, vitals->maxHp);
    bar->getChild("level")->setText(fmt::format("Lv.{}", ident->level));
    if (m_targetRoleId == ident->roleId)
        return;
    m_targetRoleId = ident->roleId;
    bar->getChild("name")->setText(fmt::format("#{}", ident->roleId));
    if (isBoss)
        if (const auto* role = mugen::conf::Config::instance().find<mugen::conf::RoleConfig>(ident->roleId))
            bar->getChild("portrait")->setIcon(role->headImage);
}

}  // namespace gameui
