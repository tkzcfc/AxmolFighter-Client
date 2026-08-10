#pragma once

#include "mugen/buff/BuffRuleBase.h"

NS_MG_BEGIN

class BuffRuleInvincible : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
    void onStack(Entity* entity, BuffInstance& inst) override;
};

class BuffRuleSuperArmor : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
    void onStack(Entity* entity, BuffInstance& inst) override;
};

/** 周期伤/疗：读 BuffConfig.interval + paramValue[0] */
class BuffRulePeriodicHurt : public BuffRuleBase
{
public:
    bool onTick(Entity* entity, BuffInstance& inst, int32_t dtMs) override;
};

/** ADD_HURT：BuffDamageHurt / HurtScale */
class BuffRuleDamageHurt : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
    void onBegin(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
                 float param) override;
    void onEnd(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
               float param) override;
};

/** AVOID_HURT：BuffDamageReduction */
class BuffRuleDamageReduction : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
    void onBegin(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
                 float param) override;
    void onEnd(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
               float param) override;
};

/** 技能槽增伤：BuffDamageSlot（施法 begin/end 临时 ADD_HURT） */
class BuffRuleDamageSlot : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
    void onBegin(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
                 float param) override;
    void onEnd(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
               float param) override;
};

/** CD 缩放：BuffCDSkill → coldTimeScale += param */
class BuffRuleCDSkill : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
};

/** 立即改当前 CD：BuffModifyCDSkill → coolDownMs += cd * param */
class BuffRuleModifyCDSkill : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onBegin(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
                 float param) override;
};

/** MP 消耗缩放：BuffTPConsumeScale */
class BuffRuleTPConsumeScale : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
};

/** EP 消耗缩放：BuffEPConsumeScale */
class BuffRuleEPConsumeScale : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
};

/** ADD_CRIT：BuffCrit */
class BuffRuleCrit : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
};

/** ADD_MAXHP：BuffHPMAX */
class BuffRuleHPMAX : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    void onRemove(Entity* entity, BuffInstance& inst) override;
};

/** 即时/周期改 HP：BuffHP */
class BuffRuleHP : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, BuffInstance& inst) override;
    bool onTick(Entity* entity, BuffInstance& inst, int32_t dtMs) override;
};

NS_MG_END
