#pragma once

#include "Types.h"
#include "Entity.h"
#include "Component.h"
#include <functional>

NS_MG_BEGIN

class ECSManager;
class System;
using ComponentCreateFuncType = std::function<Component*()>;
using SystemCreateFuncType    = std::function<System*(ECSManager*)>;

#define MG_GET_SYSTEM(ECS_MANAGER, TYPE) static_cast<TYPE*>(ECS_MANAGER->getSystem(#TYPE))

class ECSManager : public Object
{
public:
    typedef Object Super;

public:
    ECSManager();

    virtual ~ECSManager();

public:
    Entity* newEntity();

    Entity* getEntity(EntityId entityId);

    void destroyEntityById(EntityId entityId);

    void destroyEntity(Entity* entity);

    void destroyAllEntities();

    // 通过Signature来获取符合条件的实体列表
    std::vector<Entity*> getEntitiesBySignature(const Signature& signature) const;

public:
    void registerSystem(const std::string& name, const SystemCreateFuncType& createFunc);

    System* addSystem(const std::string& name);

    System* getSystem(const std::string& name) const;

    void removeSystem(const std::string& name);

    void removeAllSystems();

public:
    void registerComponent(const std::string& name, const ComponentCreateFuncType& createFunc);

    uint32_t getComponentTypeId(const std::string& name) const;

    const std::string& getComponentName(ComponentTypeId typeId) const;

    Component* addComponent(Entity* entity, const std::string& name);

    void removeComponent(Entity* entity, const std::string& name);

    void notifyEntityReady(Entity* entity);

public:
    void update(int32_t ms);

public:
    virtual void serialize(ByteBuffer& byteBuffer) const override;

    virtual bool deserialize(ByteBuffer& byteBuffer) override;

    // 反序列化完成后调用，进行一些必要的初始化
    void postDeserializeInit();

private:
    void doRemoveEntities();

private:
    struct ComponentMeta
    {
        uint32_t typeId;
        ComponentCreateFuncType createFunc;
    };
    std::unordered_map<std::string, ComponentMeta> m_componentMetas;

    struct SystemMeta
    {
        SystemCreateFuncType createFunc;
    };
    std::unordered_map<std::string, SystemMeta> m_systemMetas;

    std::vector<System*> m_systems;
    std::vector<Entity*> m_entities;

    bool m_willRemoveEntities;
    EntityId m_nextEntityId;

    MG_SYNTHESIZE_IS_READONLY(bool, m_isDeserialized, Deserialized);
    MG_SYNTHESIZE_READONLY(int32_t, m_lastUpdateTimeMs, LastUpdateTimeMs)
    MG_SYNTHESIZE_READONLY(int64_t, m_runningTimeMs, RunningTimeMs)
    MG_SYNTHESIZE(void*, m_userdata, Userdata);
};

NS_MG_END
