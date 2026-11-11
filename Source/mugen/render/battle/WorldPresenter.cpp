#include "WorldPresenter.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/conf/ReflectedConfig.h"
#    include "mugen/conf/tables/PetTables.h"
#    include "mugen/sim_legacy/role/RoleComponents.h"
#    include "ui/progress/SessionFashionAppearance.h"

#    include <algorithm>

NS_MG_BEGIN

namespace
{
ax::Node& createNamedChild(ax::Node& parent, const char* name, int zOrder = 0)
{
    auto* node = ax::Node::create();
    node->setName(name);
    parent.addChild(node, zOrder);
    return *node;
}
}  // namespace

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

WorldPresenter::WorldPresenter(ax::Node& root)
    : m_world(createNamedChild(root, "world"))
    , m_overlay(createNamedChild(m_world, "overlay", 1000))
    , m_map(m_world)
    , m_camera(m_map)
    , m_hurtNumbers(m_overlay)
    , m_debugDraw(m_overlay)
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
    // 换图会销毁旧地图树（含 entity 层上的角色节点），先摘下显示节点再加载。
    m_avatars.clear();
    m_map.load(mapKey);
    m_avatars.setEntityLayer(m_map.entityLayer());
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
            m_avatars.setEntityIntensity(event.entity.value, intensity->m_value.toFloatForRender());
        if (const auto* partnerRage = std::get_if<sim_legacy::PartnerRageChanged>(&event.payload))
            m_avatars.onPartnerRageChanged(world, event.entity, partnerRage->m_active);
        if (dedupeBySeq)
            m_lastEventSequence = event.seq + 1;
    }
}

NS_MG_END

#endif
