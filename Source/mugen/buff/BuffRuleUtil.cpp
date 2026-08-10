#include "mugen/buff/BuffRuleUtil.h"

#include "mugen/Components.h"
#include "mugen/conf/Config.h"
#include "mugen/core/ecs/ECSManager.h"
#include "mugen/core/ecs/Entity.h"
#include "mugen/core/math/Random.h"

#include <algorithm>

NS_MG_BEGIN

namespace BuffRuleUtil
{

namespace
{
std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return path + newExt;
    return path.substr(0, dot) + newExt;
}
}  // namespace

float param(const BuffConfig* cfg, size_t index, float fallback)
{
    if (!cfg || index >= cfg->paramValue.size())
        return fallback;
    return cfg->paramValue[index];
}

bool passTriggerGates(Entity* entity, BuffInstance& inst, const BuffConfig* cfg, int32_t skillId)
{
    (void)entity;
    if (!cfg)
        return true;

    if (cfg->binding != 0)
    {
        const int32_t bindId = inst.sourceSkillId;
        if (bindId > 0 && skillId > 0 && bindId != skillId)
            return false;
    }

    if (inst.innerCdMs > 0)
        return false;

    if (cfg->probability < 100)
    {
        Random rng(static_cast<uint64_t>(inst.buffId) ^ static_cast<uint64_t>(inst.remainingMs) ^
                   static_cast<uint64_t>(skillId));
        if (rng.nextInt(1, 100) > cfg->probability)
            return false;
    }

    if (cfg->innerCd > 0)
        inst.innerCdMs = cfg->innerCd;

    return true;
}

void modifyExtend(Entity* entity, ExtendAttributeType type, float delta)
{
    if (!entity || delta == 0.0f)
        return;
    if (auto* attr = MG_GET_COMPONENT(entity, AttributeComponent))
        attr->extendAttribute.modify(type, delta);
}

SkillDeckEntry* findDeckEntry(Entity* entity, int32_t skillAttackId)
{
    auto* deck = MG_GET_COMPONENT(entity, SkillDeckComponent);
    if (!deck || skillAttackId <= 0)
        return nullptr;
    for (auto& e : deck->skills)
    {
        if (e.skillAttackId == skillAttackId)
            return &e;
    }
    return nullptr;
}

void attachSpine(Entity* entity, BuffInstance& inst)
{
    if (!entity || inst.vfxEntityId != 0)
        return;
    const auto* cfg = Config::getInstance()->getBuffConfigById(inst.buffId);
    if (!cfg || cfg->spineId <= 0)
        return;
    const auto* spine = Config::getInstance()->getResSpineConfigById(cfg->spineId);
    if (!spine)
        return;
    auto* ecs = entity->getECSManager();
    if (!ecs)
        return;

    auto* ownerTf = MG_GET_COMPONENT(entity, TransformComponent);
    auto* vfx     = ecs->newEntity();
    auto* tf      = MG_ADD_COMPONENT(vfx, TransformComponent);
    auto* fx      = MG_ADD_COMPONENT(vfx, EffectLifetimeComponent);

    fx->ownerId    = entity->getId();
    fx->follow     = true;
    fx->lifetimeMs = 0;
    fx->relativePosition =
        Vector3f(cfg->spineOffsets.size() > 0 ? cfg->spineOffsets[0] : 0.0f,
                 cfg->spineOffsets.size() > 1 ? cfg->spineOffsets[1] : 0.0f, 0.0f);

    if (ownerTf)
    {
        tf->position        = ownerTf->position;
        tf->facingDirection = ownerTf->facingDirection;
    }

    auto* avatar = MG_ADD_COMPONENT(vfx, AvatarComponent);
    MG_ADD_COMPONENT(vfx, AvatarRenderComponent);
    avatar->resSpine = spine;
    if (!spine->spine.empty())
    {
        avatar->spineSkeleton = spine->spine;
        avatar->spineAtlas =
            !spine->atlas.empty() ? spine->atlas : replaceExtension(spine->spine, ".atlas");
        avatar->defaultSkin = spine->defaultSkin;
        avatar->spineScale  = spine->scale > 0.0f ? spine->scale : 1.0f;
        const auto slash    = spine->spine.find_last_of("/\\");
        avatar->defaultAnimationPath =
            slash == std::string::npos ? std::string{} : spine->spine.substr(0, slash);
    }

    vfx->notifyEntityReady();
    inst.vfxEntityId = static_cast<int32_t>(vfx->getId());
}

void detachSpine(Entity* entity, BuffInstance& inst)
{
    if (inst.vfxEntityId == 0)
        return;
    if (entity)
    {
        if (auto* ecs = entity->getECSManager())
            ecs->destroyEntityById(static_cast<EntityId>(inst.vfxEntityId));
    }
    inst.vfxEntityId = 0;
}

}  // namespace BuffRuleUtil

NS_MG_END
