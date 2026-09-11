#include "ActorSpawner.h"
#include "mugen/Components.h"
#include "mugen/GameWord.h"
#include "mugen/ai/AiAgent.h"
#include "mugen/conf/Config.h"
#include "mugen/conf/GameDef.h"
#include "mugen/core/io/FileUtils.h"
#include "mugen/skill/SkillManager.h"
#include "mugen/avatar/AvatarLayerUtils.h"
#include "mugen/common/TypeConversions.h"

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

// 收集 root 的 nextSkill 链（同槽多段），不去重进全局槽列表
void collectSkillChain(Config* config, int32_t rootId, std::vector<int32_t>& chainOut)
{
    chainOut.clear();
    std::unordered_set<int32_t> seen;
    int32_t id = rootId;
    while (id > 0)
    {
        if (!seen.insert(id).second)
            break;
        const auto* atk = config->getSkillAttackConfigById(id);
        if (!atk)
            break;
        chainOut.push_back(id);
        id = atk->nextSkill > 0 ? atk->nextSkill : 0;
    }
}

void fillAvatarFromRole(AvatarComponent* avatarComp,
                        const RoleConfig* role,
                        const ResSpineConfig* spine,
                        bool preferCity)
{
    avatarComp->roleConfig = role;
    avatarComp->roleId     = role ? role->id : 0;
    avatarComp->resSpine   = spine;
    {
        const int32_t tmplId         = (role && role->roleType != EntityRoleType::kHero) ? 2 : 1;
        avatarComp->behaviorTemplate = Config::getInstance()->getBehaviorTemplateConfigById(tmplId);
    }

    if (spine && !spine->spine.empty())
    {
        avatarComp->spineSkeleton = spine->spine;
        avatarComp->spineAtlas    = replaceExtension(spine->spine, ".atlas");
        avatarComp->defaultSkin.clear();
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
                avatarComp->spineSkeleton = citySkel;
                avatarComp->spineAtlas    = cityAtlas;
            }
        }
        avatarComp->motionFile = AvatarLayerUtils::spinePathToMotionFile(avatarComp->spineSkeleton);
    }
    else
    {
        avatarComp->motionFile.clear();
    }
}

// 参照 EntityTypeIndex[Role]：roleType → 属性模板虚拟基址（Elite/Boss 不是真实表行）。
int32_t attributeBaseIndexForRoleType(EntityRoleType roleType)
{
    switch (roleType)
    {
    case EntityRoleType::kHero:
        return 10000;
    case EntityRoleType::kMonster:
    case EntityRoleType::kMachine:
    case EntityRoleType::kCopperOre:
    case EntityRoleType::kSilverOre:
    case EntityRoleType::kGoldOre:
    case EntityRoleType::kAthena:
        return 140000;
    case EntityRoleType::kElite:
        return 150000;  // 虚拟，曲线仍走 140000
    case EntityRoleType::kBoss:
        return 160000;  // 虚拟
    case EntityRoleType::kSummon:
        return 70000;
    default:
        return 140000;
    }
}

float templateColumn(const AttributeTemplateConfig* t, int nameIndex)
{
    // getAttributeData 读列时 hp↔base_damage 互换
    switch (nameIndex)
    {
    case 0:
        return t->sourceForce;
    case 1:
        return t->agility;
    case 2:
        return t->habitus;
    case 3:
        return t->spirit;
    case 4:
        return t->baseDamage;  // AttributeName "hp" ← 列 base_damage
    case 5:
        return t->atk;
    case 6:
        return t->def;
    case 7:
        return t->matk;
    case 8:
        return t->mdef;
    case 9:
        return t->crit;
    case 10:
        return t->critResist;
    case 11:
        return t->critDamage;
    case 12:
        return t->critDamageResist;
    case 13:
        return t->dodge;
    case 14:
        return t->hit;
    case 15:
        return t->hp;  // AttributeName "base_damage" ← 列 hp
    default:
        return 0.0f;
    }
}

// 对齐 DBEntity:getAttributeData：始终从 140000 曲线插值；Elite/Boss 只加 rate。
bool bakeMonsterAttributeCurve(float outAttrs[16], int32_t attrKey)
{
    auto* config                         = Config::getInstance();
    constexpr int32_t kInitDataId        = 140000;
    const AttributeTemplateConfig* initT = config->getAttributeTemplateConfigById(kInitDataId);
    if (!initT)
        return false;

    const int32_t monsterLv = attrKey % 10000;
    const int32_t lv        = (monsterLv + 9) / 10;
    const int32_t tenDigit  = monsterLv / 100;
    const int32_t unitDigit = monsterLv % 100;
    const int32_t curId     = kInitDataId + tenDigit;
    const int32_t nextId    = kInitDataId + tenDigit + 1;

    const AttributeTemplateConfig* curT = config->getAttributeTemplateConfigById(curId);
    if (!curT)
        return false;
    const AttributeTemplateConfig* nextT = unitDigit != 0 ? config->getAttributeTemplateConfigById(nextId) : nullptr;

    const SkillHurtConfig* hurtCfg = config->getSkillHurtConfigById((std::max)(1, lv));

    for (int i = 0; i < 16; ++i)
    {
        float value = templateColumn(initT, i);
        if (curId == kInitDataId)
        {
            if (nextT)
                value = value + templateColumn(nextT, i) / 100.0f * static_cast<float>(unitDigit);
        }
        else
        {
            value = value + templateColumn(curT, i);
            if (nextT)
                value = value +
                        (templateColumn(nextT, i) - templateColumn(curT, i)) / 100.0f * static_cast<float>(unitDigit);
        }

        const AttributeTemplateConfig* rateSrc = nextT ? nextT : curT;
        if (i == 4)  // hp
        {
            if (hurtCfg)
                value = value + static_cast<float>(hurtCfg->hurt);
            if (rateSrc->monsterHitNumber > 0.0f)
                value = value * rateSrc->monsterHitNumber;
            if (attrKey > 160000)
                value = value * rateSrc->bossHpRate;
            else if (attrKey > 150000)
                value = value * rateSrc->eliteHpRate;
            value = std::floor(value);
        }
        else if (i == 15)  // base_damage
        {
            if (rateSrc->playerHitNumber > 0.0f)
                value = value / rateSrc->playerHitNumber;
            if (attrKey > 160000)
                value = value * rateSrc->bossHurtRate;
            else if (attrKey > 150000)
                value = value * rateSrc->eliteHurtRate;
        }
        outAttrs[i] = value;
    }
    return true;
}

// 按「属性模板 × 等级 × attributeRate」烘焙 basic（参照 EntityAttribute:loadAttribute）。
// rate 顺序对齐 AttributeName：source_force, agility, habitus, spirit, hp, atk, def, matk, mdef,
// crit, crit_resist, crit_damage, crit_damage_resist, dodge, hit, base_damage（16 个，0-indexed）。
void applyAttributeTemplate(AttributeComponent* attrComp, const RoleConfig* role, int32_t level)
{
    if (!attrComp || !role)
        return;

    const int32_t baseIdx = attributeBaseIndexForRoleType(role->roleType);
    const int32_t attrKey = baseIdx + (std::max)(1, level);  // 对齐：base + level（不是 level-1）

    float baked[16] = {};
    bool ok         = false;
    if (baseIdx == 10000)
    {
        // Hero：表内有 10000 段；优先直接取行，缺行再走怪物曲线
        const int32_t heroId = baseIdx + (std::max)(1, level) - 1;
        if (const auto* tmpl = Config::getInstance()->getAttributeTemplateConfigById(heroId))
        {
            baked[0]  = tmpl->sourceForce;
            baked[1]  = tmpl->agility;
            baked[2]  = tmpl->habitus;
            baked[3]  = tmpl->spirit;
            baked[4]  = tmpl->hp;
            baked[5]  = tmpl->atk;
            baked[6]  = tmpl->def;
            baked[7]  = tmpl->matk;
            baked[8]  = tmpl->mdef;
            baked[9]  = tmpl->crit;
            baked[10] = tmpl->critResist;
            baked[11] = tmpl->critDamage;
            baked[12] = tmpl->critDamageResist;
            baked[13] = tmpl->dodge;
            baked[14] = tmpl->hit;
            baked[15] = tmpl->baseDamage;
            ok        = true;
        }
    }
    if (!ok)
        ok = bakeMonsterAttributeCurve(baked, attrKey);
    if (!ok)
        return;

    auto& a           = attrComp->basic;
    const auto& r     = role->attributeRate;
    const auto rateAt = [&](size_t i) { return i < r.size() ? r[i] : 0.0f; };

    a.sourceForce      = baked[0] * rateAt(0);
    a.agility          = baked[1] * rateAt(1);
    a.habitus          = baked[2] * rateAt(2);
    a.spirit           = baked[3] * rateAt(3);
    a.hpMax            = baked[4] * rateAt(4);
    a.atk              = baked[5] * rateAt(5);
    a.def              = baked[6] * rateAt(6);
    a.matk             = baked[7] * rateAt(7);
    a.mdef             = baked[8] * rateAt(8);
    a.crit             = baked[9] * rateAt(9);
    a.critResist       = baked[10] * rateAt(10);
    a.critDamage       = baked[11] * rateAt(11);
    a.critDamageResist = baked[12] * rateAt(12);
    a.dodge            = baked[13] * rateAt(13);
    a.hit              = baked[14] * rateAt(14);
    a.baseDamage       = baked[15] * rateAt(15);

    if (attrComp->hp <= 0.0f)
        attrComp->hp = a.hpMax;
    if (attrComp->mp <= 0.0f)
        attrComp->mp = attrComp->mpMax;
}

}  // namespace

Entity* spawnRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params)
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
    auto hitReactComp     = MG_ADD_COMPONENT(actor, HitReactComponent);
    auto attributeComp    = MG_ADD_COMPONENT(actor, AttributeComponent);
    auto actorDataComp    = MG_ADD_COMPONENT(actor, ActorDataComponent);
    auto soundComp        = MG_ADD_COMPONENT(actor, SoundComponent);
    auto behaviorComp     = MG_ADD_COMPONENT(actor, BehaviorComponent);
    auto skillCastComp    = MG_ADD_COMPONENT(actor, SkillCastComponent);
    auto btComp           = MG_ADD_COMPONENT(actor, BehaviorTreeComponent);
    auto skillDeckComp    = MG_ADD_COMPONENT(actor, SkillDeckComponent);
    auto displacementComp = MG_ADD_COMPONENT(actor, DisplacementComponent);
    auto buffComp         = MG_ADD_COMPONENT(actor, BuffComponent);
    auto aiComp           = MG_ADD_COMPONENT(actor, AIComponent);

    auto* skillMgr = skillCastComp->ensureManager();

    btComp->ensureTree();
    btComp->cityMode = preferCity || params.cityMode;
    btComp->treeKind = btComp->cityMode ? 1 : 0;

    // 出生点 + 巡逻半径；怪物按住即跑，避免双击语义
    auto* agent            = aiComp->ensureAgent();
    agent->spawnPosition.x = static_cast<float>(x);
    agent->spawnPosition.y = static_cast<float>(y);
    agent->spawnPosition.z = 0.0f;
    if (params.category == EntityCategory::kMonster)
    {
        behaviorComp->clickToWalk = false;
        int32_t scope             = 200;
        if (!role->aiIds.empty())
        {
            if (const auto* ai = config->getAiConfigById(role->aiIds.front()))
            {
                const int32_t px = (std::max)(std::abs(ai->patrolScopeX.x), std::abs(ai->patrolScopeX.y));
                if (px > 0)
                    scope = px;
                else if (ai->chaseScopeX.y > 0)
                    scope = (std::max)(150, ai->chaseScopeX.y / 4);
            }
        }
        agent->patrolScope = scope;
    }

    fillAvatarFromRole(avatarComp, role, spine, preferCity);

    behaviorComp->roleConfig       = role;
    behaviorComp->behaviorTemplate = avatarComp->behaviorTemplate;
    behaviorComp->rigidityMax      = role->rigidity;
    behaviorComp->rigidityRemain   = role->rigidity;
    behaviorComp->weight           = role->weight;
    behaviorComp->fatigue          = role->fatigue;
    behaviorComp->statusTags =
        StateTag::kTagGrounded | StateTag::kTagMovable | StateTag::kTagAttackAllowed | StateTag::kTagFacingAllowed;

    applyAttributeTemplate(attributeComp, role, params.level);
    attributeComp->epMax = 100.0f;
    attributeComp->ep    = attributeComp->epMax;
    if (attributeComp->hp <= 0.0f)
        attributeComp->hp = attributeComp->basic.hpMax;
    if (attributeComp->mp <= 0.0f)
        attributeComp->mp = attributeComp->mpMax;
    if (attributeComp->moveSpeed <= 0.0f && role->velocity > 0.0f)
        attributeComp->moveSpeed = role->velocity * 1000.0f;

    physicsComp->isStaticBody = false;
    {
        const float box     = static_cast<float>((std::max)(1, role->radius) * 2);
        physicsComp->size.x = box;
        physicsComp->size.y = box;
    }
    {
        const float moveSpeed      = attributeComp->moveSpeed;
        physicsComp->maxVelocity.x = std::max(physicsComp->maxVelocity.x, moveSpeed);
        physicsComp->maxVelocity.y = std::max(physicsComp->maxVelocity.y, moveSpeed);
    }

    identityComp->monsterCamps   = role->monsterCamps;
    identityComp->category       = params.category;
    identityComp->characterClass = type_conversions::toCharacterClass(role->id);
    identityComp->playerId       = params.playerId;
    identityComp->name           = std::string(params.name);

    transformComp->position.x = x;
    transformComp->position.y = y;
    transformComp->scale.x    = 1.0f;
    transformComp->scale.y    = 1.0f;

    // 英雄用默认链 + 闪避/爆气/突刺；怪物从 AI 配置绑技能。
    // 本工程可玩角色是 101/102/103，表里 roleType 常为 Elite，按玩家实体走英雄链。
    const bool isHero = (params.category == EntityCategory::kPlayer) || (role->roleType == EntityRoleType::kHero);
    std::vector<int32_t> roots;
    if (params.category == EntityCategory::kMonster && !isHero && !role->aiIds.empty())
    {
        if (const auto* ai = config->getAiConfigById(role->aiIds.front()))
        {
            for (int32_t sid : ai->skillIds)
            {
                if (sid <= 0)
                    continue;
                if (std::find(roots.begin(), roots.end(), sid) == roots.end())
                    roots.push_back(sid);
            }
        }
    }
    if (isHero)
    {
        if (roots.empty())
        {
            roots.push_back(920000);  // 普攻 -> SLOT_0 (A/1)
            roots.push_back(920100);  // 技能A -> SLOT_1 (2)
            roots.push_back(920120);  // 技能B -> SLOT_2 (3)
            roots.push_back(920150);  // 技能C -> SLOT_3 (4)
        }
        // 补齐闪避/爆气/突刺根（已在 roots 中则跳过）
        const int32_t extras[] = {920090, 920080, 920280};
        for (int32_t extra : extras)
        {
            if (!config->getSkillAttackConfigById(extra))
                continue;
            if (std::find(roots.begin(), roots.end(), extra) == roots.end())
                roots.push_back(extra);
        }
    }

    constexpr int32_t kMaxSkillSlots = static_cast<int32_t>(INPUT_SLOT_10) - static_cast<int32_t>(INPUT_SLOT_0) + 1;

    skillDeckComp->slotSkillIndices.clear();
    skillBarComp->skillSlots.clear();
    skillDeckComp->skills.clear();
    actorDataComp->skills.clear();

    // 每个根技能占一槽；nextSkill 链写入同槽（同槽连段）
    int32_t boundCount = 0;
    for (int32_t rootId : roots)
    {
        if (boundCount >= kMaxSkillSlots)
        {
            MG_LOG_W("spawnRole: role={} skill slot overflow, drop root {}", roleId, rootId);
            break;
        }
        if (!config->getSkillAttackConfigById(rootId))
        {
            MG_LOG_E("spawnRole: skillAttack {} missing", rootId);
            continue;
        }

        std::vector<int32_t> chain;
        collectSkillChain(config, rootId, chain);
        if (chain.empty())
            continue;

        std::vector<int32_t> deckIndices;
        std::vector<int32_t> actorSkillIndices;
        deckIndices.reserve(chain.size());
        actorSkillIndices.reserve(chain.size());

        for (int32_t skillId : chain)
        {
            const auto* skillAtk = config->getSkillAttackConfigById(skillId);
            if (!skillAtk)
            {
                MG_LOG_E("spawnRole: skillAttack {} missing in chain of {}", skillId, rootId);
                continue;
            }

            SkillDeckEntry entry;
            entry.skillAttackId     = skillId;
            entry.nextSkillAttackId = skillAtk->nextSkill > 0 ? skillAtk->nextSkill : -1;
            entry.level             = 1;
            skillDeckComp->skills.push_back(entry);
            deckIndices.push_back(static_cast<int32_t>(skillDeckComp->skills.size() - 1));

            SkillInstanceData data;
            data.level = 1;
            data.buildFromSkillAttack(skillId);
            actorDataComp->skills.push_back(data);
            actorSkillIndices.push_back(static_cast<int32_t>(actorDataComp->skills.size() - 1));
        }

        if (deckIndices.empty())
            continue;

        // 突刺(Y) / 闪避(F) / 爆气(E)：专用键触发，不占数字键槽
        if (rootId == 920090)
        {
            skillMgr->thrustSkillAttackId = chain.front();
            continue;
        }
        if (rootId == 920080)
        {
            skillMgr->dodgeSkillAttackId = chain.front();
            continue;
        }
        if (rootId == 920280)
        {
            skillMgr->crazySkillAttackId = chain.front();
            continue;
        }

        SkillSlotItem slot;
        slot.slotIndex   = static_cast<int32_t>(INPUT_SLOT_0) + boundCount;
        slot.skillIndexs = actorSkillIndices;
        skillBarComp->skillSlots.push_back(slot);
        skillDeckComp->slotSkillIndices.push_back(deckIndices);
        ++boundCount;

        MG_LOG_W("spawnRole: bind INPUT_SLOT_{} -> root {} chainLen={}", boundCount - 1, rootId, deckIndices.size());
    }

    skillMgr->bindConfig(actor);

    MG_LOG_W("spawnRole: role={} skills={} slots={} thrust={} dodge={} crazy={}", roleId, actorDataComp->skills.size(),
             skillBarComp->skillSlots.size(), skillMgr->thrustSkillAttackId, skillMgr->dodgeSkillAttackId,
             skillMgr->crazySkillAttackId);
    return actor;
}

Entity* spawnRemoteRoleActor(ECSManager* ecs, int32_t roleId, int32_t x, int32_t y, const ActorSpawnParams& params)
{
    auto* config = Config::getInstance();

    auto* role = config->getRoleConfigById(roleId);
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
    MG_ADD_COMPONENT(remote, SoundComponent);

    fillAvatarFromRole(avatarComp, role, spine, false);
    transformComp->position.x    = x;
    transformComp->position.y    = y;
    transformComp->scale.x       = 1.0f;
    transformComp->scale.y       = 1.0f;
    identityComp->monsterCamps   = role->monsterCamps;
    identityComp->category       = params.category;
    identityComp->characterClass = type_conversions::toCharacterClass(role->id);
    identityComp->playerId       = params.playerId;
    identityComp->name           = std::string(params.name);
    return remote;
}

}  // namespace actor_spawner

NS_MG_END
