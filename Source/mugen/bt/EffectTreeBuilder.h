#pragma once

#include "mugen/core/bt/BTNode.h"
#include "mugen/core/ecs/Entity.h"

NS_MG_BEGIN

class BehaviorTreeComponent;

// 特效实体行为树：action_ids 查特效动作表挂 AttackAction；否则存活 Hold。
namespace EffectTreeBuilder
{

// 构建特效根：action_ids 查特效动作表挂 AttackAction，否则 Alive + Hold
BTNode* build(BehaviorTreeComponent* bt, Entity* entity = nullptr);

// 挂到实体
void attachToEntity(Entity* entity);

}  // namespace EffectTreeBuilder

NS_MG_END
