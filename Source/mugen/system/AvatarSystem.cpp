#include "AvatarSystem.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"

#include "mugen/avatar/AvatarLayerUtils.h"
#include "mugen/avatar/DamageBoxTransform.h"
#include "mugen/system/SoundSystem.h"

NS_MG_BEGIN

namespace
{

void dispatchCombatEvents(ECSManager* ecs, Entity* entity, const std::vector<const CombatEvent*>& events)
{
    if (events.empty())
        return;
    auto* soundSystem = MG_GET_SYSTEM(ecs, SoundSystem);
    for (const CombatEvent* event : events)
    {
        if (!event)
            continue;
        const std::string& type = event->getType();
        if (type == "sound")
        {
            if (soundSystem && !event->getValue().empty())
                soundSystem->play(event->getValue(), entity);
            else if (!event->getValue().empty())
                MG_LOG_W("AvatarSystem: sound event but SoundSystem missing, key='{}'", event->getValue());
        }
        else if (type == "hitPoint")
        {
            MG_LOG_D("AvatarSystem: hitPoint event at {}ms value='{}'", event->getTimeMs(), event->getValue());
        }
        else
        {
            MG_LOG_D("AvatarSystem: ignore unknown combat event type='{}' value='{}'", type, event->getValue());
        }
    }
}

void updateHitboxesFromPlayback(AvatarComponent* avatarComp, const TransformComponent* transformComp)
{
    avatarComp->attackBoxes.clear();
    avatarComp->damageBoxes.clear();

    float contentScale = 1.0f;
    if (avatarComp)
        contentScale = avatarComp->getSpineScale();

    std::vector<const DamageBox*> atk;
    std::vector<const DamageBox*> dmg;
    avatarComp->playback.boxesAt(atk, dmg);
    avatarComp->attackBoxes.reserve(atk.size());
    avatarComp->damageBoxes.reserve(dmg.size());
    for (const DamageBox* box : atk)
    {
        if (box)
            avatarComp->attackBoxes.push_back(combat_box::transformDamageBoxToWorld(*box, transformComp, contentScale));
    }
    for (const DamageBox* box : dmg)
    {
        if (box)
            avatarComp->damageBoxes.push_back(combat_box::transformDamageBoxToWorld(*box, transformComp, contentScale));
    }
}

bool initNewAvatarPlayback(AvatarComponent* avatarComp, Entity* entity)
{
    if (!avatarComp)
    {
        MG_LOG_E("AvatarSystem: AvatarComponent null, cannot init MotionPlayer");
        return false;
    }

    avatarComp->playback.clearLayers();
    const auto defs = AvatarLayerUtils::resolveLayers(avatarComp);
    for (const auto& def : defs)
    {
        if (!avatarComp->playback.addLayer(def))
            MG_LOG_W("AvatarSystem: addLayer failed motion='{}'", def.motionMapPath);
    }

    if (avatarComp->playback.layerCount() == 0)
        MG_LOG_W("AvatarSystem: MotionPlayer has no layers for role={}", avatarComp->roleId);

    return true;
}

}  // namespace

AvatarSystem::AvatarSystem() {}

AvatarSystem::~AvatarSystem() {}

void AvatarSystem::init(ECSManager* ecs)
{
    Super::init(ecs);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, AvatarComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, TransformComponent);
    MG_SYSTEM_ADD_REQUIRED_COMPONENT(this, ecs, IdentityComponent);
}

void AvatarSystem::update()
{
    auto lastUpdateTimeMs = getECSManager()->getLastUpdateTimeMs();

    for (auto& entity : entities)
    {
        auto avatarComp    = MG_GET_COMPONENT(entity, AvatarComponent);
        auto transformComp = MG_GET_COMPONENT(entity, TransformComponent);

        // 同步朝向缩放（逻辑帧计算，渲染层读取即可）
        float scalex           = std::fabs(transformComp->scale.x);
        transformComp->scale.x = transformComp->facingDirection == FacingDirection::kFacingRight ? scalex : -scalex;

        auto& playback = avatarComp->playback;

        // 顿帧：停动画推进（表现停帧）
        bool frozen = false;
        if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
            frozen = attr->freezeRemainingMs > 0 && attr->freezeDelayMs <= 0;

        if (!frozen)
        {
            const int dtMs = static_cast<int>(lastUpdateTimeMs * avatarComp->animationSpeed);
            std::vector<const CombatEvent*> events;
            playback.step(dtMs, &events);
            dispatchCombatEvents(getECSManager(), entity, events);
        }

        updateHitboxesFromPlayback(avatarComp, transformComp);
        avatarComp->animationFinished = playback.isFinished();
    }
}

void AvatarSystem::onEntityAdded(Entity* entity)
{
    auto avatarComp = MG_GET_COMPONENT(entity, AvatarComponent);

    if (getECSManager()->isDeserialized())
    {
        MG_ASSERT(avatarComp->playback.layerCount() > 0);
        return;
    }

    initNewAvatarPlayback(avatarComp, entity);
}

void AvatarSystem::onEntityRemoved(Entity* entity)
{
    auto avatarComp = MG_GET_COMPONENT(entity, AvatarComponent);
    avatarComp->playback.stop();
    avatarComp->playback.clearLayers();
}

NS_MG_END
