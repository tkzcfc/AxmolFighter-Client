#include "WorldPresenter.h"

#ifdef RUNTIME_IN_AXMOL

#    include "core/audio/AudioManager.h"
#    include "mugen/avatar/AvatarPaths.h"
#    include "mugen/conf/LayerLoader.h"
#    include "mugen/conf/ReflectedConfig.h"
#    include "mugen/conf/TypeConversions.h"
#    include "mugen/conf/tables/PetTables.h"
#    include "mugen/sim_legacy/effect/Effect.h"
#    include "mugen/sim_legacy/room/RoomEntities.h"
#    include "mugen/sim_legacy/role/RoleComponents.h"
#    include "ui/progress/SessionFashionAppearance.h"

#    include <algorithm>
#    include <cmath>

NS_MG_BEGIN

namespace
{
// 受击框调试色；引擎没有预置青色常量。
const ax::Color4F kHurtBoxColor{0.0f, 1.0f, 1.0f, 1.0f};
float renderValue(Fixed value)
{
    return value.toFloatForRender();
}

int32_t chooseSoundId(const sim_legacy::Sound& sound, std::minstd_rand& random)
{
    if (sound.id > 0)
        return sound.id;

    std::vector<int32_t> candidates;
    candidates.reserve(sound.m_candidates.size());
    for (int32_t id : sound.m_candidates)
    {
        if (id > 0)
            candidates.push_back(id);
    }
    if (candidates.empty())
        return -1;
    std::uniform_int_distribution<size_t> distribution(0, candidates.size() - 1);
    return candidates[distribution(random)];
}

ax::Node& createWorldLayer(ax::Node& root)
{
    auto* layer = ax::Node::create();
    layer->setName("worldLayer");
    root.addChild(layer);
    return *layer;
}

// 实体外观配置：Spine 资源、绘制层级、Spine 相对偏移和挂件职业。
struct AvatarLook
{
    int32_t roleId     = 0;
    int32_t resSpineId = 0;
    int32_t tier       = 0;
    std::vector<int32_t> spineOffset;
    CharacterClass characterClass = CharacterClass::kUnknown;
};

// 从实体配置行读取共有的外观字段。
template <class Row>
AvatarLook lookFromRow(const Row& row)
{
    AvatarLook look;
    look.resSpineId  = row.resSpineId;
    look.tier        = row.tier;
    look.spineOffset = row.spineRelativePosition;
    return look;
}

// 按实体组找到对应配置行；各组的配置 id 均已在生成时校验。
AvatarLook resolveLook(sim_legacy::World& world, Entity entity, sim_legacy::GroupKind kind)
{
    auto& ecs          = world.ecs();
    const auto& config = conf::Config::instance();
    switch (kind)
    {
    case sim_legacy::GroupKind::Role:
    {
        const int32_t roleId = ecs.get<sim_legacy::RoleIdentity>(entity).roleId;
        AvatarLook look      = lookFromRow(config.get<conf::RoleConfig>(roleId));
        look.roleId          = roleId;
        look.characterClass  = type_conversions::toCharacterClass(roleId);
        return look;
    }
    case sim_legacy::GroupKind::Effect:
        return lookFromRow(config.get<conf::EffectConfig>(ecs.get<sim_legacy::EffectState>(entity).m_configId));
    case sim_legacy::GroupKind::Obstacle:
        return lookFromRow(config.get<conf::ObstacleConfig>(ecs.get<sim_legacy::ObstacleState>(entity).m_id));
    case sim_legacy::GroupKind::Goods:
        return lookFromRow(config.get<conf::GoodsConfig>(ecs.get<sim_legacy::GoodsState>(entity).m_id));
    case sim_legacy::GroupKind::Portal:
        return lookFromRow(config.get<conf::PortalConfig>(ecs.get<sim_legacy::PortalState>(entity).m_id));
    case sim_legacy::GroupKind::Count:
        break;
    }
    MG_LOG_E("AvatarPresenter: invalid entity group {}", static_cast<int>(kind));
    MG_ASSERT(false && "Invalid entity group for avatar");
    return {};
}

// 按外观配置创建 Avatar，动作文件与逻辑侧绑定的同一份。
// 职业角色按职业默认外观创建：时装图集与皮肤、默认武器挂件（待机时显示，其余动作嫁接到身体插槽）。
// 本地玩家用会话时装/武器外观（可后续 setAppearance 换装）。
Avatar* createAvatar(const AvatarLook& look,
                     const SpineInfo& spine,
                     const std::string& motionFile,
                     bool useSessionFashion,
                     bool transformed)
{
    if (look.characterClass != CharacterClass::kUnknown)
    {
        FashionAppearance appearance;
        if (useSessionFashion)
        {
            appearance = gameui::buildSessionFashionAppearance();
            if (appearance.roleId == 0)
                appearance.roleId = look.roleId;
        }
        else
            appearance.roleId = look.roleId;
        appearance.transformed = transformed;
        return Avatar::create(appearance);
    }
    AvatarDesc desc;
    desc.skeleton       = spine.skeleton;
    desc.atlases        = {spine.atlas};
    desc.scale          = spine.scale;
    desc.motionFile     = motionFile;
    desc.characterClass = look.characterClass;
    return Avatar::create(desc, AvatarLoadMode::kSyncShared);
}

// 读取偏移分量，配置允许省略尾部分量。
float offsetAxis(const std::vector<int32_t>& offset, size_t index)
{
    return index < offset.size() ? static_cast<float>(offset[index]) : 0.0f;
}
}  // namespace

void AvatarPresenter::present(sim_legacy::World& world, float alpha, float renderDelta)
{
    std::set<uint32_t> live;
    world.ecs().each<sim_legacy::EntityGroup, sim_legacy::Transform, sim_legacy::MotionState>(
        [&](Entity entity, const sim_legacy::EntityGroup& group, const sim_legacy::Transform& transform,
            const sim_legacy::MotionState& motion) {
        live.insert(entity.value);
        auto& state = m_avatars[entity.value];
        // 回滚恢复快照后同一句柄可能指向另一个实体：身份变化则整体重建显示。
        const LookIdentity identity{group.kind, lookConfigId(world, entity, group.kind), !motion.motionFile.empty()};
        if (state.resolved && state.identity != identity)
            resetState(entity.value, state);
        if (!state.resolved)
        {
            state.resolved        = true;
            state.identity        = identity;
            const AvatarLook look = resolveLook(world, entity, group.kind);
            const SpineInfo spine = AvatarPaths::resolveSpine(look.resSpineId);
            if (!spine.valid)
                return;
            const bool roleEntity = group.kind == sim_legacy::GroupKind::Role;
            state.fashionAvatar   = roleEntity && look.characterClass != CharacterClass::kUnknown;
            // 只有被镜头跟随的本地玩家使用会话时装；对端玩家按各自职业默认外观创建。
            state.useSessionFashion = roleEntity && entity == m_followEntity;
            state.roleId            = look.roleId;
            state.partnerRageTransformed = motion.m_partnerRageTransformed;
            state.partnerRageStateKnown  = state.fashionAvatar;
            // 无显式换装且非职业角色的节点按外观键复用；时装角色实例私有，不入池。
            const bool poolable = motion.motionFile.empty() ||
                                  (!state.m_overrideAppearance.has_value() &&
                                   look.characterClass == CharacterClass::kUnknown);
            if (poolable)
                state.poolKey = motion.motionFile.empty() ? fmt::format("s:{}", look.resSpineId)
                                                          : fmt::format("a:{}:{}", look.resSpineId, motion.motionFile);
            if (!poolable || !acquirePooled(state))
            {
                if (motion.motionFile.empty())
                    state.skeleton = MgSkeletonAnimation::createFromCache(look.resSpineId);
                else if (state.m_overrideAppearance.has_value())
                {
                    FashionAppearance appearance = *state.m_overrideAppearance;
                    if (appearance.roleId == 0)
                        appearance.roleId = look.roleId;
                    appearance.transformed = motion.m_partnerRageTransformed;
                    state.avatar          = Avatar::create(appearance);
                }
                else
                    state.avatar = createAvatar(look, spine, motion.motionFile, state.useSessionFashion,
                                                motion.m_partnerRageTransformed);
            }
            ax::Node* node = displayNode(state);
            if (!node)
                return;
            state.spineOffset = look.spineOffset;
            tierLayer(look.tier).addChild(node);
        }
        ax::Node* node = displayNode(state);
        if (!node)
            return;
        if (state.fashionAvatar &&
            (!state.partnerRageStateKnown || state.partnerRageTransformed != motion.m_partnerRageTransformed))
            syncPartnerRageAppearance(entity.value, motion, motion.m_partnerRageTransformed, false);
        if (state.motionName != motion.motionName || state.entryId != motion.entryId)
        {
            if (state.avatar)
                state.avatar->setMotion(motion.motionName, motion.entryId, motion.loop);
            else if (!state.skeleton->setAnimation(0, motion.motionName, motion.loop))
                MG_LOG_W("AvatarPresenter: spine animation '{}' not found for entity {}", motion.motionName,
                         entity.value);
            state.motionName = motion.motionName;
            state.entryId    = motion.entryId;
        }
        if (state.avatar)
        {
            state.avatar->seek(static_cast<int>(motion.timeMs.toInt()));
            state.avatar->stepAccessories(renderDelta);
        }

        // 仅逻辑帧推进时滚动采样：previous 取上一逻辑帧，current 取最新逻辑帧，
        // 渲染帧按 alpha 在两者间插值，得到平滑运动而非 30Hz 阶梯；回滚纠正偏移随渲染时间衰减。
        // 本地玩家用短时间常数跟手，其余实体（对端英雄等）的回滚纠正用长时间常数与大吸附阈值滑行消化。
        state.track.setProfile(entity == m_followEntity ? CorrectionProfile::Local : CorrectionProfile::Remote);
        state.track.sample(world.ecs().singleton<sim_legacy::WorldClock>().frame,
                           {renderValue(transform.position.x), renderValue(transform.position.y),
                            renderValue(transform.position.z)});
        state.track.advance(renderDelta);
        state.towardX = transform.towardX < 0 ? -1.0f : 1.0f;
        state.towardZ = transform.vectorZ < Fixed{} ? -1.0f : 1.0f;
        const RenderPose pose = interpolatePose(state, alpha);
        // Spine 偏移：x 随水平朝向翻转，z 随纵深朝向翻转；层内按偏移后的纵深排序。
        const float depth = pose.z + pose.towardZ * offsetAxis(state.spineOffset, 2);
        node->setPosition(pose.x + pose.towardX * offsetAxis(state.spineOffset, 0),
                          pose.y + offsetAxis(state.spineOffset, 1) + depth);
        node->setLocalZOrder(-static_cast<int>(std::floor(depth)));
        node->setScaleX(pose.towardX);
    });
    for (auto it = m_avatars.begin(); it != m_avatars.end();)
    {
        if (!live.contains(it->first))
        {
            releaseNode(it->second);
            // 实体消失时同步清理其宠物。
            auto pet = m_petFollowers.find(it->first);
            if (pet != m_petFollowers.end())
            {
                pet->second->clear();
                m_petFollowers.erase(pet);
            }
            it = m_avatars.erase(it);
        }
        else
            ++it;
    }

    for (auto it = m_petFollowers.begin(); it != m_petFollowers.end();)
    {
        RenderPose hero;
        if (samplePose(it->first, alpha, &hero))
        {
            it->second->present({hero.x, hero.y, hero.z, hero.towardX, hero.towardZ}, renderDelta);
            ++it;
        }
        else
        {
            it->second->clear();
            it = m_petFollowers.erase(it);
        }
    }
}

AvatarPresenter::RenderPose AvatarPresenter::interpolatePose(const AvatarState& state, float alpha)
{
    const VisualPos shown = state.track.display(alpha);
    RenderPose pose;
    pose.x       = shown.x;
    pose.y       = shown.y;
    pose.z       = shown.z;
    pose.towardX = state.towardX;
    pose.towardZ = state.towardZ;
    return pose;
}

bool AvatarPresenter::samplePose(uint32_t entityValue, float alpha, RenderPose* out) const
{
    const auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end() || !it->second.track.hasSample() || !out)
        return false;
    *out = interpolatePose(it->second, alpha);
    return true;
}

void AvatarPresenter::applyCorrection(Entity entity, const VisualPos& shift)
{
    const auto it = m_avatars.find(entity.value);
    if (it != m_avatars.end())
        it->second.track.applyCorrection(shift);
}

FashionAppearance AvatarPresenter::buildRoleAppearance(const AvatarState& state, bool transformed)
{
    FashionAppearance appearance;
    if (state.m_overrideAppearance.has_value())
        appearance = *state.m_overrideAppearance;
    else if (state.useSessionFashion)
        appearance = gameui::buildSessionFashionAppearance();
    if (appearance.roleId == 0)
        appearance.roleId = state.roleId;
    appearance.transformed = transformed;
    return appearance;
}

bool AvatarPresenter::syncPartnerRageAppearance(uint32_t entityValue,
                                                const sim_legacy::MotionState& motion,
                                                bool transformed,
                                                bool force)
{
    auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end())
        return false;
    auto& state = it->second;
    if (!state.avatar || !state.fashionAvatar)
        return false;
    if (!force && state.partnerRageStateKnown && state.partnerRageTransformed == transformed)
        return true;
    const FashionAppearance appearance = buildRoleAppearance(state, transformed);
    if (!state.avatar->setAppearance(appearance))
        return false;
    state.partnerRageTransformed = transformed;
    state.partnerRageStateKnown  = true;
    // 换骨架后立即重放当前逻辑动作和时间，避免显示回到默认待机片段。
    state.avatar->setMotion(motion.motionName, motion.entryId, motion.loop);
    state.avatar->seek(static_cast<int>(motion.timeMs.toInt()));
    state.motionName = motion.motionName;
    state.entryId    = motion.entryId;
    return true;
}

void AvatarPresenter::onPartnerRageChanged(sim_legacy::World& world, Entity entity, bool active)
{
    const auto* motion = world.ecs().tryGet<sim_legacy::MotionState>(entity);
    if (!motion)
        return;
    syncPartnerRageAppearance(entity.value, *motion, active, true);
}

void AvatarPresenter::setPet(int32_t petId, int32_t spineId)
{
    setEntityPet(m_followEntity, petId, spineId);
}

void AvatarPresenter::clearPet()
{
    clearEntityPet(m_followEntity);
}

void AvatarPresenter::setEntityPet(Entity entity, int32_t petId, int32_t spineId)
{
    if (entity.value == 0 || petId == 0)
    {
        clearEntityPet(entity);
        return;
    }
    auto& slot = m_petFollowers[entity.value];
    if (!slot)
        slot = std::make_unique<PetFollower>(tierLayer(2));
    slot->setPet(petId, spineId);
}

void AvatarPresenter::clearEntityPet(Entity entity)
{
    const auto it = m_petFollowers.find(entity.value);
    if (it == m_petFollowers.end())
        return;
    it->second->clear();
    m_petFollowers.erase(it);
}

ax::Node* AvatarPresenter::displayNode(const AvatarState& state)
{
    if (state.avatar)
        return state.avatar;
    return state.skeleton;
}

int32_t AvatarPresenter::lookConfigId(sim_legacy::World& world, Entity entity, sim_legacy::GroupKind kind)
{
    auto& ecs = world.ecs();
    switch (kind)
    {
    case sim_legacy::GroupKind::Role:
        return ecs.get<sim_legacy::RoleIdentity>(entity).roleId;
    case sim_legacy::GroupKind::Effect:
        return ecs.get<sim_legacy::EffectState>(entity).m_configId;
    case sim_legacy::GroupKind::Obstacle:
        return ecs.get<sim_legacy::ObstacleState>(entity).m_id;
    case sim_legacy::GroupKind::Goods:
        return ecs.get<sim_legacy::GoodsState>(entity).m_id;
    case sim_legacy::GroupKind::Portal:
        return ecs.get<sim_legacy::PortalState>(entity).m_id;
    case sim_legacy::GroupKind::Count:
        break;
    }
    MG_LOG_E("AvatarPresenter: invalid entity group {}", static_cast<int>(kind));
    MG_ASSERT(false && "Invalid entity group for avatar");
    return 0;
}

bool AvatarPresenter::acquirePooled(AvatarState& state)
{
    const auto it = m_pool.find(state.poolKey);
    if (it == m_pool.end() || it->second.empty())
        return false;
    const PooledNode pooled = it->second.back();
    it->second.pop_back();
    --m_poolCount;
    state.avatar   = pooled.avatar;
    state.skeleton = pooled.skeleton;
    // 池中节点仅剩入池时的那一份引用：改为 autorelease，保证节点存活到随后的 addChild 接管引用，
    // 直接 release 会让引用计数归零并立即析构。
    if (state.avatar)
        state.avatar->autorelease();
    else
        state.skeleton->autorelease();
    return true;
}

void AvatarPresenter::releaseNode(AvatarState& state)
{
    ax::Node* node = displayNode(state);
    if (!node)
        return;
    if (state.poolKey.empty() || m_poolCount >= kPoolMaxTotal)
    {
        node->removeFromParent();
        return;
    }
    auto& bucket = m_pool[state.poolKey];
    if (bucket.size() >= kPoolMaxPerKey)
    {
        node->removeFromParent();
        return;
    }
    // 复位表现层改过的节点属性；动作与时间在复用时由 setMotion/seek 重新设置。
    node->retain();
    node->removeFromParent();
    node->setColor(ax::Color3B::WHITE);
    node->setOpacity(255);
    node->setScale(1.0f);
    node->setLocalZOrder(0);
    if (state.skeleton)
    {
        state.skeleton->clearTracks();
        state.skeleton->setSlotsToSetupPose();
    }
    bucket.push_back({state.avatar, state.skeleton});
    ++m_poolCount;
}

void AvatarPresenter::resetState(uint32_t entityValue, AvatarState& state)
{
    releaseNode(state);
    const auto pet = m_petFollowers.find(entityValue);
    if (pet != m_petFollowers.end())
    {
        pet->second->clear();
        m_petFollowers.erase(pet);
    }
    state = AvatarState{};
}

void AvatarPresenter::clearPool()
{
    for (auto& [_, bucket] : m_pool)
        for (const PooledNode& pooled : bucket)
        {
            if (pooled.avatar)
                pooled.avatar->release();
            else
                pooled.skeleton->release();
        }
    m_pool.clear();
    m_poolCount = 0;
}

ax::Node& AvatarPresenter::tierLayer(int32_t tier)
{
    auto& layer = m_tierLayers[tier];
    if (!layer)
    {
        layer = ax::Node::create();
        layer->setName(fmt::format("tier_{}", tier));
        m_root.addChild(layer, tier);
    }
    return *layer;
}

void AvatarPresenter::clear()
{
    for (auto& [_, state] : m_avatars)
        if (ax::Node* node = displayNode(state))
            node->removeFromParent();
    m_avatars.clear();
    for (auto& [_, pet] : m_petFollowers)
        pet->clear();
    m_petFollowers.clear();
    clearPool();
}

bool AvatarPresenter::setEntityAppearance(uint32_t entityValue, const FashionAppearance& appearance)
{
    auto& state             = m_avatars[entityValue];
    state.m_overrideAppearance = appearance;
    if (!state.avatar)
        return true;
    if (!state.avatar->setAppearance(appearance))
        return false;
    if (state.fashionAvatar)
    {
        state.partnerRageTransformed = appearance.transformed;
        state.partnerRageStateKnown  = true;
    }
    state.motionName.clear();
    state.entryId.clear();
    return true;
}

bool AvatarPresenter::setEntityIntensity(uint32_t entityValue, float intensity)
{
    auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end())
        return false;
    ax::Node* node = displayNode(it->second);
    if (!node)
        return false;
    // 引擎 Spine 顶点色为 8 位（0-255），无法超过白色实现 >1 的提亮；
    // 这里把亮度倍率饱和到 [0,1]，intensity>1 时按最亮（白色）处理。
    const float v     = std::clamp(intensity, 0.0f, 1.0f);
    const auto  level = static_cast<uint8_t>(std::lround(v * 255.0f));
    node->setColor(ax::Color3B(level, level, level));
    return true;
}

bool WorldPresenter::setEntityAppearance(Entity entity, const FashionAppearance& appearance)
{
    return m_avatars.setEntityAppearance(entity.value, appearance);
}

void WorldPresenter::setFollowEntity(Entity entity)
{
    m_followEntity = entity;
    m_avatars.setFollowEntity(entity);
}

void WorldPresenter::setPet(int32_t petId)
{
    setEntityPet(m_followEntity, petId);
}

void WorldPresenter::setEntityPet(Entity entity, int32_t petId)
{
    if (petId == 0)
    {
        m_avatars.clearEntityPet(entity);
        return;
    }
    const auto* pet = conf::Config::instance().find<conf::PetConfig>(petId);
    if (!pet)
    {
        MG_LOG_W("WorldPresenter: pet {} not found", petId);
        m_avatars.clearEntityPet(entity);
        return;
    }
    if (pet->spineId <= 0)
    {
        MG_LOG_W("WorldPresenter: pet {} has invalid spine {}", petId, pet->spineId);
        m_avatars.clearEntityPet(entity);
        return;
    }
    m_avatars.setEntityPet(entity, petId, pet->spineId);
}

void WorldPresenter::clearEntityPet(Entity entity)
{
    m_avatars.clearEntityPet(entity);
}

bool WorldPresenter::refreshLocalPlayerAppearance(sim_legacy::World& world)
{
    // 本地玩家即镜头跟随实体；联网对端玩家不套用本机会话时装。
    const Entity local = m_followEntity;
    if (local.value == 0 || !world.ecs().alive(local))
        return false;
    FashionAppearance appearance = gameui::buildSessionFashionAppearance();
    if (appearance.roleId == 0)
        appearance.roleId = world.ecs().get<sim_legacy::RoleIdentity>(local).roleId;
    appearance.transformed = world.ecs().get<sim_legacy::MotionState>(local).m_partnerRageTransformed;
    return setEntityAppearance(local, appearance);
}

void MapPresenter::load(const std::string& mapKey)
{
    if (m_mapKey == mapKey)
        return;
    if (m_map)
        m_map->removeFromParent();
    m_mapKey               = mapKey;
    const std::string path = mapKey.ends_with(".layer") ? mapKey : "/mugen/map/" + mapKey + ".layer";
    m_map                  = LayerRuntimeLoader::loadNode(path);
    m_root.addChild(m_map, -100);
    m_camera              = VirtualCamera::create();
    const IntVec2 mapSize = LayerLoader::load(path).size;
    if (mapSize.x > 0 && mapSize.y > 0)
        m_camera->setRegion({static_cast<float>(mapSize.x), static_cast<float>(mapSize.y)});
    else
        MG_LOG_E("MapPresenter: map '{}' has no size, camera cannot scroll", path);
    m_camera->setCall([map = m_map, world = &m_worldLayer](float x, float y, float zoom) {
        map->setPosition(x, y);
        map->setScale(zoom);
        world->setPosition(x, y);
        world->setScale(zoom);
    });
}

void MapPresenter::update(float delta)
{
    if (m_camera)
        m_camera->doUpdate(delta);
}

void CameraDirector::follow(sim_legacy::World& world, Entity entity)
{
    const auto* transform = world.ecs().tryGet<sim_legacy::Transform>(entity);
    if (!transform || !m_map.camera())
        return;
    m_map.camera()->setFocusPosition({renderValue(transform->position.x), renderValue(transform->position.z)});
}

void CameraDirector::consume(const sim_legacy::PresentationEvent& event)
{
    if (const auto* shake = std::get_if<sim_legacy::CameraShake>(&event.payload); shake && m_map.camera())
        m_map.camera()->shake(renderValue(shake->strength), renderValue(shake->durationMs));
    if (std::holds_alternative<sim_legacy::DisplaySpine>(event.payload))
        m_displaySlowScale = 0.35f;
}

void CameraDirector::update(float delta)
{
    m_displaySlowScale = std::min(1.0f, m_displaySlowScale + delta * 2.0f);
    m_map.update(delta * m_displaySlowScale);
}

void SoundPlayer::consume(const sim_legacy::PresentationEvent& event)
{
    const auto* sound = std::get_if<sim_legacy::Sound>(&event.payload);
    if (!sound)
        return;

    if (sound->m_stopLoop)
    {
        if (sound->m_actionId < 0)
            return;
        const ActionKey key{event.entity.value, sound->m_actionId};
        const auto it = m_loopAudioByAction.find(key);
        if (it == m_loopAudioByAction.end())
            return;
        game::AudioManager::getInstance()->setSFXLoop(it->second, false);
        m_loopAudioByAction.erase(it);
        return;
    }

    const int32_t chosenSoundId = chooseSoundId(*sound, m_random);
    if (chosenSoundId <= 0)
        return;

    const int audioId = game::AudioManager::getInstance()->playSound(chosenSoundId);
    if (sound->m_actionId < 0 || audioId == -1)
        return;

    const ActionKey key{event.entity.value, sound->m_actionId};
    if (const auto it = m_loopAudioByAction.find(key); it != m_loopAudioByAction.end())
        game::AudioManager::getInstance()->setSFXLoop(it->second, false);
    m_loopAudioByAction[key] = audioId;
}

void SoundPlayer::clear()
{
    for (const auto& entry : m_loopAudioByAction)
        game::AudioManager::getInstance()->setSFXLoop(entry.second, false);
    m_loopAudioByAction.clear();
}

namespace
{
// 飘字位图字体文件，下标与 HurtNumber::Face 一致。
constexpr const char* kHurtFontFiles[] = {
    "/mugen/font/hurt_normal.fnt",
    "/mugen/font/hurt_hero.fnt",
    "/mugen/font/hurt_crit.fnt",
    "/mugen/font/hurt_word.fnt",
};
// 文字字体中的字符：M 为未命中、D 为闪避。
constexpr const char* kMissGlyph  = "M";
constexpr const char* kDodgeGlyph = "D";
// 无接触点时飘字锚点相对实体纵深位置抬高的像素。
constexpr float kHurtHeadHeight = 220.0f;
// 文字相对数字锚点上抬的像素。
constexpr float kHurtWordRise = 50.0f;
// 动画步长（毫秒）与总步数；一步对应一次缩放/上升/淡出更新。
constexpr float kHurtStepMs = 50.0f;
constexpr int kHurtStepCount = 20;
// 第 13 步起开始上升淡出；淡出每步减少的不透明度与上升像素。
constexpr int kHurtFadeBeginStep = 13;
constexpr int kHurtFadeAlphaStep = 37;
constexpr float kHurtFadeRise    = 15.0f;
// 同时存在的飘字上限，超出时回收最早的。
constexpr size_t kHurtMaxActive = 64;
// 随机偏移的最大像素。
constexpr int kHurtRandomRange = 30;
}  // namespace

void HurtNumber::consume(const sim_legacy::PresentationEvent& event, sim_legacy::World& world)
{
    auto& ecs = world.ecs();
    if (const auto* tips = std::get_if<sim_legacy::Tips>(&event.payload))
    {
        // 闪避/未命中由逻辑侧以固定文本提示，数字以外的飘字走文字字体。
        const char* glyph = tips->text == "MISS" ? kMissGlyph : tips->text == "DODGE" ? kDodgeGlyph : nullptr;
        const auto* transform = ecs.tryGet<sim_legacy::Transform>(event.entity);
        if (!glyph || !transform)
            return;
        spawn(Face::kWord, glyph, renderValue(transform->position.x),
              renderValue(transform->position.y + transform->position.z) + kHurtHeadHeight);
        return;
    }
    const auto* hurt = std::get_if<sim_legacy::HurtNumber>(&event.payload);
    if (!hurt)
        return;
    float x = 0.0f;
    float y = 0.0f;
    if (hurt->m_hasContact)
    {
        x = renderValue(hurt->m_contact.x);
        y = renderValue(hurt->m_contact.y);
    }
    else
    {
        const auto* transform = ecs.tryGet<sim_legacy::Transform>(event.entity);
        if (!transform)
            return;
        x = renderValue(transform->position.x);
        y = renderValue(transform->position.y + transform->position.z) + kHurtHeadHeight;
    }
    // 暴击优先；否则敌方打到玩家方用受击字体，其余用普通字体。
    Face face = Face::kNormal;
    if (hurt->m_critical)
        face = Face::kCrit;
    else if (const auto* identity = ecs.tryGet<sim_legacy::RoleIdentity>(event.entity))
    {
        const bool enemyCamp = identity->camp == static_cast<int32_t>(EntityCamp::Enemy) ||
                               identity->camp == static_cast<int32_t>(EntityCamp::EnemyFriend);
        if (identity->roleType == static_cast<int32_t>(EntityRoleType::kHero) && !enemyCamp)
            face = Face::kHero;
    }
    // 位图字体没有负号，持续回复按数值绝对值显示。
    spawn(face, std::to_string(hurt->value < 0 ? -hurt->value : hurt->value), x, y);
}

float HurtNumber::randomOffset()
{
    std::uniform_int_distribution<int> magnitude(0, kHurtRandomRange);
    const int value = magnitude(m_random);
    return static_cast<float>((m_random() & 1u) ? value : -value);
}

ax::Label* HurtNumber::acquireLabel(Face face)
{
    auto& pool = m_pool[static_cast<size_t>(face)];
    if (!pool.empty())
    {
        ax::Label* label = pool.back();
        pool.pop_back();
        label->setVisible(true);
        return label;
    }
    auto* label = ax::Label::createWithBMFont(kHurtFontFiles[static_cast<size_t>(face)], "");
    m_root.addChild(label, 200);
    return label;
}

void HurtNumber::spawn(Face face, const std::string& text, float x, float y)
{
    if (m_active.size() >= kHurtMaxActive)
    {
        recycle(m_active.front());
        m_active.erase(m_active.begin());
    }
    Item item;
    item.face  = face;
    item.x     = x + randomOffset();
    item.y     = y + randomOffset();
    item.label = acquireLabel(face);
    item.label->setString(text);
    item.label->setScale(1.0f);
    item.label->setOpacity(255);
    item.label->setPosition(item.x, item.y + (face == Face::kWord ? kHurtWordRise : 0.0f));
    m_active.push_back(item);
}

void HurtNumber::applyStep(Item& item)
{
    const bool word = item.face == Face::kWord;
    // 出现阶段缩放曲线（前 5 步），之后恒为 1；数字比文字起点更大。
    static constexpr float kNumberScale[] = {3.0f, 2.0f, 1.4f, 0.8f, 0.6f};
    static constexpr float kWordScale[]   = {2.0f, 1.4f, 1.4f, 0.8f, 0.6f};
    const int step = item.step;
    if (step < 5)
    {
        item.label->setScale(word ? kWordScale[step] : kNumberScale[step]);
        // 数字第 0 步半透明闪入，第 1 步起全不透明。
        if (!word && step == 0)
            item.label->setOpacity(107);
        else if (!word && step == 1)
            item.label->setOpacity(255);
    }
    else if (step < kHurtFadeBeginStep)
        item.label->setScale(1.0f);
    else
    {
        item.alpha = std::max(0, item.alpha - kHurtFadeAlphaStep);
        item.y += kHurtFadeRise;
        item.label->setPosition(item.x, item.y + (word ? kHurtWordRise : 0.0f));
        item.label->setOpacity(static_cast<uint8_t>(item.alpha));
    }
}

void HurtNumber::update(float delta)
{
    for (auto& item : m_active)
    {
        item.carryMs += delta * 1000.0f;
        while (item.carryMs >= kHurtStepMs && item.step < kHurtStepCount)
        {
            applyStep(item);
            item.carryMs -= kHurtStepMs;
            ++item.step;
        }
    }
    for (auto it = m_active.begin(); it != m_active.end();)
    {
        if (it->step >= kHurtStepCount)
        {
            recycle(*it);
            it = m_active.erase(it);
        }
        else
            ++it;
    }
}

void HurtNumber::recycle(Item& item)
{
    item.label->setVisible(false);
    m_pool[static_cast<size_t>(item.face)].push_back(item.label);
    item.label = nullptr;
}

void HurtNumber::clear()
{
    for (auto& item : m_active)
        item.label->removeFromParent();
    m_active.clear();
    for (auto& pool : m_pool)
    {
        for (ax::Label* label : pool)
            label->removeFromParent();
        pool.clear();
    }
}

DebugDraw::DebugDraw(ax::Node& root)
{
    m_node = ax::DrawNode::create();
    root.addChild(m_node, 300);
}

void DebugDraw::present(sim_legacy::World& world)
{
    m_node->clear();
    if (!m_enabled)
        return;
    // WorldBox 已是世界坐标，pos 为 x/z 平面最小角，与碰撞判定的轴对齐框一致
    const auto drawBoxes = [this](const std::vector<sim_legacy::WorldBox>& boxes, const ax::Color4F& color) {
        for (const auto& box : boxes)
            m_node->drawRect({renderValue(box.pos.x), renderValue(box.pos.z)},
                             {renderValue(box.pos.x + box.size.x), renderValue(box.pos.z + box.size.z)}, color);
    };
    world.ecs().each<sim_legacy::Transform, sim_legacy::ColliderState>(
        [&](Entity, const sim_legacy::Transform& transform, const sim_legacy::ColliderState& collider) {
        const ax::Vec2 center{renderValue(transform.position.x), renderValue(transform.position.z)};
        m_node->drawCircle(center, renderValue(collider.radius), 0.0f, 16, false, ax::Color4F::YELLOW);
        drawBoxes(collider.attack, ax::Color4F::RED);
        drawBoxes(collider.damage, kHurtBoxColor);
    });
    for (const auto& range : world.ecs().singleton<sim_legacy::MapRegion>().moveRanges)
        m_node->drawRect({renderValue(range.x), renderValue(range.y)},
                         {renderValue(range.x + range.width), renderValue(range.y + range.height)}, ax::Color4F::GREEN);
}

WorldPresenter::WorldPresenter(ax::Node& root)
    : m_worldLayer(createWorldLayer(root))
    , m_avatars(m_worldLayer)
    , m_map(root, m_worldLayer)
    , m_camera(m_map)
    , m_hurtNumbers(m_worldLayer)
    , m_debugDraw(m_worldLayer)
{}

WorldPresenter::~WorldPresenter()
{
    m_sound.clear();
    m_hurtNumbers.clear();
    // 先拆掉角色/特效节点，让 MgSkeletonData 的节点引用在 ResourceLoader 释放之前掉干净。
    m_avatars.clear();
}

void WorldPresenter::loadMap(const std::string& mapKey)
{
    if (mapKey.empty())
        return;
    m_map.load(mapKey);
    // 换图后旧外观的复用节点基本不会再命中，及时释放。
    m_avatars.clearPool();
}

void WorldPresenter::present(sim_legacy::World& world, float interpolationAlpha, float renderDelta)
{
    m_avatars.present(world, std::clamp(interpolationAlpha, 0.0f, 1.0f), renderDelta);
    consumeEvents(world.ecs().singleton<sim_legacy::PresentationEvents>().events, world, true);
    m_hurtNumbers.update(renderDelta);
    m_camera.follow(world, m_followEntity);
    m_camera.update(renderDelta);
    m_debugDraw.present(world);
}

void WorldPresenter::presentEvents(const std::vector<sim_legacy::PresentationEvent>& events,
                                   sim_legacy::World& world,
                                   float interpolationAlpha,
                                   float renderDelta)
{
    m_avatars.present(world, std::clamp(interpolationAlpha, 0.0f, 1.0f), renderDelta);
    consumeEvents(events, world, false);
    m_hurtNumbers.update(renderDelta);
    m_camera.follow(world, m_followEntity);
    m_camera.update(renderDelta);
    m_debugDraw.present(world);
}

void WorldPresenter::consumeEvents(const std::vector<sim_legacy::PresentationEvent>& events,
                                   sim_legacy::World& world,
                                   bool dedupeBySeq)
{
    for (const auto& event : events)
    {
        if (dedupeBySeq && event.seq < m_lastEventSequence)
            continue;
        m_camera.consume(event);
        m_sound.consume(event);
        m_hurtNumbers.consume(event, world);
        if (const auto* intensity = std::get_if<sim_legacy::RoleIntensity>(&event.payload))
            m_avatars.setEntityIntensity(event.entity.value, renderValue(intensity->m_value));
        if (const auto* partnerRage = std::get_if<sim_legacy::PartnerRageChanged>(&event.payload))
            m_avatars.onPartnerRageChanged(world, event.entity, partnerRage->m_active);
        if (dedupeBySeq)
            m_lastEventSequence = event.seq + 1;
    }
}

NS_MG_END

#endif
