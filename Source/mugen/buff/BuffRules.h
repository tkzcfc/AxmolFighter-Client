#pragma once

#include "mugen/buff/BuffRuleBase.h"

NS_MG_BEGIN

class BuffRuleInvincible : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onStack(Entity* entity, Buff& inst) override;
};

class BuffRuleSuperArmor : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onStack(Entity* entity, Buff& inst) override;
};

/** 周期伤/疗：读 BuffConfig.interval + paramValue[0] */
class BuffRulePeriodicHurt : public BuffRuleBase
{
public:
    bool onTick(Entity* entity, Buff& inst, int32_t dtMs) override;
};

/** ADD_HURT：BuffDamageHurt / HurtScale */
class BuffRuleDamageHurt : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onBegin(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
    void onEnd(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
};

/** AVOID_HURT：BuffDamageReduction */
class BuffRuleDamageReduction : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onBegin(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
    void onEnd(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
};

/** 技能槽增伤：BuffDamageSlot（施法 begin/end 临时 ADD_HURT） */
class BuffRuleDamageSlot : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onBegin(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
    void onEnd(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
};

/** CD 缩放：BuffCDSkill → coldTimeScale += param */
class BuffRuleCDSkill : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
};

/** 立即改当前 CD：BuffModifyCDSkill → coolDownMs += cd * param */
class BuffRuleModifyCDSkill : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onBegin(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
};

/** MP 消耗缩放：BuffTPConsumeScale */
class BuffRuleTPConsumeScale : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
};

/** EP 消耗缩放：BuffEPConsumeScale */
class BuffRuleEPConsumeScale : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
};

/** ADD_CRIT：BuffCrit */
class BuffRuleCrit : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
};

/** ADD_MAXHP：BuffHPMAX */
class BuffRuleHPMAX : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
};

/** 即时/周期改 HP：BuffHP */
class BuffRuleHP : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    bool onTick(Entity* entity, Buff& inst, int32_t dtMs) override;
};

class BuffRuleStun : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
};

class BuffRuleSpeed : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onStack(Entity* entity, Buff& inst) override;
};

class BuffRuleCrazy : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
};

class BuffRuleHPLock : public BuffRuleBase
{
public:
    bool onTick(Entity* entity, Buff& inst, int32_t dtMs) override;
};

class BuffRuleDurance : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
};

class BuffRuleSneer : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
};

/** 按 param 里的 buffId 给自己或事件目标加/卸子 Buff */
class BuffRuleAddByApplicator : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onBegin(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
    void onEnd(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
};

/** 起身/击退/击飞达到时长后加子 Buff */
class BuffRuleAddByState : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onRemove(Entity* entity, Buff& inst) override;
    void onStack(Entity* /*entity*/, Buff& /*inst*/) override {}
    bool onTick(Entity* entity, Buff& inst, int32_t dtMs) override;
    void onEvent(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
};

/** 加 MP：param>1 为绝对值，否则为 mpMax 比例 */
class BuffRuleTP : public BuffRuleBase
{
public:
    void onAdd(Entity* entity, Buff& inst) override;
    void onBegin(Entity* entity, Buff& inst, BFEvent event, Entity* other, int32_t skillId, float param) override;
};

NS_MG_END
