#pragma once

#include "resource/Resource.h"

#include "mugen/render/spine/MgSkeletonData.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gameres
{

// 预热一份 Spine 骨架数据到 mugen::SpineSkeletonCache。
//
// 注意：skel + atlas 列表 + scale 必须和之后查询（getOrCreate/createFromCache 等）时
// 完全一致，否则缓存 key 不同会导致预热落空，详见 SpineSkeletonCache::makeKey。
class SpineResource : public Resource
{
public:
    SpineResource(std::string skeletonFile, std::vector<std::string> atlasFiles = {}, float scale = 1.0f)
        : Resource(ResourceType::Spine)
        , m_skeletonFile(std::move(skeletonFile))
        , m_atlasFiles(std::move(atlasFiles))
        , m_scale(scale)
    {}

    explicit SpineResource(int32_t resSpineId) : Resource(ResourceType::Spine), m_resSpineId(resSpineId) {}

    std::string getKey() const override
    {
        if (m_resSpineId > 0)
            return "res:" + std::to_string(m_resSpineId);

        std::string key = m_skeletonFile;
        for (const auto& atlas : m_atlasFiles)
            key += "|" + atlas;
        key += "|" + std::to_string(m_scale);
        return key;
    }

    // resSpineId <= 0 视为无效（0 是"没有配置传送门/效果 Spine"的常见哨兵值），按骨架路径处理
    bool isByResId() const { return m_resSpineId > 0; }
    int32_t getResSpineId() const { return m_resSpineId; }
    const std::string& getSkeletonFile() const { return m_skeletonFile; }
    const std::vector<std::string>& getAtlasFiles() const { return m_atlasFiles; }
    float getScale() const { return m_scale; }

    mugen::MgSkeletonDataPtr getData() const { return m_data; }
    void setData(mugen::MgSkeletonDataPtr data) { m_data = std::move(data); }

    void releaseHold() override { m_data.reset(); }

private:
    std::string m_skeletonFile;
    std::vector<std::string> m_atlasFiles;
    float m_scale        = 1.0f;
    int32_t m_resSpineId = -1;
    mugen::MgSkeletonDataPtr m_data;
};

}  // namespace gameres
