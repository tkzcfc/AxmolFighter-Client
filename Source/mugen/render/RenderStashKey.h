#pragma once

#include "mugen/core/ecs/Types.h"

#include <functional>
#include <string>
#include <string_view>
#include <type_traits>

NS_MG_BEGIN

// 渲染暂存键：EntityId + 流式业务 tag（如 mapId / 资源路径）。
// 用作 RenderObjectPool 的 map key；内容变化时需把区分信息编进 tag，避免错误复用。
//
// 用法：
//   pool->recycleNode(RenderStashKey(entityId) << "mapRoot" << mapId, node);
class RenderStashKey
{
public:
    struct Hash
    {
        std::size_t operator()(const RenderStashKey& key) const noexcept
        {
            return (static_cast<std::size_t>(key.m_entityId) * 1315423911u) ^ std::hash<std::string>{}(key.m_tag);
        }
    };

    explicit RenderStashKey(EntityId entityId) : m_entityId(entityId) {}

    EntityId entityId() const { return m_entityId; }
    const std::string& tag() const { return m_tag; }
    bool empty() const { return m_entityId == INVALID_ENTITY_ID || m_tag.empty(); }

    RenderStashKey& add(std::string_view part)
    {
        appendPart(part);
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>>>
    RenderStashKey& add(T value)
    {
        appendPart(std::to_string(value));
        return *this;
    }

    RenderStashKey& add(bool value)
    {
        appendPart(value ? "1" : "0");
        return *this;
    }

    RenderStashKey& operator<<(std::string_view value) { return add(value); }
    RenderStashKey& operator<<(const std::string& value) { return add(std::string_view(value)); }
    RenderStashKey& operator<<(const char* value) { return add(value ? std::string_view(value) : std::string_view{}); }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    RenderStashKey& operator<<(T value)
    {
        return add(value);
    }

    bool operator==(const RenderStashKey& other) const
    {
        return m_entityId == other.m_entityId && m_tag == other.m_tag;
    }

    bool operator!=(const RenderStashKey& other) const { return !(*this == other); }

private:
    void appendPart(std::string_view part)
    {
        if (part.empty())
        {
            return;
        }
        if (!m_tag.empty())
        {
            m_tag.push_back('|');
        }
        m_tag.append(part.data(), part.size());
    }

    EntityId m_entityId = INVALID_ENTITY_ID;
    std::string m_tag;
};

NS_MG_END
