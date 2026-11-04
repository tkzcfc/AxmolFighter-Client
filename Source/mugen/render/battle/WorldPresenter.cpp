#include "WorldPresenter.h"

#ifdef RUNTIME_IN_AXMOL

#    include "core/audio/AudioManager.h"
#    include "mugen/avatar/AvatarPaths.h"
#    include "mugen/conf/LayerLoader.h"
#    include "mugen/conf/ReflectedConfig.h"
#    include "mugen/conf/TypeConversions.h"
#    include "mugen/conf/tables/PetTables.h"
#    include "mugen/sim/effect/Effect.h"
#    include "mugen/sim/room/RoomEntities.h"
#    include "mugen/sim/role/RoleComponents.h"
#    include "ui/progress/SessionFashionAppearance.h"

#    include <algorithm>
#    include <cmath>

NS_MG_BEGIN

namespace
{
// 受击框调试色；引擎没有预置青色常量。
const ax::Color4F kHurtBoxColor{0.0f, 1.0f, 1.0f, 1.0f};
// 单帧位移平方超过该值视为瞬移（换房/位置吸附），跳过插值直接吸附。
constexpr float kTeleportSnapDistanceSq = 500.0f * 500.0f;

float renderValue(Fixed value)
{
    return value.toFloatForRender();
}

int32_t chooseSoundId(const sim::Sound& sound, std::minstd_rand& random)
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
AvatarLook resolveLook(sim::World& world, Entity entity, sim::GroupKind kind)
{
    auto& ecs          = world.ecs();
    const auto& config = conf::Config::instance();
    switch (kind)
    {
    case sim::GroupKind::Role:
    {
        const int32_t roleId = ecs.get<sim::RoleIdentity>(entity).roleId;
        AvatarLook look      = lookFromRow(config.get<conf::RoleConfig>(roleId));
        look.roleId          = roleId;
        look.characterClass  = type_conversions::toCharacterClass(roleId);
        return look;
    }
    case sim::GroupKind::Effect:
        return lookFromRow(config.get<conf::EffectConfig>(ecs.get<sim::EffectState>(entity).m_configId));
    case sim::GroupKind::Obstacle:
        return lookFromRow(config.get<conf::ObstacleConfig>(ecs.get<sim::ObstacleState>(entity).m_id));
    case sim::GroupKind::Goods:
        return lookFromRow(config.get<conf::GoodsConfig>(ecs.get<sim::GoodsState>(entity).m_id));
    case sim::GroupKind::Portal:
        return lookFromRow(config.get<conf::PortalConfig>(ecs.get<sim::PortalState>(entity).m_id));
    case sim::GroupKind::Count:
        break;
    }
    MG_LOG_E("AvatarPresenter: invalid entity group {}", static_cast<int>(kind));
    MG_ASSERT(false && "Invalid entity group for avatar");
    return {};
}

bool isLocalPlayerRole(sim::World& world, Entity entity)
{
    const auto* identity = world.ecs().tryGet<sim::RoleIdentity>(entity);
    return identity && identity->playerSlot.has_value() && !identity->isAi;
}

// 按外观配置创建 Avatar，动作文件与逻辑侧绑定的同一份。
// 职业角色按职业默认外观创建：时装图集与皮肤、默认武器挂件（待机时显示，其余动作嫁接到身体插槽）。
// 本地玩家用会话时装/武器外观（可后续 setAppearance 换装）。
Avatar* createAvatar(const AvatarLook& look, const SpineInfo& spine, const std::string& motionFile,
                     bool useSessionFashion)
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

void AvatarPresenter::present(sim::World& world, float alpha, float renderDelta)
{
    std::set<uint32_t> live;
    world.ecs().each<sim::EntityGroup, sim::Transform, sim::MotionState>(
        [&](Entity entity, const sim::EntityGroup& group, const sim::Transform& transform,
            const sim::MotionState& motion) {
        live.insert(entity.value);
        auto& state = m_avatars[entity.value];
        if (!state.resolved)
        {
            state.resolved        = true;
            const AvatarLook look = resolveLook(world, entity, group.kind);
            const SpineInfo spine = AvatarPaths::resolveSpine(look.resSpineId);
            if (!spine.valid)
                return;
            if (motion.motionFile.empty())
                state.skeleton = MgSkeletonAnimation::createFromCache(look.resSpineId);
            else
            {
                const bool sessionFashion =
                    group.kind == sim::GroupKind::Role && isLocalPlayerRole(world, entity);
                state.avatar = createAvatar(look, spine, motion.motionFile, sessionFashion);
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
        // 渲染帧按 alpha 在两者间插值，得到平滑运动而非 30Hz 阶梯。
        const uint64_t frame = world.ecs().singleton<sim::WorldClock>().frame;
        if (!state.hasPrevious)
        {
            // 首次出现：前后帧均取当前变换，避免从原点插值。
            state.previous    = transform;
            state.current     = transform;
            state.lastFrame   = frame;
            state.hasPrevious = true;
        }
        else if (frame != state.lastFrame)
        {
            const float dx = renderValue(transform.position.x - state.current.position.x);
            const float dy = renderValue(transform.position.y - state.current.position.y);
            const float dz = renderValue(transform.position.z - state.current.position.z);
            // 跳帧或大幅瞬移（换房/位置吸附）直接吸附，避免插值拖尾。
            const bool snap = frame - state.lastFrame > 1 || (dx * dx + dy * dy + dz * dz) > kTeleportSnapDistanceSq;
            state.previous  = snap ? transform : state.current;
            state.current   = transform;
            state.lastFrame = frame;
        }
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
            if (ax::Node* node = displayNode(it->second))
                node->removeFromParent();
            it = m_avatars.erase(it);
        }
        else
            ++it;
    }

    if (m_petFollower && m_followEntity.value != 0)
    {
        RenderPose hero;
        if (samplePose(m_followEntity.value, alpha, &hero))
            m_petFollower->present({hero.x, hero.y, hero.z, hero.towardX, hero.towardZ}, renderDelta);
    }
}

AvatarPresenter::RenderPose AvatarPresenter::interpolatePose(const AvatarState& state, float alpha)
{
    RenderPose pose;
    pose.x = renderValue(state.previous.position.x) +
             (renderValue(state.current.position.x) - renderValue(state.previous.position.x)) * alpha;
    pose.y = renderValue(state.previous.position.y) +
             (renderValue(state.current.position.y) - renderValue(state.previous.position.y)) * alpha;
    pose.z = renderValue(state.previous.position.z) +
             (renderValue(state.current.position.z) - renderValue(state.previous.position.z)) * alpha;
    pose.towardX = state.current.towardX < 0 ? -1.0f : 1.0f;
    pose.towardZ = state.current.vectorZ < Fixed{} ? -1.0f : 1.0f;
    return pose;
}

bool AvatarPresenter::samplePose(uint32_t entityValue, float alpha, RenderPose* out) const
{
    const auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end() || !it->second.hasPrevious || !out)
        return false;
    *out = interpolatePose(it->second, alpha);
    return true;
}

void AvatarPresenter::setPet(int32_t petId, int32_t spineId)
{
    if (!m_petFollower)
        m_petFollower = std::make_unique<PetFollower>(tierLayer(2));
    m_petFollower->setPet(petId, spineId);
}

void AvatarPresenter::clearPet()
{
    if (m_petFollower)
        m_petFollower->clear();
}

ax::Node* AvatarPresenter::displayNode(const AvatarState& state)
{
    if (state.avatar)
        return state.avatar;
    return state.skeleton;
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
    clearPet();
}

bool AvatarPresenter::setEntityAppearance(uint32_t entityValue, const FashionAppearance& appearance)
{
    auto it = m_avatars.find(entityValue);
    if (it == m_avatars.end() || !it->second.avatar)
        return false;
    return it->second.avatar->setAppearance(appearance);
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
    if (petId == 0)
    {
        m_avatars.clearPet();
        return;
    }
    const auto* pet = conf::Config::instance().find<conf::PetConfig>(petId);
    if (!pet)
    {
        MG_LOG_W("WorldPresenter: pet {} not found", petId);
        m_avatars.clearPet();
        return;
    }
    if (pet->spineId <= 0)
    {
        MG_LOG_W("WorldPresenter: pet {} has invalid spine {}", petId, pet->spineId);
        m_avatars.clearPet();
        return;
    }
    m_avatars.setPet(petId, pet->spineId);
}

bool WorldPresenter::refreshLocalPlayerAppearance(sim::World& world)
{
    Entity local{};
    bool found = false;
    world.ecs().each<sim::RoleIdentity>([&](Entity entity, const sim::RoleIdentity& identity) {
        if (found)
            return;
        if (identity.playerSlot.has_value() && !identity.isAi)
        {
            local = entity;
            found = true;
        }
    });
    if (!found)
        return false;
    FashionAppearance appearance = gameui::buildSessionFashionAppearance();
    if (appearance.roleId == 0)
        appearance.roleId = world.ecs().get<sim::RoleIdentity>(local).roleId;
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

void CameraDirector::follow(sim::World& world, Entity entity)
{
    const auto* transform = world.ecs().tryGet<sim::Transform>(entity);
    if (!transform || !m_map.camera())
        return;
    m_map.camera()->setFocusPosition({renderValue(transform->position.x), renderValue(transform->position.z)});
}

void CameraDirector::consume(const sim::PresentationEvent& event)
{
    if (const auto* shake = std::get_if<sim::CameraShake>(&event.payload); shake && m_map.camera())
        m_map.camera()->shake(renderValue(shake->strength), renderValue(shake->durationMs));
    if (std::holds_alternative<sim::DisplaySpine>(event.payload))
        m_displaySlowScale = 0.35f;
}

void CameraDirector::update(float delta)
{
    m_displaySlowScale = std::min(1.0f, m_displaySlowScale + delta * 2.0f);
    m_map.update(delta * m_displaySlowScale);
}

void SoundPlayer::consume(const sim::PresentationEvent& event)
{
    const auto* sound = std::get_if<sim::Sound>(&event.payload);
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

void HurtNumber::consume(const sim::PresentationEvent& event, sim::World& world)
{
    const auto* hurt      = std::get_if<sim::HurtNumber>(&event.payload);
    const auto* transform = world.ecs().tryGet<sim::Transform>(event.entity);
    if (!hurt || !transform)
        return;
    auto* label = ax::Label::createWithSystemFont(std::to_string(hurt->value), "Arial", 24);
    label->setPosition(renderValue(transform->position.x), renderValue(transform->position.y) + 48.0f);
    m_root.addChild(label, 200);
    label->runAction(ax::Sequence::create(ax::MoveBy::create(0.45f, {0.0f, 32.0f}), ax::RemoveSelf::create(), nullptr));
}

DebugDraw::DebugDraw(ax::Node& root)
{
    m_node = ax::DrawNode::create();
    root.addChild(m_node, 300);
}

void DebugDraw::present(sim::World& world)
{
    m_node->clear();
    if (!m_enabled)
        return;
    // WorldBox 已是世界坐标，pos 为 x/z 平面最小角，与碰撞判定的轴对齐框一致
    const auto drawBoxes = [this](const std::vector<sim::WorldBox>& boxes, const ax::Color4F& color) {
        for (const auto& box : boxes)
            m_node->drawRect({renderValue(box.pos.x), renderValue(box.pos.z)},
                             {renderValue(box.pos.x + box.size.x), renderValue(box.pos.z + box.size.z)}, color);
    };
    world.ecs().each<sim::Transform, sim::ColliderState>(
        [&](Entity, const sim::Transform& transform, const sim::ColliderState& collider) {
        const ax::Vec2 center{renderValue(transform.position.x), renderValue(transform.position.z)};
        m_node->drawCircle(center, renderValue(collider.radius), 0.0f, 16, false, ax::Color4F::YELLOW);
        drawBoxes(collider.attack, ax::Color4F::RED);
        drawBoxes(collider.damage, kHurtBoxColor);
    });
    for (const auto& range : world.ecs().singleton<sim::MapRegion>().moveRanges)
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
}

void WorldPresenter::loadMap(const std::string& mapKey)
{
    if (!mapKey.empty())
        m_map.load(mapKey);
}

void WorldPresenter::present(sim::World& world, float interpolationAlpha, float renderDelta)
{
    m_avatars.present(world, std::clamp(interpolationAlpha, 0.0f, 1.0f), renderDelta);
    const auto& events = world.ecs().singleton<sim::PresentationEvents>().events;
    for (const auto& event : events)
    {
        if (event.seq < m_lastEventSequence)
            continue;
        m_camera.consume(event);
        m_sound.consume(event);
        m_hurtNumbers.consume(event, world);
        if (const auto* intensity = std::get_if<sim::RoleIntensity>(&event.payload))
            m_avatars.setEntityIntensity(event.entity.value, renderValue(intensity->m_value));
        m_lastEventSequence = event.seq + 1;
    }
    m_camera.follow(world, m_followEntity);
    m_camera.update(renderDelta);
    m_debugDraw.present(world);
}

NS_MG_END

#endif
