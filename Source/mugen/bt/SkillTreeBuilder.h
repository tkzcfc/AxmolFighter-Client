#pragma once

#include "mugen/core/ecs/Entity.h"

NS_MG_BEGIN

namespace SkillTreeBuilder
{

/** 向 RoleTreeBuilder 创建的 Attack Selector 灌入 Slot→Step→Pipe→Toward→AttackAction 子树 */
void fill(Entity* entity);

/** 清空并重建技能子树（换装/学技能时调用；一期通常只 fill 一次） */
void rebuild(Entity* entity);

}  // namespace SkillTreeBuilder

NS_MG_END
