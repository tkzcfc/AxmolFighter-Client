#include "RenderObjectPool.h"

#ifdef RUNTIME_IN_AXMOL
#    include "mugen/render/VirtualCamera.h"
#endif

NS_MG_BEGIN

RenderObjectPool* RenderObjectPool::s_instance = nullptr;

RenderObjectPool* RenderObjectPool::getInstance()
{
    if (s_instance == nullptr)
    {
        s_instance = new RenderObjectPool();
    }
    return s_instance;
}

RenderObjectPool::RenderObjectPool() = default;

RenderObjectPool::~RenderObjectPool()
{
#ifdef RUNTIME_IN_AXMOL
    clear();
#endif
}

#ifdef RUNTIME_IN_AXMOL

void RenderObjectPool::recycleNode(const RenderStashKey& key, ax::Node* node)
{
    if (key.empty() || node == nullptr)
    {
        return;
    }

    node->retain();
    node->removeFromParentAndCleanup(false);

    auto it = m_nodes.find(key);
    if (it != m_nodes.end())
    {
        if (it->second != node)
        {
            it->second->release();
            it->second = node;
        }
        return;
    }

    m_nodes.emplace(key, node);
}

ax::Node* RenderObjectPool::acquireNode(const RenderStashKey& key)
{
    if (key.empty())
    {
        return nullptr;
    }

    auto it = m_nodes.find(key);
    if (it == m_nodes.end())
    {
        return nullptr;
    }

    ax::Node* node = it->second;
    m_nodes.erase(it);

    // 放入 m_nodesToRelease 延迟调用 release，避免 acquire 后立即 release 导致 node 被销毁。
    m_nodesToRelease.push_back(node);

    return node;
}

void RenderObjectPool::recycleCamera(const RenderStashKey& key, std::unique_ptr<VirtualCamera> cam)
{
    if (key.empty() || !cam)
    {
        return;
    }

    m_cameras[key] = std::move(cam);
}

std::unique_ptr<VirtualCamera> RenderObjectPool::acquireCamera(const RenderStashKey& key)
{
    if (key.empty())
    {
        return nullptr;
    }

    auto it = m_cameras.find(key);
    if (it == m_cameras.end())
    {
        return nullptr;
    }

    std::unique_ptr<VirtualCamera> cam = std::move(it->second);
    m_cameras.erase(it);
    return cam;
}

void RenderObjectPool::clear()
{
    for (auto& [key, node] : m_nodes)
    {
        if (node)
        {
            node->removeFromParent();
            node->release();
        }
    }
    m_nodes.clear();
    m_cameras.clear();

    for (ax::Node* node : m_nodesToRelease)
    {
        if (node)
        {
            node->release();
        }
    }
    m_nodesToRelease.clear();
}

void RenderObjectPool::clear(EntityId id)
{
    for (auto it = m_nodes.begin(); it != m_nodes.end();)
    {
        if (it->first.entityId() == id)
        {
            if (it->second)
            {
                it->second->removeFromParent();
                it->second->release();
            }
            it = m_nodes.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = m_cameras.begin(); it != m_cameras.end();)
    {
        if (it->first.entityId() == id)
        {
            it = m_cameras.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void RenderObjectPool::update(float /*dt*/)
{
#    if _DEBUG
    for (const auto& [key, node] : m_nodes)
    {
        if (node && node->getReferenceCount() == 1)
        {
            MG_LOG_E("RenderObjectPool: pooled node only held by pool, entity={} tag={}", key.entityId(), key.tag());
        }
    }
#    endif

    if (!m_nodesToRelease.empty())
    {
        for (ax::Node* node : m_nodesToRelease)
        {
            if (node)
            {
                node->release();
            }
        }
        m_nodesToRelease.clear();
    }
}

#endif  // RUNTIME_IN_AXMOL

NS_MG_END
