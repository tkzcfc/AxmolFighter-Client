#pragma once

#include "mugen/core/StdC.h"

NS_MG_BEGIN

class Entity;
class SkillAiConfig;
class SkillAiRuntimeState;
class Random;

/** skill_ai 条件检查（对齐参照 SkillAi.lua） */
namespace SkillAiRules
{

void ensureRuntime(SkillAiRuntimeState& state, const SkillAiConfig* cfg);
void tick(SkillAiRuntimeState& state, int32_t dtMs);

/**
 * 完整 check：loadCd / useCount / checkCd / composition → 置 checkCd → prob → 扣 useCount。
 * @return true 表示本帧允许用该 skill_ai 绑定的技能施放
 */
bool check(Entity* self,
           Entity* target,
           SkillAiRuntimeState& state,
           const SkillAiConfig* cfg,
           Random& rng);

}  // namespace SkillAiRules

NS_MG_END
