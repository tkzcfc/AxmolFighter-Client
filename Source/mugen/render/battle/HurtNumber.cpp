#include "HurtNumber.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/sim_legacy/role/RoleComponents.h"

#    include <algorithm>

NS_MG_BEGIN

namespace
{
float renderValue(Fixed value)
{
    return value.toFloatForRender();
}

// 飘字位图字体文件，下标与 HurtNumber::Face 一致。
constexpr const char* kHurtFontFiles[] = {
    "/mugen/font/hurt_normal.fnt",
    "/mugen/font/hurt_hero.fnt",
    "/mugen/font/hurt_crit.fnt",
    "/mugen/font/hurt_word.fnt",
};
// 文字字体中的字符：M 为未命中、D 为闪避。
constexpr const char* kMissGlyph  = "M";
constexpr const char* kDodgeGlyph = "D";
// 无接触点时飘字锚点相对实体纵深位置抬高的像素。
constexpr float kHurtHeadHeight = 220.0f;
// 文字相对数字锚点上抬的像素。
constexpr float kHurtWordRise = 50.0f;
// 动画步长（毫秒）与总步数；一步对应一次缩放/上升/淡出更新。
constexpr float kHurtStepMs = 50.0f;
constexpr int kHurtStepCount = 20;
// 第 13 步起开始上升淡出；淡出每步减少的不透明度与上升像素。
constexpr int kHurtFadeBeginStep = 13;
constexpr int kHurtFadeAlphaStep = 37;
constexpr float kHurtFadeRise    = 15.0f;
// 同时存在的飘字上限，超出时回收最早的。
constexpr size_t kHurtMaxActive = 64;
// 随机偏移的最大像素。
constexpr int kHurtRandomRange = 30;
}  // namespace

void HurtNumber::consume(const sim_legacy::PresentationEvent& event, sim_legacy::World& world)
{
    auto& ecs = world.ecs();
    if (const auto* tips = std::get_if<sim_legacy::Tips>(&event.payload))
    {
        // 闪避/未命中由逻辑侧以固定文本提示，数字以外的飘字走文字字体。
        const char* glyph = tips->text == "MISS" ? kMissGlyph : tips->text == "DODGE" ? kDodgeGlyph : nullptr;
        const auto* transform = ecs.tryGet<sim_legacy::Transform>(event.entity);
        if (!glyph || !transform)
            return;
        spawn(Face::kWord, glyph, renderValue(transform->position.x),
              renderValue(transform->position.y + transform->position.z) + kHurtHeadHeight);
        return;
    }
    const auto* hurt = std::get_if<sim_legacy::HurtNumber>(&event.payload);
    if (!hurt)
        return;
    float x = 0.0f;
    float y = 0.0f;
    if (hurt->m_hasContact)
    {
        x = renderValue(hurt->m_contact.x);
        y = renderValue(hurt->m_contact.y);
    }
    else
    {
        const auto* transform = ecs.tryGet<sim_legacy::Transform>(event.entity);
        if (!transform)
            return;
        x = renderValue(transform->position.x);
        y = renderValue(transform->position.y + transform->position.z) + kHurtHeadHeight;
    }
    // 暴击优先；否则敌方打到玩家方用受击字体，其余用普通字体。
    Face face = Face::kNormal;
    if (hurt->m_critical)
        face = Face::kCrit;
    else if (const auto* identity = ecs.tryGet<sim_legacy::RoleIdentity>(event.entity))
    {
        const bool enemyCamp = identity->camp == static_cast<int32_t>(EntityCamp::Enemy) ||
                               identity->camp == static_cast<int32_t>(EntityCamp::EnemyFriend);
        if (identity->roleType == static_cast<int32_t>(EntityRoleType::kHero) && !enemyCamp)
            face = Face::kHero;
    }
    // 位图字体没有负号，持续回复按数值绝对值显示。
    spawn(face, std::to_string(hurt->value < 0 ? -hurt->value : hurt->value), x, y);
}

float HurtNumber::randomOffset()
{
    std::uniform_int_distribution<int> magnitude(0, kHurtRandomRange);
    const int value = magnitude(m_random);
    return static_cast<float>((m_random() & 1u) ? value : -value);
}

ax::Label* HurtNumber::acquireLabel(Face face)
{
    auto& pool = m_pool[static_cast<size_t>(face)];
    if (!pool.empty())
    {
        ax::Label* label = pool.back();
        pool.pop_back();
        label->setVisible(true);
        return label;
    }
    auto* label = ax::Label::createWithBMFont(kHurtFontFiles[static_cast<size_t>(face)], "");
    m_root.addChild(label, 200);
    return label;
}

void HurtNumber::spawn(Face face, const std::string& text, float x, float y)
{
    if (m_active.size() >= kHurtMaxActive)
    {
        recycle(m_active.front());
        m_active.erase(m_active.begin());
    }
    Item item;
    item.face  = face;
    item.x     = x + randomOffset();
    item.y     = y + randomOffset();
    item.label = acquireLabel(face);
    item.label->setString(text);
    item.label->setScale(1.0f);
    item.label->setOpacity(255);
    item.label->setPosition(item.x, item.y + (face == Face::kWord ? kHurtWordRise : 0.0f));
    m_active.push_back(item);
}

void HurtNumber::applyStep(Item& item)
{
    const bool word = item.face == Face::kWord;
    // 出现阶段缩放曲线（前 5 步），之后恒为 1；数字比文字起点更大。
    static constexpr float kNumberScale[] = {3.0f, 2.0f, 1.4f, 0.8f, 0.6f};
    static constexpr float kWordScale[]   = {2.0f, 1.4f, 1.4f, 0.8f, 0.6f};
    const int step = item.step;
    if (step < 5)
    {
        item.label->setScale(word ? kWordScale[step] : kNumberScale[step]);
        // 数字第 0 步半透明闪入，第 1 步起全不透明。
        if (!word && step == 0)
            item.label->setOpacity(107);
        else if (!word && step == 1)
            item.label->setOpacity(255);
    }
    else if (step < kHurtFadeBeginStep)
        item.label->setScale(1.0f);
    else
    {
        item.alpha = std::max(0, item.alpha - kHurtFadeAlphaStep);
        item.y += kHurtFadeRise;
        item.label->setPosition(item.x, item.y + (word ? kHurtWordRise : 0.0f));
        item.label->setOpacity(static_cast<uint8_t>(item.alpha));
    }
}

void HurtNumber::update(float delta)
{
    for (auto& item : m_active)
    {
        item.carryMs += delta * 1000.0f;
        while (item.carryMs >= kHurtStepMs && item.step < kHurtStepCount)
        {
            applyStep(item);
            item.carryMs -= kHurtStepMs;
            ++item.step;
        }
    }
    for (auto it = m_active.begin(); it != m_active.end();)
    {
        if (it->step >= kHurtStepCount)
        {
            recycle(*it);
            it = m_active.erase(it);
        }
        else
            ++it;
    }
}

void HurtNumber::recycle(Item& item)
{
    item.label->setVisible(false);
    m_pool[static_cast<size_t>(item.face)].push_back(item.label);
    item.label = nullptr;
}

void HurtNumber::clear()
{
    for (auto& item : m_active)
        item.label->removeFromParent();
    m_active.clear();
    for (auto& pool : m_pool)
    {
        for (ax::Label* label : pool)
            label->removeFromParent();
        pool.clear();
    }
}

NS_MG_END

#endif
