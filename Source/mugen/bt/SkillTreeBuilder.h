#pragma once

#include "mugen/core/ecs/Entity.h"

NS_MG_BEGIN

// 按 SkillBar + SkillDeck 向 Attack Selector 灌入技能图。拓扑不进快照。
namespace SkillTreeBuilder
{

// 向 Attack Selector 灌入 Slot→SlotIndex→Mode→Step→Pipe→Toward→AttackAction
void fill(Entity* entity);

// 清空并重建技能子树（换装/学技能）
void rebuild(Entity* entity);

}  // namespace SkillTreeBuilder

NS_MG_END
