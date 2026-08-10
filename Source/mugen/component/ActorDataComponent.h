#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

#include <string>
#include <unordered_map>
#include <vector>

NS_MG_BEGIN

// 技能动态数据：记录已学技能的名称和等级
class SkillInstanceData : public Object
{
public:
    typedef Object Super;

public:
    SkillInstanceData() {}
    virtual ~SkillInstanceData() {}

public:
    // 当前技能的组合输入匹配进度,0表示未匹配，1 表示已匹配 comboInputs[0]，以此类推
    int32_t comboInputsMatchedCount = 0;
    // 最后匹配成功时间
    uint64_t lastComboInputMatchedTime = 0;

    // 搓招序列：输入槽位 id 列表（空=无搓招，走普通槽触发）
    std::vector<int32_t> comboInputs;

    // 技能等级,如果为0表示未学会，1表示初级，以此类推
    int32_t level = 0;

    // 技能攻击配置 id
    int32_t skillAttackId = 0;

    uint32_t slotTriggerFlags       = SlotTriggerFlag::kSlotTriggerPress;
    uint32_t allowTags              = StateTag::kTagGrounded | StateTag::kTagAttackAllowed;
    uint32_t denyTags               = StateTag::kTagHitState;
    int32_t comboWindowMs           = 500;
    uint32_t inputBufferReleaseTags = 0;
    int32_t inputBufferTimeoutMs    = 500;

    // 从 skill_attack 表构建运行时触发数据。
    void buildFromSkillAttack(int32_t attackId)
    {
        skillAttackId          = attackId;
        slotTriggerFlags       = SlotTriggerFlag::kSlotTriggerPress;
        allowTags              = StateTag::kTagGrounded | StateTag::kTagAttackAllowed;
        denyTags               = StateTag::kTagHitState;
        comboWindowMs          = 500;
        inputBufferReleaseTags = 0;
        inputBufferTimeoutMs   = 500;
        comboInputs.clear();
        comboInputsMatchedCount    = 0;
        lastComboInputMatchedTime  = 0;

        if (const auto* overlay = Config::getInstance()->getSkillActivationOverlayById(attackId))
        {
            slotTriggerFlags       = overlay->slotTriggerFlags;
            allowTags              = overlay->allowTags;
            denyTags               = overlay->denyTags;
            comboWindowMs          = overlay->comboWindowMs;
            inputBufferReleaseTags = overlay->inputBufferReleaseTags;
            inputBufferTimeoutMs   = overlay->inputBufferTimeoutMs;
            comboInputs            = overlay->comboInputs;
        }
    }

    // 判断技能是否相同
    bool isSameSkill(const std::string& skillSourcePath) const
    {
        return skillSourcePath == ("skill_attack:" + std::to_string(skillAttackId));
    }

    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  comboInputsMatchedCount,
                                  lastComboInputMatchedTime,
                                  comboInputs,
                                  level,
                                  skillAttackId)

    void serializeCustomImpl(ByteBuffer& /*byteBuffer*/) const {}

    bool deserializeCustomImpl(ByteBuffer& byteBuffer)
    {
        (void)byteBuffer;
        buildFromSkillAttack(skillAttackId);
        return true;
    }
};

// Actor 通用动态数据组件（玩家和怪物均可挂载）
class ActorDataComponent : public Component
{
public:
    typedef Component Super;

public:
    ActorDataComponent() {}
    virtual ~ActorDataComponent() {}

    // 角色等级
    int32_t characterLevel = 1;

    // 角色经验值
    int32_t exp = 0;

    // 已学技能
    std::vector<SkillInstanceData> skills;

    MG_DEFINE_SERIALIZABLE(characterLevel, exp, skills)

public:
    bool hasSkill(const std::string& skillConfigFileName) const
    {
        for (const auto& skillInst : skills)
        {
            if (skillInst.isSameSkill(skillConfigFileName))
            {
                return true;
            }
        }
        return false;
    }

    int32_t getSkillIndex(const std::string& skillConfigFileName) const
    {
        for (size_t i = 0; i < skills.size(); ++i)
        {
            if (skills[i].isSameSkill(skillConfigFileName))
            {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }
};

NS_MG_END
