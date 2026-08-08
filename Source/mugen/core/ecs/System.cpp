#include "System.h"

NS_MG_BEGIN

System::System() : m_ecsManager(nullptr)
{
    MG_ECS_OBJECT_GC_LOG("[%p] new System\n", this);
}

System::~System()
{
    MG_ASSERT(entities.empty() && "System entities not fully removed.");
    MG_ECS_OBJECT_GC_LOG("[%p] System delete\n", this);
}

void System::init(ECSManager* ecs)
{
    m_ecsManager = ecs;
}

void System::update() {}

bool System::filtersMatch(const Signature& entitySignature) const
{
    return !m_signature.none() && !entitySignature.none() && ((entitySignature & m_signature) == m_signature);
}

bool System::containsComponentType(ComponentTypeId typeId) const
{
    return m_signature.test(static_cast<size_t>(typeId));
}

bool System::isActive() const
{
    return !entities.empty();
}

void System::addRequiredComponent(ComponentTypeId typeId)
{
    m_signature.set(static_cast<size_t>(typeId));
}

std::string System::getName() const
{
    return m_name;
}

ECSManager* System::getECSManager() const
{
    return m_ecsManager;
}

GameWord* System::getGameWord() const
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

void System::addEntity(Entity* entity)
{
    for (auto* it : entities)
    {
        if (it == entity)
        {
            return;
        }
    }
    entities.push_back(entity);
    onEntityAdded(entity);
    MG_ECS_LOG("[ECS] %s onEntityAdded [%u]\n", m_name.c_str(), entity->getId());
}

void System::removeEntity(Entity* entity)
{
    for (auto it = entities.begin(); it != entities.end(); ++it)
    {
        if (*it == entity)
        {
            MG_ECS_LOG("[ECS] %s onEntityRemoved [%u]\n", m_name.c_str(), entity->getId());
            onEntityRemoved(entity);
            entities.erase(it);
            break;
        }
    }
}

NS_MG_END
