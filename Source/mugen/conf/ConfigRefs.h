#pragma once

// 声明配置表字段到目标表的全部引用关系。
#define MG_CONFIG_REFS(X) \
    X(skill, actionIds, action) X(skill, upActionIds, action) X(skill, nextSkill, skill) X(skill, skillAiId, skill_ai) \
    X(skill_hit, displacementId, displacement) X(skill_hit, airDisplacementId, displacement) X(skill_hit, floorDisplacementId, displacement) \
    X(skill_rock, skillId, skill) X(skill_leaf, skillId, skill) \
    X(skill_tree, leafIds, skill_leaf) X(skill_tree, leafRubId, skill_leaf) X(skill_tree, leafBuffId, skill_leaf) \
    X(skill_change_system, skillId, skill) X(skill_change_system, skillNewId, skill) \
    X(action, displacementId, displacement) X(action, effectIds, effect) X(action, buffIds, buff) X(action, cameraId, camera) X(action, transformId, role) \
    X(action_effect, displacementId, displacement) X(action_effect, effectIds, effect) X(action_effect, cameraId, camera) X(action_effect, summonId, role) \
    X(effect, actionIds, action_effect) X(effect, hitId, skill_hit) X(effect, hitEffectIds, effect) X(effect, nextEffectId, effect) \
    X(effect, buffId, buff) X(effect, buffAllId, buff) X(effect, debuffId, buff) X(effect, specialabilityId, special_ability) X(effect, specialabilityAllId, special_ability) \
    X(role, aiId, ai) X(role, hitDisplacementId, displacement) X(role, deathDisplacementId, displacement) X(role, deathEffectId, effect) X(role, koEffectId, effect) X(role, buffIds, buff) \
    X(ai, skillIds, skill) X(ai, crazySkillIds, skill) X(ai, joystickSkillIds, skill) X(ai, otherSkillIds, skill) X(ai, skillAiIds, skill_ai) X(ai, crazySkillAiIds, skill_ai) \
    X(buff, ruleId, buff_rule) X(buff, bindSpecialAbilityId, special_ability) X(special_ability, effectId, effect) X(special_ability, bindBuffId, buff) \
    X(goods, buffId, buff) X(goods, specialabilityId, special_ability) X(obstacle, deathEffectId, effect) X(obstacle, deathDisplacementId, displacement) \
    X(room, monsterIds, role) X(room, actorBuffIds, buff) X(room, obstacleIds, obstacle) X(room, goodIds, goods) X(room, portalIds, portal) \
    X(stage, roomId, room) X(copy, stageId, stage) X(equip, buffId, buff) X(equip, specialAbilityId, special_ability) X(fashion, specialability, special_ability)
