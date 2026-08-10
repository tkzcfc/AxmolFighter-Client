#pragma once

#include "mugen/buff/BFEvent.h"
#include "mugen/core/StdC.h"

NS_MG_BEGIN

class Entity;
class BuffInstance;

/** Buff 规则基类：无持久 gameplay 状态，读写组件 */
class BuffRuleBase
{
public:
    virtual ~BuffRuleBase() = default;

    virtual void onAdd(Entity* entity, BuffInstance& inst) {}
    virtual void onRemove(Entity* entity, BuffInstance& inst) {}
    /** 叠层后刷新（默认再走 onAdd 语义由子类自行幂等） */
    virtual void onStack(Entity* entity, BuffInstance& inst) { onAdd(entity, inst); }
    /** 周期 tick；返回 true 表示本帧已处理（跳过默认周期伤） */
    virtual bool onTick(Entity* entity, BuffInstance& inst, int32_t dtMs) { return false; }
    /** 表 began 命中 */
    virtual void onBegin(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
                         float param)
    {
        onEvent(entity, inst, event, other, skillId, param);
    }
    /** 表 ended 命中 */
    virtual void onEnd(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
                       float param)
    {
        onEvent(entity, inst, event, other, skillId, param);
    }
    virtual void onEvent(Entity* entity, BuffInstance& inst, BFEvent event, Entity* other, int32_t skillId,
                         float param)
    {
        (void)entity;
        (void)inst;
        (void)event;
        (void)other;
        (void)skillId;
        (void)param;
    }
};

NS_MG_END
