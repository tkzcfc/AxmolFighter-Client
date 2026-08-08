#pragma once

#include "Entity.h"
#include "ECSManager.h"

#define MG_SYSTEM_ADD_REQUIRED_COMPONENT(system, ecs, componentType) \
    system->addRequiredComponent(ecs->getComponentTypeId(#componentType))

NS_MG_BEGIN

class System : public Object
{
public:
    typedef Object Super;

public:
    System();

    virtual ~System();

    virtual void init(ECSManager* ecs);

    virtual void update();

    bool filtersMatch(const Signature& entitySignature) const;

    bool containsComponentType(ComponentTypeId typeId) const;

    bool isActive() const;

    void addRequiredComponent(ComponentTypeId typeId);

    std::string getName() const;

    ECSManager* getECSManager() const;

    GameWord* getGameWord() const;

private:
    virtual void onEntityAdded(Entity* entity) {}

    virtual void onEntityRemoved(Entity* entity) {}

private:
    void addEntity(Entity* entity);

    void removeEntity(Entity* entity);

protected:
    friend class ECSManager;
    ECSManager* m_ecsManager;
    Signature m_signature;
    std::vector<Entity*> entities;
    std::string m_name;
};

NS_MG_END
