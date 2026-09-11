#include "AvatarRenderSystem.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"

#ifdef RUNTIME_IN_AXMOL
#    include "mugen/avatar/AvatarLayerUtils.h"
#    include "mugen/avatar/render/AvatarBuilder.h"
#    include "mugen/render/RenderObjectPool.h"

#    include "2d/RenderTexture.h"

#    include <algorithm>
#    include <cmath>
#endif

NS_MG_BEGIN

namespace
{
#ifdef RUNTIME_IN_AXMOL
constexpr int kAvatarMaxDriftMs = 100;
constexpr int kGhostIntervalMs  = 120;
constexpr float kGhostFadeSec   = 0.5f;

const ax::Color3B kGhostColors[] = {
    {255, 255, 255}, {0, 47, 127}, {102, 51, 204}, {216, 116, 16}, {160, 20, 255},
};

int modularDistance(int a, int b, int duration)
{
    if (duration <= 0)
        return std::abs(a - b);
    int d = std::abs(a - b) % duration;
    return std::min(d, duration - d);
}

void spawnGhostTrail(Avatar* avatar, ax::Node* entityNode, EntityId entityId)
{
    if (!avatar || !entityNode)
        return;

    const ax::Rect box = avatar->localSkeletonBounds();
    int width          = static_cast<int>(std::ceil(box.size.width));
    int height         = static_cast<int>(std::ceil(box.size.height));
    if (width < 2 || height < 2)
        return;
    width  = std::min(width, 512);
    height = std::min(height, 512);

    const ax::Vec2 savedPos = avatar->getPosition();
    const float savedSx     = avatar->getScaleX();
    const float savedSy     = avatar->getScaleY();

    auto* rt = ax::RenderTexture::create(width, height, ax::backend::PixelFormat::RGBA8);
    if (!rt)
        return;
    rt->setCascadeOpacityEnabled(true);

    avatar->setScale(1.0f, 1.0f);
    avatar->setPosition(-box.origin.x, -box.origin.y);

    ax::Director* director = ax::Director::getInstance();
    rt->beginWithClear(0.0f, 0.0f, 0.0f, 0.0f);
    avatar->visit(director->getRenderer(), ax::Mat4::IDENTITY, ax::Node::FLAGS_TRANSFORM_DIRTY);
    rt->end();

    avatar->setPosition(savedPos);
    avatar->setScaleX(savedSx);
    avatar->setScaleY(savedSy);

    ax::Sprite* sprite = rt->getSprite();
    sprite->setAnchorPoint(ax::Vec2(0.5f, 0.0f));
    sprite->setPosition(savedPos);
    sprite->setScaleX(savedSx >= 0.0f ? 1.0f : -1.0f);
    sprite->setBlendFunc(ax::BlendFunc::ADDITIVE);
    sprite->setOpacityModifyRGB(false);
    sprite->setColor(kGhostColors[static_cast<size_t>(entityId) % (sizeof(kGhostColors) / sizeof(kGhostColors[0]))]);

    rt->setPosition(ax::Vec2::ZERO);
    entityNode->addChild(rt, avatar->getLocalZOrder() - 1);
    rt->runAction(ax::Sequence::create(ax::FadeTo::create(kGhostFadeSec, 0), ax::RemoveSelf::create(), nullptr));
}
#endif
}  // namespace

AvatarRenderSystem::AvatarRenderSystem() {}
AvatarRenderSystem::~AvatarRenderSystem() {}

void AvatarRenderSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, AvatarComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, AvatarRenderComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, TransformComponent);
}

#ifdef RUNTIME_IN_AXMOL

void AvatarRenderSystem::update()
{
    auto directorComp          = MG_GET_COMPONENT(this->getGameWord()->getDirector(), DirectorComponent);
    auto mapEntity             = getECSManager()->getEntity(directorComp->mapEntityId);
    auto mapRenderComp         = MG_GET_COMPONENT(mapEntity, GameMapRenderComponent);
    const int lastUpdateTimeMs = getECSManager()->getLastUpdateTimeMs();

    for (auto& entity : entities)
    {
        auto avatarRenderComp = MG_GET_COMPONENT(entity, AvatarRenderComponent);
        auto avatarComp       = MG_GET_COMPONENT(entity, AvatarComponent);
        auto transformComp    = MG_GET_COMPONENT(entity, TransformComponent);

        Avatar* avatar = avatarRenderComp->avatar;
        if (!avatar)
            continue;

        avatar->setPosition(static_cast<float>(transformComp->position.x),
                            static_cast<float>(transformComp->position.y + transformComp->position.z));
        // 逻辑 Transform 是节点变换的唯一权威：scale.x 为负即水平镜像
        // spine 缩放在 Skeleton 上，这里只镜像/逻辑缩放，不要把影子系数套到角色身上
        avatar->setScaleX(transformComp->scale.x);
        avatar->setScaleY(transformComp->scale.y);
        avatar->setLocalZOrder(static_cast<int>(-transformComp->position.y));

        // 残影：引用计数 >0 时按间隔抓当前骨骼剪影
        if (avatarComp->ghostRefCount > 0 && mapRenderComp->entityNode)
        {
            avatarComp->ghostAccumMs += lastUpdateTimeMs;
            if (avatarComp->ghostAccumMs >= kGhostIntervalMs)
            {
                avatarComp->ghostAccumMs = 0;
                spawnGhostTrail(avatar, mapRenderComp->entityNode, entity->getId());
            }
        }
        else
        {
            avatarComp->ghostAccumMs = 0;
        }

        auto& playback = avatarComp->playback;
        if (!playback.getCurrentMotionName().empty())
        {
            if (playback.getCurrentMotionName() != avatarRenderComp->syncedMotion ||
                playback.getCurrentEntryId() != avatarRenderComp->syncedEntry)
            {
                avatar->setMotion(playback.getCurrentMotionName(), playback.getCurrentEntryId(), playback.isLoop());
                avatar->seek(playback.getCurrentTimeMs());
                avatarRenderComp->syncedMotion = playback.getCurrentMotionName();
                avatarRenderComp->syncedEntry  = playback.getCurrentEntryId();
                avatarComp->animationFinished  = playback.isFinished();
            }
            else
            {
                const int dtMs = static_cast<int>(lastUpdateTimeMs * avatarComp->animationSpeed);
                if (dtMs > 0)
                    avatar->step(dtMs);

                // 逻辑时长为 0 时无法对齐（否则会反复 seek 回 0，表现为播几帧就抽回）
                const int dur = playback.getDurationMs();
                if (dur > 0)
                {
                    int drift = 0;
                    if (playback.isLoop())
                        drift = modularDistance(avatar->getCurrentTimeMs(), playback.getCurrentTimeMs(), dur);
                    else
                        drift = std::abs(avatar->getCurrentTimeMs() - playback.getCurrentTimeMs());

                    if (drift > kAvatarMaxDriftMs)
                        avatar->seek(playback.getCurrentTimeMs());
                }
            }
        }

        if (directorComp->debugDrawCollisionBox)
        {
            for (const auto& damageBox : avatarComp->getDamageBoxes())
            {
                damageBox.drawDebug(mapRenderComp->actorDebugDrawNode, ax::Color4F(0.2f, 0.9f, 0.3f, 1.0f),
                                    ax::Color4F(0.2f, 0.9f, 0.3f, 0.45f));
            }
            for (const auto& attackBox : avatarComp->getAttackBoxes())
            {
                attackBox.drawDebug(mapRenderComp->actorDebugDrawNode, ax::Color4F(1.0f, 0.35f, 0.1f, 1.0f),
                                    ax::Color4F(1.0f, 0.35f, 0.1f, 0.45f));
            }
        }
    }
}

void AvatarRenderSystem::onEntityAdded(Entity* entity)
{
    auto avatarComp       = MG_GET_COMPONENT(entity, AvatarComponent);
    auto avatarRenderComp = MG_GET_COMPONENT(entity, AvatarRenderComponent);
    auto transformComp    = MG_GET_COMPONENT(entity, TransformComponent);

    auto directorComp  = MG_GET_COMPONENT(this->getGameWord()->getDirector(), DirectorComponent);
    auto mapEntity     = getECSManager()->getEntity(directorComp->mapEntityId);
    auto mapRenderComp = MG_GET_COMPONENT(mapEntity, GameMapRenderComponent);

    MG_ASSERT(!avatarRenderComp->avatar && "AvatarRenderComponent should not have avatar when added.");
    MG_ASSERT(mapRenderComp && mapRenderComp->entityNode);

    auto* pool = RenderObjectPool::getInstance();
    if (getECSManager()->isDeserialized())
    {
        const auto avatarKey = RenderStashKey(entity->getId()) << "avatar" << (avatarComp->roleId);
        if (auto* avatar = pool->acquireNode<Avatar>(avatarKey))
        {
            avatarRenderComp->avatar = avatar;
            avatarRenderComp->syncedMotion.clear();
            avatarRenderComp->syncedEntry.clear();
            avatar->setPosition(static_cast<float>(transformComp->position.x),
                                static_cast<float>(transformComp->position.y + transformComp->position.z));
            mapRenderComp->entityNode->addChild(avatar);
            return;
        }
    }

    avatarRenderComp->avatar = AvatarBuilder::createAvatar(avatarComp);
    if (!avatarRenderComp->avatar)
    {
        MG_LOG_E("AvatarRenderSystem: AvatarBuilder failed");
        return;
    }

    Avatar* avatar = avatarRenderComp->avatar;
    avatar->setPosition(static_cast<float>(transformComp->position.x),
                        static_cast<float>(transformComp->position.y + transformComp->position.z));
    mapRenderComp->entityNode->addChild(avatar);

    avatarRenderComp->syncedMotion.clear();
    avatarRenderComp->syncedEntry.clear();
}

void AvatarRenderSystem::onEntityRemoved(Entity* entity)
{
    auto avatarRenderComp = MG_GET_COMPONENT(entity, AvatarRenderComponent);
    if (!avatarRenderComp->avatar)
    {
        return;
    }

    if (getECSManager()->isDeserialized())
    {
        auto avatarComp      = MG_GET_COMPONENT(entity, AvatarComponent);
        const auto avatarKey = RenderStashKey(entity->getId()) << "avatar" << (avatarComp ? avatarComp->roleId : 0);
        RenderObjectPool::getInstance()->recycleNode(avatarKey, avatarRenderComp->avatar);
        avatarRenderComp->avatar = nullptr;
        avatarRenderComp->syncedMotion.clear();
        avatarRenderComp->syncedEntry.clear();
        return;
    }

    avatarRenderComp->avatar->removeFromParent();
    avatarRenderComp->avatar = nullptr;
    avatarRenderComp->syncedMotion.clear();
    avatarRenderComp->syncedEntry.clear();
}

#else

void AvatarRenderSystem::update() {}

void AvatarRenderSystem::onEntityAdded(Entity* entity) {}

void AvatarRenderSystem::onEntityRemoved(Entity* entity) {}

#endif

NS_MG_END
