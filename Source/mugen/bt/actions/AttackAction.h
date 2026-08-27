#pragma once

#include "mugen/core/bt/BTAction.h"

#include <vector>

NS_MG_BEGIN

class ActionAttackConfig;

class AttackAction : public BTAction
{
public:
    typedef BTAction Super;

public:
    AttackAction();
    AttackAction(int32_t actionId, int32_t actionIndex, int32_t skillAttackId, bool effectTable = false);
    virtual ~AttackAction();

    const char* typeName() const override { return "AttackAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    void onActionExit(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;

private:
    const ActionAttackConfig* lookupActionCfg() const;
    bool roleCastGone(BTContext& ctx) const;
    void syncDerivedFromConfig(BTContext& ctx);
    void applyBuffs(BTContext& ctx, bool add);
    void resetDisplacement(BTContext& ctx);
    void spawnEffect(BTContext& ctx, int32_t effectId);
    void playSounds(BTContext& ctx);
    void dealWithControl(BTContext& ctx);
    void dealWithPresentation(BTContext& ctx, float frame);
    void triggerShake(BTContext& ctx);
    void triggerDisplaySpine(BTContext& ctx);
    void triggerTransform(BTContext& ctx);
    void triggerStatic(BTContext& ctx);

private:
    std::vector<bool> effectSpawned;
    std::vector<bool> soundsPlayed;

public:
    int32_t actionId          = 0;
    int32_t actionIndex       = 0;
    int32_t skillAttackId     = 0;
    bool effectTable          = false;
    int32_t estimatedDurMs    = 5000;
    int32_t elapsedMs         = 0;
    int32_t animFinishedAtMs  = -1;
    int32_t actionDelayMs     = 0;
    float frameIntervalMs     = 1000.0f / 30.0f;
    bool airborneOnce         = false;
    uint32_t effectSpawnMask  = 0;
    uint32_t presentationMask = 0;
    bool animationEnd         = false;
    bool sessionOpen          = false;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  estimatedDurMs,
                                  elapsedMs,
                                  animFinishedAtMs,
                                  airborneOnce,
                                  effectSpawnMask,
                                  presentationMask,
                                  animationEnd,
                                  sessionOpen)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
