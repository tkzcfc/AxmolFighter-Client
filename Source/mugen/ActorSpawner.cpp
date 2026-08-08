#include "ActorSpawner.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/io/FileUtils.h"

#include <algorithm>

NS_MG_BEGIN

namespace actor_spawner
{

namespace
{

std::string replaceExtension(const std::string& path, const std::string& newExt)
{
    const auto slash = path.find_last_of("/\\");
    const auto dot   = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return path + newExt;
    return path.substr(0, dot) + newExt;
}

std::string parentDir(const std::string& path)
{
    const auto slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
        return {};
    return path.substr(0, slash);
}

std::string toCitySpinePath(const std::string& spinePath)
{
    if (spinePath.empty() || spinePath.find("_city.") != std::string::npos)
        return spinePath;
    const auto slash = spinePath.find_last_of("/\\");
    const auto dot   = spinePath.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return spinePath + "_city";
    return spinePath.substr(0, dot) + "_city" + spinePath.substr(dot);
}

void applyIdentity(IdentityComponent* identityComp, JobType job, const ActorSpawnParams& params)
{
    identityComp->category = params.category;
    identityComp->job      = job;
    identityComp->playerId = params.playerId;
    identityComp->name     = std::string(params.name);
}

void fillAvatarFromRole(AvatarComponent* avatarComp,
                        const RoleConfig* role,
                        const ResSpineConfig* spine,
                        bool preferCity)
{
    avatarComp->roleConfig = role;
    avatarComp->roleId     = role ? role->id : 0;
    avatarComp->resSpine   = spine;
    if (role && role->behaviorTemplateId > 0)
        avatarComp->behaviorTemplate = Config::getInstance()->getBehaviorTemplateConfigById(role->behaviorTemplateId);
    else
        avatarComp->behaviorTemplate = Config::getInstance()->getBehaviorTemplateConfigById(1);

    if (spine && !spine->spine.empty())
    {
        avatarComp->spineSkeleton = spine->spine;
        avatarComp->spineAtlas    = !spine->atlas.empty() ? spine->atlas : replaceExtension(spine->spine, ".atlas");
        avatarComp->defaultSkin   = spine->defaultSkin;
        avatarComp->defaultAnimationPath = parentDir(spine->spine);
        if (spine->scale > 0.0f)
            avatarComp->spineScale = spine->scale;
        else if (spine->spine.find("/hero/") != std::string::npos)
            avatarComp->spineScale = 0.25f;
        else
            avatarComp->spineScale = 1.0f;

        if (preferCity)
        {
            const std::string citySkel  = toCitySpinePath(avatarComp->spineSkeleton);
            const std::string cityAtlas = replaceExtension(citySkel, ".atlas");
            if (citySkel != avatarComp->spineSkeleton && io::isFileExist(citySkel) && io::isFileExist(cityAtlas))
            {
                avatarComp->spineSkeleton        = citySkel;
                avatarComp->spineAtlas           = cityAtlas;
                avatarComp->defaultAnimationPath = parentDir(citySkel);
            }
        }
    }
    avatarComp->motionFile.clear();
}

JobType jobFromOccupation(int32_t occupationOrRoleId)
{
    switch (occupationOrRoleId)
    {
    case 1:
        return JobType::kSwordman;
    case 2:
        return JobType::kRanger;
    case 4:
        return JobType::kMage;
    default:
        return JobType::kUnknown;
    }
}

Entity* spawnRoleFullActorImpl(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params)
{
    auto* config = Config::getInstance();
    auto* role   = config->getRoleConfigById(roleId);
    if (!role)
    {
        MG_LOG_E("spawnRole: RoleConfig {} not found", roleId);
        return nullptr;
    }

    const ResSpineConfig* spine = nullptr;
    if (role->resSpineId > 0)
        spine = config->getResSpineConfigById(role->resSpineId);

    bool preferCity = false;
    if (auto* word = reinterpret_cast<GameWord*>(ecs->getUserdata()))
        preferCity = (word->getMode() == GameWordMode::kTown);

    auto actor            = ecs->newEntity();
    auto avatarComp       = MG_ADD_COMPONENT(actor, AvatarComponent);
    auto avatarRenderComp = MG_ADD_COMPONENT(actor, AvatarRenderComponent);
    auto inputComp        = MG_ADD_COMPONENT(actor, InputComponent);
    auto physicsComp      = MG_ADD_COMPONENT(actor, PhysicsComponent);
    auto transformComp    = MG_ADD_COMPONENT(actor, TransformComponent);
    auto identityComp     = MG_ADD_COMPONENT(actor, IdentityComponent);
    auto skillBarComp     = MG_ADD_COMPONENT(actor, SkillBarComponent);
    auto skillStateComp   = MG_ADD_COMPONENT(actor, SkillStateComponent);
    auto attributeComp    = MG_ADD_COMPONENT(actor, AttributeComponent);
    auto actorDataComp    = MG_ADD_COMPONENT(actor, ActorDataComponent);
    auto soundComp        = MG_ADD_COMPONENT(actor, SoundComponent);
    auto behaviorComp     = MG_ADD_COMPONENT(actor, BehaviorComponent);
    auto skillDeckComp    = MG_ADD_COMPONENT(actor, SkillDeckComponent);
    auto displacementComp = MG_ADD_COMPONENT(actor, DisplacementComponent);
    auto buffComp         = MG_ADD_COMPONENT(actor, BuffComponent);
    (void)avatarRenderComp;
    (void)inputComp;
    (void)skillStateComp;
    (void)soundComp;
    (void)displacementComp;
    (void)buffComp;

    fillAvatarFromRole(avatarComp, role, spine, preferCity);

    behaviorComp->roleConfig       = role;
    behaviorComp->behaviorTemplate = avatarComp->behaviorTemplate;
    behaviorComp->statusTags =
        StateTag::kTagGrounded | StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;

    attributeComp->baseAttribute    = role->attribute;
    attributeComp->currentAttribute = role->attribute;
    if (attributeComp->currentAttribute.hp <= 0)
        attributeComp->currentAttribute.hp = static_cast<float>(attributeComp->currentAttribute.hpMax);
    if (attributeComp->currentAttribute.mp <= 0)
        attributeComp->currentAttribute.mp = static_cast<float>(attributeComp->currentAttribute.mpMax);
    if (role->attribute.moveSpeed <= 0.0f && role->velocity > 0.0f)
    {
        attributeComp->baseAttribute.moveSpeed    = role->velocity * 1000.0f;
        attributeComp->currentAttribute.moveSpeed = role->velocity * 1000.0f;
    }
    attributeComp->bindVariable();

    physicsComp->isStaticBody = false;
    physicsComp->size.x       = static_cast<float>(role->size.x);
    physicsComp->size.y       = static_cast<float>(role->size.y);
    {
        const float moveSpeed      = attributeComp->currentAttribute.moveSpeed;
        physicsComp->maxVelocity.x = std::max(physicsComp->maxVelocity.x, moveSpeed);
        physicsComp->maxVelocity.y = std::max(physicsComp->maxVelocity.y, moveSpeed);
    }

    identityComp->monsterCamps = role->monsterCamps;
    applyIdentity(identityComp, jobFromOccupation(role->id), params);

    transformComp->position.x = x;
    transformComp->position.y = y;
    transformComp->scale.x    = 1.0f;
    transformComp->scale.y    = 1.0f;

    std::vector<int32_t> skillIds = role->defaultSkillIds;
    if (skillIds.empty() && (roleId == 1 || roleId == 101))
        skillIds.push_back(920000);

    SkillSlotItem slot;
    slot.slotIndex = static_cast<int32_t>(INPUT_SLOT_0);
    for (int32_t skillId : skillIds)
    {
        if (!config->getSkillAttackConfigById(skillId))
        {
            MG_LOG_E("spawnRole: skillAttack {} missing", skillId);
            continue;
        }
        SkillDeckEntry entry;
        entry.skillAttackId = skillId;
        entry.level         = 1;
        skillDeckComp->skills.push_back(entry);

        SkillInstanceData data;
        data.level = 1;
        data.buildFromSkillAttack(skillId);
        actorDataComp->skills.push_back(data);
        slot.skillIndexs.push_back(static_cast<int32_t>(actorDataComp->skills.size() - 1));
        skillDeckComp->slotSkillIndices.push_back({static_cast<int32_t>(skillDeckComp->skills.size() - 1)});
    }
    if (!slot.skillIndexs.empty())
        skillBarComp->skillSlots.push_back(slot);

    MG_LOG_W("spawnRole: role={} skills={}", roleId, actorDataComp->skills.size());
    return actor;
}

Entity* spawnRemoteRoleImpl(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params)
{
    auto* config = Config::getInstance();
    auto* role   = config->getRoleConfigById(roleId);
    if (!role)
        return nullptr;

    const ResSpineConfig* spine = nullptr;
    if (role->resSpineId > 0)
        spine = config->getResSpineConfigById(role->resSpineId);

    auto remote           = ecs->newEntity();
    auto avatarComp       = MG_ADD_COMPONENT(remote, AvatarComponent);
    auto avatarRenderComp = MG_ADD_COMPONENT(remote, AvatarRenderComponent);
    auto transformComp    = MG_ADD_COMPONENT(remote, TransformComponent);
    auto identityComp     = MG_ADD_COMPONENT(remote, IdentityComponent);
    (void)avatarRenderComp;

    fillAvatarFromRole(avatarComp, role, spine, false);
    transformComp->position.x  = x;
    transformComp->position.y  = y;
    transformComp->scale.x     = 1.0f;
    transformComp->scale.y     = 1.0f;
    identityComp->monsterCamps = role->monsterCamps;
    applyIdentity(identityComp, jobFromOccupation(role->id), params);
    return remote;
}

}  // namespace

Entity* spawnRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params)
{
    return spawnRoleFullActorImpl(ecs, roleId, x, y, params);
}

Entity* spawnRolePlayerActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y)
{
    ActorSpawnParams params;
    params.category = EntityCategory::kPlayer;
    params.name     = "Player";
    return spawnRoleFullActorImpl(ecs, roleId, x, y, params);
}

Entity* spawnRolePlayerActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params)
{
    ActorSpawnParams p = params;
    p.category         = EntityCategory::kPlayer;
    return spawnRoleFullActorImpl(ecs, roleId, x, y, p);
}

Entity* spawnRolePlayerActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const PlayerSpawnParams& params)
{
    return spawnRolePlayerActor(ecs, roleId, x, y, params.toActorParams());
}

Entity* spawnRemoteRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params)
{
    return spawnRemoteRoleImpl(ecs, roleId, x, y, params);
}

}  // namespace actor_spawner

NS_MG_END
