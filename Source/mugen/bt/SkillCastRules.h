#pragma once

#include "mugen/core/StdC.h"

NS_MG_BEGIN

class Entity;
class SkillCastComponent;
class SkillDeckComponent;
class AttributeComponent;
class BehaviorComponent;
class InputComponent;
class SkillAttackConfig;

/** 施法规则（静态函数，状态写组件；对应 SkillBase / SkillPool） */
namespace SkillCastRules
{

// SkillOrderControlType
constexpr int32_t kIgnoreOrderInterruptFrame      = 1;
constexpr int32_t kIgnoreOrderInterruptExtraFrame = 2;

// 跑取消 sorder 门槛（-1=始终允许；否则 cur.sorder 须 <= 此值）
constexpr int32_t kRunSorder = 30;
// 连段（slotIndex>1）MP 倍率
constexpr float kLinkSkillMpRate = 0.8f;

// SkillVector（用于朝向折叠）
enum class SkillVector : int32_t
{
    None      = 0,
    Front     = 1,
    FrontUp   = 2,
    FrontDown = 3,
    Up        = 4,
    Down      = 5,
    Back      = 6,
    BackUp    = 7,
    BackDown  = 8,
};

bool hasOrderControl(const SkillAttackConfig* cfg, int32_t controlType);
bool isPriority(const SkillAttackConfig* nextCfg, const SkillAttackConfig* curCfg);
bool isSuperPriority(const SkillAttackConfig* nextCfg,
                     const SkillAttackConfig* curCfg,
                     bool interruptOpen,
                     bool interruptExtraOpen);

int32_t pipeMaxOf(const SkillAttackConfig* cfg);
void resetSkillPipe(SkillCastComponent* cast, int32_t pipeMax);
void expectSkillPipe(SkillCastComponent* cast);
void selectSkillPipe(SkillCastComponent* cast);

/** 摇杆/按键 → towardIndex（1-based，含 Back 折叠；无效回落 1） */
int32_t dealWithDirection(Entity* entity, const SkillAttackConfig* skillCfg);

bool isAllowCast(Entity* entity, int32_t skillAttackId, bool isAutoCast = false);

/** Pipe.enter：扣费/CD/次数/朝向/管道推进；costPaid 幂等 */
bool castBegan(Entity* entity);

/** Pipe.exit：尝试消费预输入衔接 */
bool castEnded(Entity* entity);

/** 消费 pending → 切 active；算朝向；selectSkillPipe；costPaid=false */
bool dealWithNextSkillBase(Entity* entity);

/**
 * 输入入口（presetSkill）。
 * @return true 已激活或已写入预输入
 */
bool presetSkill(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot);

/** 同槽解析下一招（含 cdCount>1 不推进 Step） */
int32_t resolveFightSkill(Entity* entity, int32_t inputSlot, int32_t* outStep);

/** 普通取消窗是否可切（同槽无条件 / 异槽 isPriority） */
bool canConsumePendingOnInterrupt(Entity* entity);

/** 至尊取消窗是否可切 */
bool canConsumePendingOnExtraInterrupt(Entity* entity);

/** 跑取消门控：RUN_SORDER / IgnoreOrder+interruptOpen */
bool canRunCancel(Entity* entity);

/** 写入输入缓冲（tags 未过时） */
void queueInputBuffer(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot);

/** 推进并尝试消费输入缓冲 */
void tickInputBuffer(Entity* entity, int32_t dtMs);

/** 请求跑取消（Dash 意图时置 wantRunCancel） */
void requestRunCancel(Entity* entity);

/** 消费跑取消：清技能并切 Dash；成功返回 true */
bool dealWithRun(Entity* entity);

/** 同步 BehaviorComponent.currentKind（施法态已迁出到 SkillCastComponent） */
void syncBehaviorMirror(Entity* entity);

/** Slot.exit：无 pending 则清当前技能 */
void onSlotEnded(Entity* entity, int32_t slot);

/** Step.enter/exit：reset+expect */
void onStepBegan(Entity* entity);
void onStepEnded(Entity* entity);

/** 受击等强制打断：清施法上下文（幂等） */
void forceInterruptCast(Entity* entity);

}  // namespace SkillCastRules

NS_MG_END
