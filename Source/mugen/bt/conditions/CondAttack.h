#pragma once

#include "mugen/core/bt/BTCondition.h"

NS_MG_BEGIN

class CondRoleAttack : public BTCondition
{
public:
    CondRoleAttack();
    virtual ~CondRoleAttack();
    const char* typeName() const override { return "CondRoleAttack"; }
    bool check(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;
};

class CondAttackSlot : public BTCondition
{
public:
    typedef BTCondition Super;

public:
    CondAttackSlot();
    explicit CondAttackSlot(int32_t slot);
    virtual ~CondAttackSlot();
    const char* typeName() const override { return "CondAttackSlot"; }
    bool check(BTContext& ctx) override;
    bool onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;

public:
    int32_t slot = 0;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

class CondAttackStep : public BTCondition
{
public:
    typedef BTCondition Super;

public:
    CondAttackStep();
    CondAttackStep(int32_t slot, int32_t stepIndex);
    virtual ~CondAttackStep();
    const char* typeName() const override { return "CondAttackStep"; }
    bool check(BTContext& ctx) override;
    bool onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;

public:
    int32_t slot      = 0;
    int32_t stepIndex = 0;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

class CondAttackPipe : public BTCondition
{
public:
    typedef BTCondition Super;

public:
    CondAttackPipe();
    CondAttackPipe(int32_t slot, int32_t stepIndex, int32_t pipeIndex, int32_t modeIndex = 0);
    virtual ~CondAttackPipe();
    const char* typeName() const override { return "CondAttackPipe"; }
    bool check(BTContext& ctx) override;
    bool onEnter(BTContext& ctx) override;
    void onExit(BTContext& ctx) override;

public:
    int32_t slot      = 0;
    int32_t stepIndex = 0;
    int32_t pipeIndex = 0;
    int32_t modeIndex = 0;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

class CondAttackSlotIndex : public BTCondition
{
public:
    typedef BTCondition Super;

public:
    CondAttackSlotIndex();
    CondAttackSlotIndex(int32_t slot, int32_t slotIndex);
    virtual ~CondAttackSlotIndex();
    const char* typeName() const override { return "CondAttackSlotIndex"; }
    bool check(BTContext& ctx) override;

public:
    int32_t slot      = 0;
    int32_t slotIndex = 1;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

class CondAttackMode : public BTCondition
{
public:
    typedef BTCondition Super;

public:
    CondAttackMode();
    CondAttackMode(int32_t slot, int32_t slotIndex, int32_t modeIndex);
    virtual ~CondAttackMode();
    const char* typeName() const override { return "CondAttackMode"; }
    bool check(BTContext& ctx) override;

public:
    int32_t slot      = 0;
    int32_t slotIndex = 1;
    int32_t modeIndex = 0;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

class CondAttackToward : public BTCondition
{
public:
    typedef BTCondition Super;

public:
    CondAttackToward();
    explicit CondAttackToward(int32_t towardIndex);
    virtual ~CondAttackToward();
    const char* typeName() const override { return "CondAttackToward"; }
    bool check(BTContext& ctx) override;

public:
    int32_t towardIndex = 1;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;
    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
