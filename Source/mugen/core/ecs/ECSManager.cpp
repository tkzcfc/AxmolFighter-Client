#include "ECSManager.h"
#include "System.h"

NS_MG_BEGIN

ECSManager::ECSManager()
    : m_isDeserialized(false)
    , m_willRemoveEntities(false)
    , m_nextEntityId(0)
    , m_userdata(nullptr)
    , m_lastUpdateTimeMs(0)
    , m_runningTimeMs(0)
{
    m_systems.reserve(20);
    m_entities.reserve(200);
}

ECSManager::~ECSManager()
{
    destroyAllEntities();
    removeAllSystems();
}

Entity* ECSManager::newEntity()
{
    m_nextEntityId++;
    auto entity          = new Entity();
    entity->m_id         = m_nextEntityId;
    entity->m_ecsManager = this;
    m_entities.push_back(entity);
    MG_ECS_LOG("[ECS] New Entity [%u] created.\n", entity->getId());
    return entity;
}

Entity* ECSManager::getEntity(EntityId entityId)
{
    for (auto& entity : m_entities)
    {
        if (entity->getId() == entityId)
        {
            return entity;
        }
    }
    return nullptr;
}

void ECSManager::destroyEntityById(EntityId entityId)
{
    destroyEntity(getEntity(entityId));
}

void ECSManager::destroyEntity(Entity* entity)
{
    if (entity)
    {
        entity->m_pendingRemoval = true;
        m_willRemoveEntities     = true;
    }
}

void ECSManager::destroyAllEntities()
{
    for (auto* it : m_entities)
    {
        it->m_pendingRemoval = true;
    }
    m_willRemoveEntities = true;
    doRemoveEntities();
}

std::vector<Entity*> ECSManager::getEntitiesBySignature(const Signature& signature) const
{
    std::vector<Entity*> result;
    result.reserve(m_entities.size());
    for (auto entity : m_entities)
    {
        if (signature.none() || (entity->getSignature() & signature) == signature)
        {
            result.push_back(entity);
        }
    }
    return result;
}

void ECSManager::registerSystem(const std::string& name, const SystemCreateFuncType& createFunc)
{
    MG_ASSERT(m_systemMetas.find(name) == m_systemMetas.end() && "System type already registered.");

    MG_ECS_LOG("[ECS] RegisterSystem: %s\n", name.c_str());
    SystemMeta meta;
    meta.createFunc     = createFunc;
    m_systemMetas[name] = meta;
}

System* ECSManager::addSystem(const std::string& name)
{
    auto it = m_systemMetas.find(name);
    if (it == m_systemMetas.end())
    {
        MG_ASSERT(false && "System type not registered.");
        return nullptr;
    }

    auto system = it->second.createFunc(this);
    MG_ASSERT(system != nullptr && "Failed to create system instance.");

    if (system == nullptr)
    {
        return nullptr;
    }

    MG_ECS_LOG("[ECS] Add System: %s\n", name.c_str());
    system->m_name = name;
    m_systems.push_back(system);

    for (auto entity : m_entities)
    {
        if (entity->isReady && system->filtersMatch(entity->getSignature()))
        {
            system->addEntity(entity);
        }
    }

    return system;
}

System* ECSManager::getSystem(const std::string& name) const
{
    for (auto* system : m_systems)
    {
        if (system->getName() == name)
        {
            return system;
        }
    }
    return nullptr;
}

void ECSManager::removeSystem(const std::string& name)
{
    for (auto it = m_systems.begin(); it != m_systems.end(); ++it)
    {
        if ((*it)->getName() == name)
        {
            MG_ECS_LOG("[ECS] Remove System: %s\n", name.c_str());
            delete *it;
            m_systems.erase(it);
            return;
        }
    }
}

void ECSManager::removeAllSystems()
{
    while (m_systems.size() > 0)
    {
        removeSystem(m_systems.back()->getName());
    }
}

void ECSManager::registerComponent(const std::string& name, const ComponentCreateFuncType& createFunc)
{
    MG_ASSERT(m_componentMetas.find(name) == m_componentMetas.end() && "Component type already registered.");

    ComponentMeta meta;
    meta.createFunc        = createFunc;
    meta.typeId            = static_cast<uint32_t>(m_componentMetas.size()) + 1;  // Start IDs from 1
    m_componentMetas[name] = meta;

    MG_ECS_LOG("[ECS] RegisterComponent: %s (typeId=%u)\n", name.c_str(), meta.typeId);
}

uint32_t ECSManager::getComponentTypeId(const std::string& name) const
{
    auto it = m_componentMetas.find(name);
    if (it != m_componentMetas.end())
    {
        return it->second.typeId;
    }
    MG_ASSERT(false && "Component type not registered.");
    return INVALID_COMPONENT_TYPE_ID;
}

const std::string& ECSManager::getComponentName(ComponentTypeId typeId) const
{
    for (const auto& pair : m_componentMetas)
    {
        if (pair.second.typeId == typeId)
        {
            return pair.first;
        }
    }
    MG_ASSERT(false && "Component type ID not registered.");
    static const std::string unknown = "Unknown Component";
    return unknown;
}

Component* ECSManager::addComponent(Entity* entity, const std::string& name)
{
    if (entity == nullptr)
    {
        MG_ASSERT(false && "Entity is null.");
        return nullptr;
    }

    auto it = m_componentMetas.find(name);
    if (it == m_componentMetas.end())
    {
        MG_ASSERT(false && "Component type not registered.");
        return nullptr;
    }

    Component* component = it->second.createFunc();
    if (component == nullptr)
    {
        MG_ASSERT(false && "Failed to create component instance.");
        return nullptr;
    }

    component->m_typeId = it->second.typeId;

    if (entity->containsComponentByTypeId(component->m_typeId))
    {
        MG_ASSERT(false && "Entity already contains component of this type.");
        delete component;
        return nullptr;
    }

    MG_ECS_LOG("[ECS] Entity [%u] addComponent: %s (typeId=%u)\n", entity->getId(), name.c_str(), component->m_typeId);
    entity->m_components.push_back(component);

    if (entity->isReady)
    {
        // Update entity signature in systems
        Signature entitySignature = entity->getSignature();
        for (auto* system : m_systems)
        {
            if (system->filtersMatch(entitySignature))
            {
                system->addEntity(entity);
            }
        }
    }

    return component;
}

void ECSManager::removeComponent(Entity* entity, const std::string& name)
{
    if (entity == nullptr)
    {
        MG_ASSERT(false && "Entity is null.");
        return;
    }

    auto typeId = getComponentTypeId(name);

    for (auto it = entity->m_components.begin(); it != entity->m_components.end(); ++it)
    {
        if ((*it)->m_typeId == typeId)
        {
            MG_ECS_LOG("[ECS] Entity [%u] removeComponent: %s (typeId=%u)\n", entity->getId(), name.c_str(), typeId);
            if (entity->isReady)
            {
                for (auto* system : m_systems)
                {
                    if (system->containsComponentType(typeId))
                    {
                        system->removeEntity(entity);
                    }
                }
            }

            delete *it;
            entity->m_components.erase(it);
            break;
        }
    }
}

void ECSManager::notifyEntityReady(Entity* entity)
{
    if (entity->isReady)
        return;

    entity->isReady = true;
    // Update entity signature in systems
    Signature entitySignature = entity->getSignature();
    for (auto* system : m_systems)
    {
        if (system->filtersMatch(entitySignature))
        {
            system->addEntity(entity);
        }
    }
}

void ECSManager::update(int32_t ms)
{
    m_runningTimeMs += ms;
    m_lastUpdateTimeMs = ms;

    doRemoveEntities();

#if _DEBUG
    for (auto entity : m_entities)
    {
        MG_ASSERT(entity->isReady);
    }
#endif

    for (auto system : m_systems)
    {
        if (system->isActive())
        {
            system->update();
        }
    }
}

void ECSManager::doRemoveEntities()
{
    if (m_willRemoveEntities)
    {
        std::vector<Entity*> entitiesToRemove;
        std::vector<Entity*> entitiesToKeep;

        entitiesToRemove.reserve(m_entities.size());
        entitiesToKeep.reserve(m_entities.size());

        for (auto entity : m_entities)
        {
            if (entity->m_pendingRemoval)
            {
                entitiesToRemove.push_back(entity);
            }
            else
            {
                entitiesToKeep.push_back(entity);
            }
        }

        m_entities.swap(entitiesToKeep);

        for (size_t i = entitiesToRemove.size(); i > 0; --i)
        {
            auto* entity  = entitiesToRemove[i - 1];
            auto entityId = entity->getId();
            delete entity;
            MG_ECS_LOG("[ECS] Destroy Entity [%u]\n", entityId);
        }
        m_willRemoveEntities = false;
    }
}

void ECSManager::serialize(ByteBuffer& byteBuffer) const
{
    Super::serialize(byteBuffer);

    // 序列化系统列表名称
    std::vector<std::string> systemNames;
    for (const auto* system : m_systems)
    {
        systemNames.push_back(system->getName());
    }
    byteBuffer.writeValue(systemNames);
    // 序列化系统状态
    for (const auto* system : m_systems)
    {
        byteBuffer.writeObject(*system);
    }

    // 只序列化未标记删除的实体
    uint32_t entityCount = 0;
    for (const auto* entity : m_entities)
    {
        if (!entity->m_pendingRemoval && entity->isReady)
        {
            entityCount++;
        }
    }
    byteBuffer.writeValue(entityCount);

    for (auto* entity : m_entities)
    {
        if (!entity->m_pendingRemoval && entity->isReady)
        {
            byteBuffer.writeUint32(entity->getId());
            byteBuffer.writeObject(*entity);
        }
    }

    // 序列化下一个实体ID
    byteBuffer.writeUint32(static_cast<uint32_t>(m_nextEntityId));
}

bool ECSManager::deserialize(ByteBuffer& byteBuffer)
{
    if (!Super::deserialize(byteBuffer))
    {
        return false;
    }

    // 清除应该正常清理的实体，准备反序列化
    doRemoveEntities();

    // 先销毁实体（系统仍在，渲染系统可 recycle 到 RenderObjectPool）
    m_isDeserialized = true;
    destroyAllEntities();
    m_isDeserialized = false;

    // 反序列化系统名称列表
    std::vector<std::string> systemNames;
    if (!byteBuffer.getValue(systemNames))
    {
        return false;
    }

    bool systemsUnchanged = (systemNames.size() == m_systems.size());
    if (systemsUnchanged)
    {
        for (size_t i = 0; i < systemNames.size(); ++i)
        {
            if (m_systems[i]->getName() != systemNames[i])
            {
                systemsUnchanged = false;
                break;
            }
        }
    }

    if (!systemsUnchanged)
    {
        removeAllSystems();
        for (auto& systemName : systemNames)
        {
            if (addSystem(systemName) == nullptr)
            {
                return false;
            }
        }
    }

    // 反序列化系统状态
    for (auto* system : m_systems)
    {
        if (!byteBuffer.getObject(*system))
        {
            return false;
        }
    }

    // 反序列化实体
    uint32_t entityCount = 0;
    if (!byteBuffer.getValue(entityCount))
    {
        return false;
    }

    for (uint32_t i = 0; i < entityCount; ++i)
    {
        EntityId entityId = 0;
        if (!byteBuffer.getUint32(entityId))
        {
            return false;
        }
        Entity* entity = newEntity();
        if (entity == nullptr)
        {
            return false;
        }
        entity->m_id = entityId;
        if (!byteBuffer.getObject(*entity))
        {
            return false;
        }
    }

    // 反序列化下一个实体ID
    if (!byteBuffer.getUint32(m_nextEntityId))
    {
        return false;
    }

    return true;
}

// 反序列化完成后调用，进行一些必要的初始化
void ECSManager::postDeserializeInit()
{
    // 标记为正在反序列化，避免在反序列化过程中触发系统的添加逻辑
    m_isDeserialized = true;

    for (auto* entity : m_entities)
    {
        notifyEntityReady(entity);
    }

    // 反序列化完成，重置标记
    m_isDeserialized = false;
}

NS_MG_END
