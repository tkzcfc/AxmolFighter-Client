#include "Hit.h"
#include "HitResolver.h"

namespace mugen::sim::hit
{
// 碰撞分派按目标类型与攻击者阵营过滤，End 事件不产生打击。
void onEffectCollision(World& world, Entity effectEntity, Entity target, CollisionEvent event, FixedVec2 contact)
{
    if (event == CollisionEvent::End)
        return;
    HitResolver(world, effectEntity, target, contact).resolve();
}
}  // namespace mugen::sim::hit
