#include "PhysicsSystem.h"

#include "mugen/Components.h"
#include "mugen/GameWord.h"

NS_MG_BEGIN

PhysicsSystem::PhysicsSystem() {}
PhysicsSystem::~PhysicsSystem() {}

void PhysicsSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, PhysicsComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, TransformComponent);
}

void PhysicsSystem::onEntityAdded(Entity* entity)
{
    if (getECSManager()->isDeserialized())
    {
        return;
    }

    auto physicsComp   = MG_GET_COMPONENT(entity, PhysicsComponent);
    auto transformComp = MG_GET_COMPONENT(entity, TransformComponent);
    if (physicsComp == nullptr || transformComp == nullptr)
    {
        return;
    }

    physicsComp->position.x = static_cast<float>(transformComp->position.x);
    physicsComp->position.y = static_cast<float>(transformComp->position.y);
    physicsComp->position.z = static_cast<float>(transformComp->position.z);
    physicsComp->onGround   = physicsComp->position.z <= 0 ? 1 : 0;
}

void PhysicsSystem::onEntityRemoved(Entity* entity) {}

void PhysicsSystem::update()
{
    float deltaTimeSec = getECSManager()->getLastUpdateTimeMs() / 1000.0f;
    for (auto& entity : entities)
    {
        auto physicsComp = MG_GET_COMPONENT(entity, PhysicsComponent);
        // 只对非静态物体应用物理更新
        if (physicsComp->isStaticBody)
            continue;

        // 顿帧：跳过积分
        if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        {
            if (attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0)
                continue;
        }

        auto transformComp = MG_GET_COMPONENT(entity, TransformComponent);

        // 1. 记录上一帧位置（碰撞回退时使用）
        physicsComp->lastPosition = physicsComp->position;

        // 2. 重力（仅空中施加，受 gravityScale 缩放）
        if (!physicsComp->onGround)
        {
            physicsComp->velocity.z -= physicsComp->gravity * physicsComp->gravityScale * deltaTimeSec;
        }

        // 3. 落地后对冲量速度 x/y 施加摩擦衰减
        // 使用指数衰减保证帧率无关：friction 语义为「每秒保留比例的自然对数系数」。
        // 例：friction=5 → 1秒后剩余 e^-5 ≈ 0.7%，约 0.15s 内衰减到近似停止。
        if (physicsComp->onGround)
        {
            float decay = std::exp(-physicsComp->friction * deltaTimeSec);
            physicsComp->impulseVelocity.x *= decay;
            physicsComp->impulseVelocity.y *= decay;
        }

        // 4. 限制主动移动速度（velocity）大小
        auto clampF = [](float v, float maxAbs) { return v > maxAbs ? maxAbs : (v < -maxAbs ? -maxAbs : v); };
        physicsComp->velocity.x = clampF(physicsComp->velocity.x, physicsComp->maxVelocity.x);
        physicsComp->velocity.y = clampF(physicsComp->velocity.y, physicsComp->maxVelocity.y);
        physicsComp->velocity.z = clampF(physicsComp->velocity.z, physicsComp->maxVelocity.z);

        // 5. 位置积分（主动移动 + 冲量叠加）
        float vx = physicsComp->velocity.x + physicsComp->impulseVelocity.x;
        float vy = physicsComp->velocity.y + physicsComp->impulseVelocity.y;
        float vz = physicsComp->velocity.z + physicsComp->impulseVelocity.z;
        physicsComp->position.x += vx * deltaTimeSec;
        physicsComp->position.y += vy * deltaTimeSec;
        physicsComp->position.z += vz * deltaTimeSec;

        // 6. 地面解析：z 低于 groundLevel 时吸附并清零竖直速度
        if (physicsComp->position.z <= physicsComp->groundLevel)
        {
            physicsComp->position.z        = physicsComp->groundLevel;
            physicsComp->velocity.z        = 0.0f;
            physicsComp->impulseVelocity.z = 0.0f;
            physicsComp->onGround          = 1;
        }
        else
        {
            physicsComp->onGround = 0;
        }

        // 7. 地图边界约束（x/y）
        float mapMinX = mapMin.x + physicsComp->size.x * 0.5f;
        float mapMaxX = mapMax.x - physicsComp->size.x * 0.5f;
        float mapMinY = mapMin.y + physicsComp->size.y * 0.5f;
        float mapMaxY = mapMax.y - physicsComp->size.y * 0.5f;

        if (physicsComp->position.x < mapMinX)
            physicsComp->position.x = mapMinX;
        else if (physicsComp->position.x > mapMaxX)
            physicsComp->position.x = mapMaxX;

        if (physicsComp->position.y < mapMinY)
            physicsComp->position.y = mapMinY;
        else if (physicsComp->position.y > mapMaxY)
            physicsComp->position.y = mapMaxY;

        // 8. 同步回TransformComponent的整数坐标
        transformComp->position.x = static_cast<int32_t>(physicsComp->position.x);
        transformComp->position.y = static_cast<int32_t>(physicsComp->position.y);
        transformComp->position.z = static_cast<int32_t>(physicsComp->position.z);
    }

#if RUNTIME_IN_AXMOL
    auto directorComp = MG_GET_COMPONENT(this->getGameWord()->getDirector(), DirectorComponent);
    if (directorComp->debugDrawGroundBox)
    {
        auto mapEntity     = getECSManager()->getEntity(directorComp->mapEntityId);
        auto mapRenderComp = MG_GET_COMPONENT(mapEntity, GameMapRenderComponent);

        auto debugDrawNode = mapRenderComp->groundDebugDrawNode;
        if (debugDrawNode)
        {
            debugDrawNode->drawSolidRect(ax::Vec2(mapMin.x, mapMin.y), ax::Vec2(mapMax.x, mapMax.y),
                                         ax::Color4F(0.0f, 0.0f, 0.8f, 0.15f), 1.0f,
                                         ax::Color4F(0.0f, 0.0f, 0.8f, 1.0f));
        }
    }
#endif
}

NS_MG_END
