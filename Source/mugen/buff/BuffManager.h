#pragma once

#include "mugen/buff/BFEvent.h"
#include "mugen/buff/Buff.h"

#include <memory>
#include <vector>

NS_MG_BEGIN

class Entity;
class BuffRuleBase;

class BuffManager : public Object
{
public:
    typedef Object Super;

public:
    BuffManager() {}

    virtual ~BuffManager() {}

    const char* typeName() const { return "BuffManager"; }

    void update(Entity* entity, int32_t dtMs);

    bool addBuff(Entity* entity, int32_t buffId, int32_t sourceSkillId = 0, int32_t level = 1);

    void removeBuff(Entity* entity, int32_t buffId);

    void trigger(Entity* entity, BFEvent event, Entity* other = nullptr, int32_t skillId = 0, float param = 0.0f);

    Buff* findBuff(int32_t buffId) const;

    static BuffManager* of(Entity* entity);

    static BuffRuleBase* resolveRule(const Buff& buff);

public:
    std::vector<std::unique_ptr<Buff>> buffs;
    int32_t invincibleRef = 0;
    int32_t superArmorRef = 0;
    int32_t stunRef       = 0;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl, deserializeCustomImpl, invincibleRef, superArmorRef, stunRef)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
