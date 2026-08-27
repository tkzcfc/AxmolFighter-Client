#pragma once

#include "mugen/core/ecs/System.h"

NS_MG_BEGIN

// 驱动行为树：建树、输入施法、受击打断、每帧 tick root。
class BehaviorTreeSystem : public System
{
public:
    typedef System Super;

    BehaviorTreeSystem();
    virtual ~BehaviorTreeSystem();

    virtual void init(ECSManager* ecs) override;

    // root 为空则 attach；否则 rebindAttackSelector
    virtual void onEntityAdded(Entity* entity) override;

    virtual void update() override;

private:
    // 只填 entity；ecs / 世界时间由叶子从 entity 取
    void fillContext(Entity* entity, struct BTContext& ctx);

    // 从输入解析技能并 preset
    void tryCastFromInput(Entity* entity);

    // 消费受击队列；首次受击对 root onExit
    void processPendingHits(Entity* entity);

    // 硬直 / 倒地 / 起身 / 定身倒计时
    void tickHitRecovery(Entity* entity, int32_t dtMs);

    // 根据物理落地更新 Airborne / Grounded 标签
    void updateAirborneTags(Entity* entity);

    // 双击同向进入 Dash；攻击中则请求跑取消
    void updateDoubleTapRun(Entity* entity, int64_t runningTimeMs);
};

NS_MG_END
