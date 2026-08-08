#pragma once

#include "mugen/core/ecs/Types.h"
#include "mugen/render/RenderStashKey.h"

#include <memory>
#include <unordered_map>
#include <vector>

NS_MG_BEGIN

class VirtualCamera;

// 反序列化期间按 RenderStashKey 回收/获取渲染对象，供实体重建后复用。
class RenderObjectPool
{
public:
    static RenderObjectPool* getInstance();

    RenderObjectPool();
    ~RenderObjectPool();

#ifdef RUNTIME_IN_AXMOL
    void recycleNode(const RenderStashKey& key, ax::Node* node);
    ax::Node* acquireNode(const RenderStashKey& key);

    template <typename T>
    T* acquireNode(const RenderStashKey& key)
    {
        return dynamic_cast<T*>(acquireNode(key));
    }

    void recycleCamera(const RenderStashKey& key, std::unique_ptr<VirtualCamera> cam);
    std::unique_ptr<VirtualCamera> acquireCamera(const RenderStashKey& key);

    void clear();
    void clear(EntityId id);
    void update(float dt);
#endif

private:
#ifdef RUNTIME_IN_AXMOL
    std::unordered_map<RenderStashKey, ax::Node*, RenderStashKey::Hash> m_nodes;
    std::unordered_map<RenderStashKey, std::unique_ptr<VirtualCamera>, RenderStashKey::Hash> m_cameras;
    std::vector<ax::Node*> m_nodesToRelease;
#endif

    static RenderObjectPool* s_instance;
};

NS_MG_END
