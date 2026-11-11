#include "DebugDraw.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

namespace
{
// 受击框调试色；引擎没有预置青色常量。
const ax::Color4F kHurtBoxColor{0.0f, 1.0f, 1.0f, 1.0f};
float renderValue(Fixed value)
{
    return value.toFloatForRender();
}
}  // namespace

DebugDraw::DebugDraw(ax::Node& root)
{
    m_node = ax::DrawNode::create();
    root.addChild(m_node, 300);
}

void DebugDraw::present(sim_legacy::World& world)
{
    m_node->clear();
    if (!m_enabled)
        return;
    // WorldBox 已是世界坐标，pos 为 x/z 平面最小角，与碰撞判定的轴对齐框一致
    const auto drawBoxes = [this](const std::vector<sim_legacy::WorldBox>& boxes, const ax::Color4F& color) {
        for (const auto& box : boxes)
            m_node->drawRect({renderValue(box.pos.x), renderValue(box.pos.z)},
                             {renderValue(box.pos.x + box.size.x), renderValue(box.pos.z + box.size.z)}, color);
    };
    world.ecs().each<sim_legacy::Transform, sim_legacy::ColliderState>(
        [&](Entity, const sim_legacy::Transform& transform, const sim_legacy::ColliderState& collider) {
        const ax::Vec2 center{renderValue(transform.position.x), renderValue(transform.position.z)};
        m_node->drawCircle(center, renderValue(collider.radius), 0.0f, 16, false, ax::Color4F::YELLOW);
        drawBoxes(collider.attack, ax::Color4F::RED);
        drawBoxes(collider.damage, kHurtBoxColor);
    });
    for (const auto& range : world.ecs().singleton<sim_legacy::MapRegion>().moveRanges)
        m_node->drawRect({renderValue(range.x), renderValue(range.y)},
                         {renderValue(range.x + range.width), renderValue(range.y + range.height)}, ax::Color4F::GREEN);
}

NS_MG_END

#endif
