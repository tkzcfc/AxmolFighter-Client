#include "WorldPresenter.h"

#ifdef RUNTIME_IN_AXMOL

#    include "audio/AudioEngine.h"
#    include "mugen/avatar/AvatarPaths.h"
#    include "mugen/conf/LayerLoader.h"
#    include "mugen/conf/ReflectedConfig.h"
#    include "mugen/conf/TypeConversions.h"
#    include "mugen/sim/effect/Effect.h"
#    include "mugen/sim/room/RoomEntities.h"

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

// 按外观配置创建 Avatar，动作文件与逻辑侧绑定的同一份。
// 职业角色按职业默认外观创建：时装图集与皮肤、默认武器挂件（待机时显示，其余动作嫁接到身体插槽）。
Avatar* createAvatar(const AvatarLook& look, const SpineInfo& spine, const std::string& motionFile)
{
    if (look.characterClass != CharacterClass::kUnknown)
    {
        FashionAppearance appearance;
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
                state.avatar = createAvatar(look, spine, motion.motionFile);
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

        const sim::Transform& previous = state.hasPrevious ? state.previous : transform;
        const float x                  = renderValue(previous.position.x) +
                        (renderValue(transform.position.x) - renderValue(previous.position.x)) * alpha;
        const float y = renderValue(previous.position.y) +
                        (renderValue(transform.position.y) - renderValue(previous.position.y)) * alpha;
        // Spine 偏移：x 随水平朝向翻转，z 随纵深朝向翻转；层内按偏移后的纵深排序。
        const float towardX = transform.towardX < 0 ? -1.0f : 1.0f;
        const float towardZ = transform.vectorZ < Fixed{} ? -1.0f : 1.0f;
        const float depth   = renderValue(transform.position.z) + towardZ * offsetAxis(state.spineOffset, 2);
        node->setPosition(x + towardX * offsetAxis(state.spineOffset, 0), y + offsetAxis(state.spineOffset, 1) + depth);
        node->setLocalZOrder(-static_cast<int>(std::floor(depth)));
        node->setScaleX(towardX);
        state.previous    = transform;
        state.hasPrevious = true;
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
}

void MapPresenter::load(const std::string& mapKey)
{
    if (m_mapKey == mapKey)
        return;
    if (m_map)
        m_map->removeFromParent();
    m_mapKey               = mapKey;
    const std::string path = mapKey.ends_with(".layer") ? mapKey : "mugen/map/" + mapKey + ".layer";
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
    if (!sound || sound->id <= 0)
        return;
    const float volume = 0.95f + static_cast<float>(m_random() % 11) * 0.01f;
    ax::AudioEngine::play2d(fmt::format("mugen/sound/{}.ogg", sound->id), false, volume);
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
        m_lastEventSequence = event.seq + 1;
    }
    m_camera.follow(world, m_followEntity);
    m_camera.update(renderDelta);
    m_debugDraw.present(world);
}

NS_MG_END

#endif
