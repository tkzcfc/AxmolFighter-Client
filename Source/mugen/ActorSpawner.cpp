#include "ActorSpawner.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/io/FileUtils.h"

#include <algorithm>
#include <unordered_set>

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

// 将 root 的 nextSkill 链展开为有序 skillAttackId 列表（去重，便于绑到 INPUT_SLOT_0..N）
void appendSkillChain(Config* config,
                      int32_t rootId,
                      std::vector<int32_t>& out,
                      std::unordered_set<int32_t>& seen,
                      int32_t maxCount)
{
    int32_t id = rootId;
    while (id > 0 && static_cast<int32_t>(out.size()) < maxCount)
    {
        if (!seen.insert(id).second)
            break;
        const auto* atk = config->getSkillAttackConfigById(id);
        if (!atk)
            break;
        out.push_back(id);
        id = atk->nextSkill > 0 ? atk->nextSkill : 0;
    }
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
    attributeComp->epMax            = 100.0f;
    attributeComp->ep               = attributeComp->epMax;
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

    std::vector<int32_t> roots = role->defaultSkillIds;
    if (roots.empty() && (roleId == 1 || roleId == 101 || roleId == 102 || roleId == 103))
        roots.push_back(920000);

    constexpr int32_t kMaxSkillSlots = static_cast<int32_t>(INPUT_SLOT_10) - static_cast<int32_t>(INPUT_SLOT_0) + 1;

    // 按 nextSkill 链展开，再追加高 sorder 技能便于打断测试：A/1/2… 各绑一个
    std::vector<int32_t> skillIds;
    std::unordered_set<int32_t> seen;
    skillIds.reserve(static_cast<size_t>(kMaxSkillSlots));
    for (int32_t root : roots)
        appendSkillChain(config, root, skillIds, seen, kMaxSkillSlots);

    // 额外根技能：920041(sorder80) / 920050 / 920080（含 ignore-order）
    static constexpr int32_t kExtraTestRoots[] = {920041, 920050, 920080};
    for (int32_t extra : kExtraTestRoots)
    {
        if (static_cast<int32_t>(skillIds.size()) >= kMaxSkillSlots)
            break;
        appendSkillChain(config, extra, skillIds, seen, kMaxSkillSlots);
    }

    skillDeckComp->slotSkillIndices.clear();
    skillBarComp->skillSlots.clear();

    int32_t boundCount = 0;
    for (int32_t skillId : skillIds)
    {
        if (boundCount >= kMaxSkillSlots)
        {
            MG_LOG_W("spawnRole: role={} skill overflow, drop skillAttack {}", roleId, skillId);
            break;
        }
        if (!config->getSkillAttackConfigById(skillId))
        {
            MG_LOG_E("spawnRole: skillAttack {} missing", skillId);
            continue;
        }

        const auto* skillAtk = config->getSkillAttackConfigById(skillId);
        SkillDeckEntry entry;
        entry.skillAttackId     = skillId;
        entry.nextSkillAttackId = skillAtk ? skillAtk->nextSkill : -1;
        entry.level             = 1;
        entry.coolDownMaxMs     = skillAtk && skillAtk->cd > 0 ? skillAtk->cd : 0;
        entry.coolDownMs        = 0;
        entry.releaseMax        = skillAtk && skillAtk->cdCount > 0 ? skillAtk->cdCount : 1;
        entry.releaseCount      = entry.releaseMax;
        skillDeckComp->skills.push_back(entry);
        const int32_t deckIndex = static_cast<int32_t>(skillDeckComp->skills.size() - 1);

        SkillInstanceData data;
        data.level = 1;
        data.buildFromSkillAttack(skillId);
        actorDataComp->skills.push_back(data);
        const int32_t actorSkillIndex = static_cast<int32_t>(actorDataComp->skills.size() - 1);

        // 顺序：INPUT_SLOT_0(A), INPUT_SLOT_1(1), … INPUT_SLOT_10(0)
        SkillSlotItem slot;
        slot.slotIndex = static_cast<int32_t>(INPUT_SLOT_0) + boundCount;
        slot.skillIndexs.push_back(actorSkillIndex);
        skillBarComp->skillSlots.push_back(slot);
        skillDeckComp->slotSkillIndices.push_back({deckIndex});
        ++boundCount;
    }

    // 跑中突刺：默认取第二个绑定技能
    if (skillDeckComp->skills.size() >= 2)
        behaviorComp->thrustSkillAttackId = skillDeckComp->skills[1].skillAttackId;
    else
        behaviorComp->thrustSkillAttackId = 0;

    MG_LOG_W("spawnRole: role={} skills={} slots={} thrust={}", roleId, actorDataComp->skills.size(),
             skillBarComp->skillSlots.size(), behaviorComp->thrustSkillAttackId);
    for (int32_t i = 0; i < boundCount; ++i)
    {
        const int32_t slot = static_cast<int32_t>(INPUT_SLOT_0) + i;
        const int32_t sid  = skillDeckComp->skills[static_cast<size_t>(i)].skillAttackId;
        MG_LOG_W("  bind INPUT_SLOT_{}({}) -> skill {}", i, slot, sid);
    }
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

int32_t resolvePlayableRoleId(int32_t classOrRoleId)
{
    // 服务器 class_id 是 JobType，不是 RoleConfig id
    constexpr int32_t kDefaultHeroRoleId = 101;
    switch (classOrRoleId)
    {
    case static_cast<int32_t>(JobType::kSwordman):
        return 101;
    case static_cast<int32_t>(JobType::kRanger):
        return 102;
    case static_cast<int32_t>(JobType::kMage):
        return 103;
    default:
        break;
    }

    if (classOrRoleId <= 0)
        return kDefaultHeroRoleId;

    auto* config = Config::getInstance();
    const auto* role = config->getRoleConfigById(classOrRoleId);
    if (!role || role->resSpineId <= 0)
        return kDefaultHeroRoleId;

    const auto* spine = config->getResSpineConfigById(role->resSpineId);
    if (!spine || spine->spine.empty())
        return kDefaultHeroRoleId;

    // 英雄资源路径约定：mugen/spine/hero/...
    if (spine->spine.find("/hero/") == std::string::npos)
        return kDefaultHeroRoleId;

    return classOrRoleId;
}

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
