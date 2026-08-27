#pragma once

#include "mugen/skill/Skill.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class Entity;
class SkillAttackConfig;

constexpr int32_t kIgnoreOrderInterruptFrame      = 1;
constexpr int32_t kIgnoreOrderInterruptExtraFrame = 2;
constexpr int32_t kRunSorder                      = 30;

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

class SkillManager : public Object
{
public:
    typedef Object Super;

public:
    SkillManager() {}

    virtual ~SkillManager() {}

    const char* typeName() const { return "SkillManager"; }

    void bindConfig(Entity* entity);

    void update(Entity* entity, int32_t dtMs);

    Skill* findSkill(int32_t skillAttackId) const;

    Skill* currentSkill() const { return findSkill(activeSkillAttackId); }

    static SkillManager* of(Entity* entity);

    static bool hasOrderControl(const SkillAttackConfig* cfg, int32_t controlType);

    static bool isPriority(const SkillAttackConfig* nextCfg, const SkillAttackConfig* curCfg);

    static bool isSuperPriority(const SkillAttackConfig* nextCfg,
                                const SkillAttackConfig* curCfg,
                                bool interruptOpen,
                                bool interruptExtraOpen);

    static int32_t pipeMaxOf(const SkillAttackConfig* cfg);

    void resetSkillPipe(int32_t pipeMax);

    void expectSkillPipe();

    void selectSkillPipe();

    int32_t dealWithDirection(Entity* entity, const SkillAttackConfig* skillCfg);

    bool isAllowCast(Entity* entity, int32_t skillAttackId, bool isAutoCast = false);

    bool castBegan(Entity* entity);

    bool castEnded(Entity* entity);

    bool dealWithNextSkillBase(Entity* entity);

    bool presetSkill(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot);

    int32_t resolveFightSkill(Entity* entity, int32_t inputSlot, int32_t* outStep);

    int32_t findSlotForSkill(Entity* entity, int32_t skillAttackId, int32_t* outStep);

    bool canConsumePendingOnInterrupt(Entity* entity);

    bool canConsumePendingOnExtraInterrupt(Entity* entity);

    bool canRunCancel(Entity* entity);

    void queueInputBuffer(Entity* entity, int32_t skillAttackId, int32_t inputSlot, int32_t stepInSlot);

    void requestRunCancel(Entity* entity);

    bool dealWithRun(Entity* entity);

    void syncBehaviorMirror(Entity* entity);

    void onSlotEnded(Entity* entity, int32_t slot);

    void onStepBegan(Entity* entity);

    void onStepEnded(Entity* entity);

    void forceInterruptCast(Entity* entity);

    void startCrazy(Entity* entity);

    void endCrazy(Entity* entity);

private:
    void clearInputBuffer();

    void clearActiveSkillFields(Entity* entity);

    void setActiveSkill(Entity* entity, int32_t skillId, int32_t inputSlot, int32_t stepInSlot, bool closeWindows);

    void tickInputBuffer(Entity* entity, int32_t dtMs);

public:
    std::vector<std::unique_ptr<Skill>> skills;
    int32_t activeSkillAttackId  = 0;
    int32_t pendingSkillAttackId = 0;
    int32_t activeInputSlot      = 0;
    int32_t activeStepInSlot     = 0;
    int32_t activeSlotIndex      = 1;
    int32_t modeIndex            = 0;
    bool interruptOpen           = false;
    bool interruptExtraOpen      = false;
    int32_t thrustSkillAttackId  = 0;
    int32_t dodgeSkillAttackId   = 0;
    int32_t crazySkillAttackId   = 0;
    bool crazyActive             = false;
    int32_t crazyRemainMs        = 0;
    bool crazyEnding             = false;  // EP 到 0 后等当前招结束或切槽再 endCrazy
    int32_t pendingInputSlot     = 0;
    int32_t pendingStepInSlot    = 0;
    bool wantRunCancel           = false;
    int32_t pipeIndex            = 0;
    int32_t prePipeIndex         = 0;
    int32_t expectPipeIndex      = 0;
    int32_t towardIndex          = 0;
    bool costPaid                = false;
    int32_t costPaidPipeIndex    = -1;
    int32_t meleeHitSkillId      = 0;
    std::vector<uint32_t> meleeHitTargetIds;
    std::vector<int32_t> meleeHitCounts;
    std::vector<int32_t> meleeHitCooldowns;
    std::vector<uint32_t> spawnedEffectIds;
    int32_t bufferSkillAttackId = 0;
    int32_t bufferInputSlot     = 0;
    int32_t bufferStepInSlot    = 0;
    int32_t bufferRemainMs      = 0;
    uint32_t bufferReleaseTags  = 0;
    int32_t staticResetRemainMs = 0;
    int32_t lastCastSkillId     = 0;
    int32_t lastCastSlot        = 0;
    int32_t lastCastSlotIndex   = 0;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  activeSkillAttackId,
                                  pendingSkillAttackId,
                                  activeInputSlot,
                                  activeStepInSlot,
                                  activeSlotIndex,
                                  modeIndex,
                                  interruptOpen,
                                  interruptExtraOpen,
                                  thrustSkillAttackId,
                                  dodgeSkillAttackId,
                                  crazySkillAttackId,
                                  crazyActive,
                                  crazyRemainMs,
                                  crazyEnding,
                                  pendingInputSlot,
                                  pendingStepInSlot,
                                  wantRunCancel,
                                  pipeIndex,
                                  prePipeIndex,
                                  expectPipeIndex,
                                  towardIndex,
                                  costPaid,
                                  costPaidPipeIndex,
                                  meleeHitSkillId,
                                  meleeHitTargetIds,
                                  meleeHitCounts,
                                  meleeHitCooldowns,
                                  spawnedEffectIds,
                                  bufferSkillAttackId,
                                  bufferInputSlot,
                                  bufferStepInSlot,
                                  bufferRemainMs,
                                  bufferReleaseTags,
                                  staticResetRemainMs,
                                  lastCastSkillId,
                                  lastCastSlot,
                                  lastCastSlotIndex)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
