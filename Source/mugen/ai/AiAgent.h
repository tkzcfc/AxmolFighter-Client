#pragma once

#include "mugen/ai/SkillAi.h"
#include "mugen/core/math/Vec3.h"

#include <cstddef>
#include <memory>
#include <vector>

NS_MG_BEGIN

class Entity;
class ECSManager;
class TransformComponent;
class Random;
class AiConfig;

class AiAgent : public Object
{
public:
    typedef Object Super;

public:
    AiAgent() {}

    virtual ~AiAgent() {}

    const char* typeName() const { return "AiAgent"; }

    void bindConfig(Entity* entity);

    void update(Entity* entity, int32_t dtMs);

    SkillAi* findSkillAi(int32_t skillAiId) const;

    SkillAi* skillAiForSlot(size_t slot) const;

    const AiConfig* config() const;

    static AiAgent* of(Entity* entity);

    static Entity* findNearestPlayer(ECSManager* ecs, const TransformComponent* selfTf, bool skipDead = false);

    static Random& worldRandom(ECSManager* ecs);

    static const AiConfig* resolveConfig(Entity* entity);

public:
    std::vector<std::unique_ptr<SkillAi>> skillAis;
    std::vector<int32_t> skillSlotIntervalMs;
    int32_t aiConfigId = 0;
    Vector3f spawnPosition;
    int32_t patrolTargetX      = 0;
    int32_t patrolTargetY      = 0;
    int32_t patrolWaitRemainMs = 0;
    int32_t patrolState        = 0;
    int32_t patrolScope        = 0;
    int8_t moveDirX            = 0;
    int8_t moveDirY            = 0;
    int32_t alertRemainMs      = 0;
    bool alertDone             = false;
    bool pathFindingActive     = false;

public:
    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  skillSlotIntervalMs,
                                  aiConfigId,
                                  spawnPosition,
                                  patrolTargetX,
                                  patrolTargetY,
                                  patrolWaitRemainMs,
                                  patrolState,
                                  patrolScope,
                                  moveDirX,
                                  moveDirY,
                                  alertRemainMs,
                                  alertDone,
                                  pathFindingActive)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
