#include "Entity.h"
#include "ECSManager.h"

NS_MG_BEGIN

Entity::Entity() : m_pendingRemoval(false), m_id(INVALID_ENTITY_ID), m_ecsManager(nullptr), isReady(false)
{
    MG_ECS_OBJECT_GC_LOG("[%p] new Entity\n", this);
    m_components.reserve(10);
}
Entity::~Entity()
{
    std::vector<std::string> componentNames;
    componentNames.reserve(m_components.size());
    for (auto* it : m_components)
    {
        componentNames.push_back(m_ecsManager->getComponentName(it->getTypeId()));
    }

    for (size_t i = componentNames.size(); i > 0; --i)
    {
        m_ecsManager->removeComponent(this, componentNames[i - 1]);
    }
    assert(m_components.empty() && "Entity components not fully removed.");
    MG_ECS_OBJECT_GC_LOG("[%p] delete Entity\n", this);
}

EntityId Entity::getId() const
{
    return m_id;
}

Component* Entity::addComponent(const std::string& name)
{
    return m_ecsManager->addComponent(this, name);
}

void Entity::removeComponent(const std::string& name)
{
    m_ecsManager->removeComponent(this, name);
}

void Entity::removeAllComponents()
{
    std::vector<std::string> componentNames;
    componentNames.reserve(m_components.size());
    for (auto* it : m_components)
    {
        componentNames.push_back(m_ecsManager->getComponentName(it->getTypeId()));
    }
    for (const auto& name : componentNames)
    {
        m_ecsManager->removeComponent(this, name);
    }
}

bool Entity::containsComponent(const std::string& name) const
{
    return getComponent(name) != nullptr;
}

bool Entity::containsComponentByTypeId(ComponentTypeId typeId) const
{
    return getComponentByTypeId(typeId) != nullptr;
}

Component* Entity::getComponent(const std::string& name) const
{
    auto typeId = m_ecsManager->getComponentTypeId(name);
    for (auto* component : m_components)
    {
        if (component->m_typeId == typeId)
        {
            return component;
        }
    }
    return nullptr;
}

Component* Entity::getComponentByTypeId(ComponentTypeId typeId) const
{
    for (auto* component : m_components)
    {
        if (component->m_typeId == typeId)
        {
            return component;
        }
    }
    return nullptr;
}

bool Entity::isPendingRemoval() const
{
    return m_pendingRemoval;
}

Signature Entity::getSignature() const
{
    Signature signature;
    for (auto* comp : m_components)
    {
        signature.set(comp->m_typeId);
    }
    return signature;
}

void Entity::destroy()
{
#if _DEBUG
    MG_ASSERT(m_ecsManager != nullptr && "ECSManager is null.");
    MG_ASSERT(!m_pendingRemoval && "Entity is already pending removal.");
#endif
    m_ecsManager->destroyEntity(this);
}

ECSManager* Entity::getECSManager() const
{
    return m_ecsManager;
}

GameWord* Entity::getGameWord() const
{
    if (m_ecsManager)
    {
        return reinterpret_cast<GameWord*>(m_ecsManager->getUserdata());
    }
    else
    {
        return nullptr;
    }
}

void Entity::notifyEntityReady() const
{
    if (m_ecsManager)
    {
        m_ecsManager->notifyEntityReady(const_cast<Entity*>(this));
    }
}

void Entity::serialize(ByteBuffer& byteBuffer) const
{
    Super::serialize(byteBuffer);

    std::vector<std::string> compNames;
    compNames.reserve(m_components.size());
    for (const auto* component : m_components)
    {
        compNames.push_back(m_ecsManager->getComponentName(component->getTypeId()));
    }
    byteBuffer.writeValue(compNames);

#ifdef _DEBUG
#    define PRINT_COMPONENT_MEMORY_USAGE 1
#endif

#if PRINT_COMPONENT_MEMORY_USAGE
    MG_LOG_D("\n\n");
    MG_LOG_D("{}: serializing entity with {} components", __FUNCTION__, compNames.size());
#endif

    for (const auto* component : m_components)
    {

#if PRINT_COMPONENT_MEMORY_USAGE
        auto prePos = byteBuffer.getPosition();
#endif

        byteBuffer.writeObject(*component);

#if PRINT_COMPONENT_MEMORY_USAGE
        // 打印每个组件的序列化大小，便于调试和优化
        MG_LOG_D("{}: serialized component {} size {} bytes", __FUNCTION__,
                 m_ecsManager->getComponentName(component->getTypeId()), byteBuffer.getPosition() - prePos);
#endif
    }

#if PRINT_COMPONENT_MEMORY_USAGE
    MG_LOG_D("\n\n");
#endif
}

bool Entity::deserialize(ByteBuffer& byteBuffer)
{
    do
    {
        MG_BREAK_IF(!Super::deserialize(byteBuffer));

        removeAllComponents();

        std::vector<std::string> compNames;
        MG_BREAK_IF(!byteBuffer.getValue(compNames));

        std::vector<Component*> newComponents;
        newComponents.reserve(compNames.size());
        for (const auto& name : compNames)
        {
            Component* component = addComponent(name);
            if (component == nullptr)
            {
                return false;
            }
            newComponents.push_back(component);
        }

        for (auto* component : newComponents)
        {
            if (!byteBuffer.getObject(*component))
            {
                return false;
            }
        }

        return true;
    } while (false);
    return false;
}

NS_MG_END
