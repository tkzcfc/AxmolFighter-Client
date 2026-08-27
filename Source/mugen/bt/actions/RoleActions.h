#pragma once

#include "mugen/core/bt/BTAction.h"
#include "mugen/conf/GameDef.h"

NS_MG_BEGIN

class LocomoAction : public BTAction
{
public:
    typedef BTAction Super;

public:
    LocomoAction();
    explicit LocomoAction(BehaviorKind kind);
    virtual ~LocomoAction();

    const char* typeName() const override { return "LocomoAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;

public:
    BehaviorKind kind = BehaviorKind::kIdle;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

// 受击枝叶子：Hit / HitUp / HitDown / HitFloor / HitSwitch / GetUp
class HitKindAction : public BTAction
{
public:
    typedef BTAction Super;

public:
    HitKindAction();
    explicit HitKindAction(BehaviorKind kind);
    virtual ~HitKindAction();

    const char* typeName() const override { return "HitKindAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;

public:
    BehaviorKind kind      = BehaviorKind::kStun;
    bool animationEnd      = false;
    bool displacementEnd   = false;
    int32_t afterEndMs     = 0;
    int32_t displacementId = -1;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  animationEnd,
                                  displacementEnd,
                                  afterEndMs,
                                  displacementId)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

class TimedKindAction : public BTAction
{
public:
    typedef BTAction Super;

public:
    TimedKindAction();
    explicit TimedKindAction(BehaviorKind kind);
    virtual ~TimedKindAction();

    const char* typeName() const override { return "TimedKindAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;

public:
    BehaviorKind kind = BehaviorKind::kIdle;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

class DeathAction : public BTAction
{
public:
    typedef BTAction Super;

public:
    DeathAction();
    virtual ~DeathAction();

    const char* typeName() const override { return "DeathAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;

public:
    bool animationEnd      = false;
    bool displacementEnd   = false;
    int32_t afterEndMs     = 0;
    int32_t displacementId = -1;

public:
    MG_DEFINE_SERIALIZABLE(animationEnd, displacementEnd, afterEndMs, displacementId)
};

class WakeAction : public BTAction
{
public:
    WakeAction();
    virtual ~WakeAction();

    const char* typeName() const override { return "WakeAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

class ReviveAction : public BTAction
{
public:
    ReviveAction();
    virtual ~ReviveAction();

    const char* typeName() const override { return "ReviveAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

class HoldAttackAction : public BTAction
{
public:
    HoldAttackAction();
    virtual ~HoldAttackAction();

    const char* typeName() const override { return "HoldAttackAction"; }

protected:
    void onActionEnter(BTContext& ctx) override;
    BTStatus onActionUpdate(BTContext& ctx, int32_t dtMs) override;
};

NS_MG_END
