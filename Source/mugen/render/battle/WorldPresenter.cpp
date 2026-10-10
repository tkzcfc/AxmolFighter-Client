#include "WorldPresenter.h"

#ifdef RUNTIME_IN_AXMOL

#    include "audio/AudioEngine.h"
#    include "mugen/avatar/AvatarPaths.h"
#    include "mugen/conf/LayerLoader.h"
#    include "mugen/conf/TypeConversions.h"

#    include <algorithm>

NS_MG_BEGIN

namespace
{
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

Avatar* createAvatar(const sim::RoleIdentity& identity)
{
    const RoleSpineInfo spine = AvatarPaths::resolveRoleSpine(identity.roleId);
    if (!spine.valid)
        return nullptr;
    AvatarDesc desc;
    desc.skeleton       = spine.skeleton;
    desc.atlases        = {spine.atlas};
    desc.scale          = spine.scale;
    desc.motionFile     = AvatarPaths::motionFileFromSpine(spine.skeleton);
    desc.characterClass = type_conversions::toCharacterClass(identity.roleId);
    return Avatar::create(desc, AvatarLoadMode::kSyncShared);
}
}

void AvatarPresenter::present(sim::World& world, float alpha)
{
    std::set<uint32_t> live;
    world.ecs().each<sim::RoleIdentity, sim::Transform, sim::MotionState>(
        [&](Entity entity, const sim::RoleIdentity& identity, const sim::Transform& transform, const sim::MotionState& motion) {
            live.insert(entity.value);
            auto& state = m_avatars[entity.value];
            if (!state.avatar)
            {
                state.avatar = createAvatar(identity);
                if (!state.avatar)
                    return;
                m_root.addChild(state.avatar);
            }
            if (state.motionName != motion.motionName || state.entryId != motion.entryId)
            {
                state.avatar->setMotion(motion.motionName, motion.entryId, motion.loop);
                state.motionName = motion.motionName;
                state.entryId    = motion.entryId;
            }
            state.avatar->seek(static_cast<int>(motion.timeMs.toInt()));

            const sim::Transform& previous = state.hasPrevious ? state.previous : transform;
            const float x = renderValue(previous.position.x) +
                            (renderValue(transform.position.x) - renderValue(previous.position.x)) * alpha;
            const float y = renderValue(previous.position.y) +
                            (renderValue(transform.position.y) - renderValue(previous.position.y)) * alpha;
            state.avatar->setPosition(x, y + renderValue(transform.position.z));
            state.avatar->setScaleX(transform.towardX < 0 ? -1.0f : 1.0f);
            state.previous    = transform;
            state.hasPrevious = true;
        });
    for (auto it = m_avatars.begin(); it != m_avatars.end();)
    {
        if (!live.contains(it->first))
        {
            if (it->second.avatar)
                it->second.avatar->removeFromParent();
            it = m_avatars.erase(it);
        }
        else
            ++it;
    }
}

void AvatarPresenter::clear()
{
    for (auto& [_, state] : m_avatars)
        if (state.avatar)
            state.avatar->removeFromParent();
    m_avatars.clear();
}

void MapPresenter::load(const std::string& mapKey)
{
    if (m_mapKey == mapKey)
        return;
    if (m_map)
        m_map->removeFromParent();
    m_mapKey = mapKey;
    const std::string path = mapKey.ends_with(".layer") ? mapKey : "mugen/map/" + mapKey + ".layer";
    m_map                   = LayerRuntimeLoader::loadNode(path);
    m_root.addChild(m_map, -100);
    m_camera = VirtualCamera::create();
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
    const auto* hurt = std::get_if<sim::HurtNumber>(&event.payload);
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
    world.ecs().each<sim::Transform, sim::ColliderState>(
        [&](Entity, const sim::Transform& transform, const sim::ColliderState& collider) {
            const ax::Vec2 center{renderValue(transform.position.x), renderValue(transform.position.z)};
            m_node->drawCircle(center, renderValue(collider.radius), 0.0f, 16, false, ax::Color4F::YELLOW);
            for (const auto& box : collider.attack)
                m_node->drawRect({center.x + renderValue(box.pos.x) - renderValue(box.size.x) * 0.5f,
                                  center.y + renderValue(box.pos.z) - renderValue(box.size.z) * 0.5f},
                                 {center.x + renderValue(box.pos.x) + renderValue(box.size.x) * 0.5f,
                                  center.y + renderValue(box.pos.z) + renderValue(box.size.z) * 0.5f},
                                 ax::Color4F::RED);
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
    m_avatars.present(world, std::clamp(interpolationAlpha, 0.0f, 1.0f));
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
