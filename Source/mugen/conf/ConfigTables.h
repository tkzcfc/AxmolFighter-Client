#pragma once

#include "tables/SkillTables.h"

#include <cstdint>
#include <utility>

// skill -> SkillAttackConfig -> tables/SkillTables.h
// skill_hit -> SkillHitConfig -> tables/SkillTables.h
// skill_hurt -> SkillHurtConfig -> tables/SkillTables.h
// skill_ai -> SkillAiConfig -> tables/SkillTables.h
// skill_rock -> SkillRockConfig -> tables/SkillTables.h
// skill_tree -> SkillTreeConfig -> tables/SkillTables.h
// skill_leaf -> SkillLeafConfig -> tables/SkillTables.h
// skill_change_system -> SkillChangeSystemConfig -> tables/SkillTables.h
// action -> ActionConfig -> tables/ActionTables.h
// action_effect -> ActionEffectConfig -> tables/ActionTables.h
// camera -> CameraConfig -> tables/ActionTables.h
// displacement -> DisplacementConfig -> tables/ActionTables.h
// role -> RoleConfig -> tables/EntityTables.h
// effect -> EffectConfig -> tables/EntityTables.h
// ai -> AiConfig -> tables/AiTables.h
// attribute -> AttributeConfig -> tables/EntityTables.h
// obstacle -> ObstacleConfig -> tables/EntityTables.h
// goods -> GoodsConfig -> tables/EntityTables.h
// npc -> NpcConfig -> tables/EntityTables.h
// portal -> PortalConfig -> tables/EntityTables.h
// res_spine -> ResSpineConfig -> tables/EntityTables.h
// buff -> BuffConfig -> tables/BuffTables.h
// buff_rule -> BuffRuleConfig -> tables/BuffTables.h
// special_ability -> SpecialAbilityConfig -> tables/BuffTables.h
// room -> RoomConfig -> tables/MapTables.h
// stage -> StageConfig -> tables/MapTables.h
// copy -> CopyConfig -> tables/MapTables.h
// chapter -> ChapterConfig -> tables/MapTables.h
// camp -> CampConfig -> tables/MapTables.h
// town -> TownConfig -> tables/MapTables.h
// res_sound -> ResSoundConfig -> tables/SoundTables.h
// sound_ui -> SoundUiConfig -> tables/SoundTables.h
// sound_spine -> SoundSpineConfig -> tables/SoundTables.h
// sound_spine_bgm -> SoundSpineBgmConfig -> tables/SoundTables.h
// sound_map_spine -> SoundMapSpineConfig -> tables/SoundTables.h
// sound_talk -> SoundTalkConfig -> tables/SoundTables.h
// sound_sendmessage -> SoundSendMessageConfig -> tables/SoundTables.h
// equip -> EquipConfig -> tables/EquipTables.h
// fashion -> FashionConfig -> tables/FashionTables.h
// fashion_suit -> FashionSuitConfig -> tables/FashionTables.h
// res_fashion -> ResFashionConfig -> tables/FashionTables.h
// item_base -> ItemBaseConfig -> tables/ItemTables.h

namespace mugen::conf
{
struct SkillHurtConfig;
struct SkillAiConfig;
struct SkillRockConfig;
struct SkillTreeConfig;
struct SkillLeafConfig;
struct SkillChangeSystemConfig;
struct ActionConfig;
struct ActionEffectConfig;
struct CameraConfig;
struct DisplacementConfig;
struct RoleConfig;
struct EffectConfig;
struct AiConfig;
struct AttributeConfig;
struct ObstacleConfig;
struct GoodsConfig;
struct NpcConfig;
struct PortalConfig;
struct ResSpineConfig;
struct BuffConfig;
struct BuffRuleConfig;
struct SpecialAbilityConfig;
struct RoomConfig;
struct StageConfig;
struct CopyConfig;
struct ChapterConfig;
struct CampConfig;
struct TownConfig;
struct ResSoundConfig;
struct SoundUiConfig;
struct SoundSpineConfig;
struct SoundSpineBgmConfig;
struct SoundMapSpineConfig;
struct SoundTalkConfig;
struct SoundSendMessageConfig;
struct EquipConfig;
struct FashionConfig;
struct FashionSuitConfig;
struct ResFashionConfig;
struct ItemBaseConfig;

// 按单一 id 建立索引的表键策略。
struct KeyById
{
    // 从行数据提取 id 键。
    template <class T>
    static int32_t get(const T& row)
    {
        return row.id;
    }
};
// 按 id 与职业联合建立索引的表键策略。
struct KeyByIdAndOccupation
{
    // 从行数据提取联合键。
    template <class T>
    static std::pair<int32_t, int32_t> get(const T& row)
    {
        return {row.id, row.occupation};
    }
};
// 保留源表顺序的一基序号键策略。
struct KeyByOrdinal
{
    // 返回调用方提供的序号键。
    template <class T>
    static int32_t get(const T&, int32_t ordinal)
    {
        return ordinal;
    }
};
}  // namespace mugen::conf

#define MG_CONFIG_TABLES(X)                                  \
    X(skill, SkillAttackConfig, KeyById)                     \
    X(skill_hit, SkillHitConfig, KeyById)                    \
    X(skill_hurt, SkillHurtConfig, KeyById)                  \
    X(skill_ai, SkillAiConfig, KeyById)                      \
    X(skill_rock, SkillRockConfig, KeyById)                  \
    X(skill_tree, SkillTreeConfig, KeyById)                  \
    X(skill_leaf, SkillLeafConfig, KeyById)                  \
    X(skill_change_system, SkillChangeSystemConfig, KeyById) \
    X(action, ActionConfig, KeyById)                         \
    X(action_effect, ActionEffectConfig, KeyById)            \
    X(camera, CameraConfig, KeyById)                         \
    X(displacement, DisplacementConfig, KeyById)             \
    X(role, RoleConfig, KeyById)                             \
    X(effect, EffectConfig, KeyById)                         \
    X(ai, AiConfig, KeyById)                                 \
    X(attribute, AttributeConfig, KeyById)                   \
    X(obstacle, ObstacleConfig, KeyById)                     \
    X(goods, GoodsConfig, KeyById)                           \
    X(npc, NpcConfig, KeyById)                               \
    X(portal, PortalConfig, KeyById)                         \
    X(res_spine, ResSpineConfig, KeyById)                    \
    X(buff, BuffConfig, KeyById)                             \
    X(buff_rule, BuffRuleConfig, KeyById)                    \
    X(special_ability, SpecialAbilityConfig, KeyById)        \
    X(room, RoomConfig, KeyById)                             \
    X(stage, StageConfig, KeyById)                           \
    X(copy, CopyConfig, KeyById)                             \
    X(chapter, ChapterConfig, KeyById)                       \
    X(camp, CampConfig, KeyById)                             \
    X(town, TownConfig, KeyById)                             \
    X(res_sound, ResSoundConfig, KeyById)                    \
    X(sound_ui, SoundUiConfig, KeyByOrdinal)                 \
    X(sound_spine, SoundSpineConfig, KeyByOrdinal)           \
    X(sound_spine_bgm, SoundSpineBgmConfig, KeyByOrdinal)    \
    X(sound_map_spine, SoundMapSpineConfig, KeyByOrdinal)    \
    X(sound_talk, SoundTalkConfig, KeyByOrdinal)             \
    X(sound_sendmessage, SoundSendMessageConfig, KeyById)    \
    X(equip, EquipConfig, KeyByIdAndOccupation)              \
    X(fashion, FashionConfig, KeyByIdAndOccupation)          \
    X(fashion_suit, FashionSuitConfig, KeyById)              \
    X(res_fashion, ResFashionConfig, KeyById)                \
    X(item_base, ItemBaseConfig, KeyById)
