#pragma once

#include "Types.h"
#include "Component.h"

#define MG_GET_COMPONENT(ENTITY, TYPE) static_cast<TYPE*>(ENTITY->getComponent(#TYPE))
#define MG_ADD_COMPONENT(ENTITY, TYPE) static_cast<TYPE*>(ENTITY->addComponent(#TYPE))

NS_MG_BEGIN

class ECSManager;
class GameWord;
class Entity : public Object
{
public:
    typedef Object Super;

public:
    Entity();

    virtual ~Entity();

    EntityId getId() const;

    Component* addComponent(const std::string& name);

    void removeComponent(const std::string& name);

    void removeAllComponents();

    bool containsComponent(const std::string& name) const;

    bool containsComponentByTypeId(ComponentTypeId typeId) const;

    Component* getComponent(const std::string& name) const;

    Component* getComponentByTypeId(ComponentTypeId typeId) const;

    bool isPendingRemoval() const;

    Signature getSignature() const;

    void destroy();

    ECSManager* getECSManager() const;

    GameWord* getGameWord() const;

    void notifyEntityReady() const;

public:
    virtual void serialize(ByteBuffer& byteBuffer) const override;

    virtual bool deserialize(ByteBuffer& byteBuffer) override;

private:
    friend class ECSManager;
    ECSManager* m_ecsManager;
    EntityId m_id;
    // Indicates whether the entity is marked for removal
    bool m_pendingRemoval;
    bool isReady;

    MG_SYNTHESIZE_READONLY_BY_REF(std::vector<Component*>, m_components, Components);
};

NS_MG_END
