#include "DebugDraw.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/battle/AvatarPresenter.h"

NS_MG_BEGIN

namespace
{
// 受击框调试色；引擎没有预置青色常量。
const ax::Color4F kHurtBoxColor{0.0f, 1.0f, 1.0f, 1.0f};
const ax::Color4F kAttackFill{1.0f, 0.0f, 0.0f, 0.15f};
const ax::Color4F kHurtFill{0.0f, 1.0f, 1.0f, 0.15f};
float renderValue(Fixed value)
{
    return value.toFloatForRender();
}
// 编辑器同一套斜二测：深度相对实体纵深投影到右上 45° ×0.5。
// 屏幕纵坐标加上实体纵深，与角色节点的 position.y + position.z 对齐。
void drawCombatBox(ax::DrawNode& node, const sim_legacy::WorldBox& box, float entityDepth, const VisualPos& shift,
                   const ax::Color4F& border, const ax::Color4F& fill)
{
    constexpr float oblique = 0.5f;
    const float x0          = renderValue(box.pos.x) + shift.x;
    const float x1          = x0 + renderValue(box.size.x);
    const float depth0      = renderValue(box.pos.y) + shift.z;
    const float depth1      = depth0 + renderValue(box.size.y);
    const float height0     = renderValue(box.pos.z) + shift.y;
    const float height1     = height0 + renderValue(box.size.z);
    auto project            = [&](float x, float depth, float height) {
        const float shear = (depth - entityDepth) * oblique;
        return ax::Vec2{x + shear, height + entityDepth + shear};
    };
    const ax::Vec2 frontLeft      = project(x0, depth0, height0);
    const ax::Vec2 frontRight     = project(x1, depth0, height0);
    const ax::Vec2 frontRightTop  = project(x1, depth0, height1);
    const ax::Vec2 frontLeftTop   = project(x0, depth0, height1);
    const ax::Vec2 backRight      = project(x1, depth1, height0);
    const ax::Vec2 backRightTop   = project(x1, depth1, height1);
    const ax::Vec2 backLeftTop    = project(x0, depth1, height1);
    const ax::Color4F side(fill.r * 0.8f, fill.g * 0.8f, fill.b * 0.8f, fill.a);
    ax::Vec2 top[]   = {frontLeftTop, frontRightTop, backRightTop, backLeftTop};
    ax::Vec2 right[] = {frontRight, backRight, backRightTop, frontRightTop};
    ax::Vec2 front[] = {frontLeft, frontRight, frontRightTop, frontLeftTop};
    node.drawSolidPoly(top, 4, side, 1.0f, border);
    node.drawSolidPoly(right, 4, side, 1.0f, border);
    node.drawSolidPoly(front, 4, fill, 1.0f, border);
}
}  // namespace

DebugDraw::DebugDraw(ax::Node& root)
{
    m_node = ax::DrawNode::create();
    root.addChild(m_node, 300);
}

void DebugDraw::present(sim_legacy::World& world, const AvatarPresenter* avatars, float alpha)
{
    m_node->clear();
    if (!m_enabled)
        return;
    world.ecs().each<sim_legacy::Transform, sim_legacy::ColliderState>(
        [&](Entity entity, const sim_legacy::Transform& transform, const sim_legacy::ColliderState& collider) {
        VisualPos shift;
        if (avatars)
            avatars->renderOffset(entity.value, alpha, &shift);
        const float entityDepth = renderValue(transform.position.z) + shift.z;
        const ax::Vec2 center{renderValue(transform.position.x) + shift.x,
                              renderValue(transform.position.y) + shift.y + entityDepth};
        m_node->drawCircle(center, renderValue(collider.radius), 0.0f, 16, false, ax::Color4F::YELLOW);
        for (const auto& box : collider.attack)
            drawCombatBox(*m_node, box, entityDepth, shift, ax::Color4F::RED, kAttackFill);
        for (const auto& box : collider.damage)
            drawCombatBox(*m_node, box, entityDepth, shift, kHurtBoxColor, kHurtFill);
    });
    for (const auto& range : world.ecs().singleton<sim_legacy::MapRegion>().moveRanges)
        m_node->drawRect({renderValue(range.x), renderValue(range.y)},
                         {renderValue(range.x + range.width), renderValue(range.y + range.height)}, ax::Color4F::GREEN);
}

NS_MG_END

#endif
