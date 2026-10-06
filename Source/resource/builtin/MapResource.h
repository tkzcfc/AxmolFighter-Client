#pragma once

#include "resource/Resource.h"

#include <string>

namespace gameres
{

// 预热一个 .layer 地图引用的全部资源：Spine 骨架、纹理、SpriteFrames 图集、背景音乐。
// 只解析 JSON、不创建任何节点；解析逻辑见 mugen::LayerRuntimeLoader::collectResources，
// 和真正建节点的 mugen::LayerRuntimeLoader::loadNode 共用同一套字段，保证不会读漏。
// Spine 的加载缩放固定为 1.0（和 .layer 里的用法一致，节点自身的 scale 只作用于渲染节点）。
class MapResource : public Resource
{
public:
    explicit MapResource(std::string layerFile) : Resource(ResourceType::Map), m_layerFile(std::move(layerFile)) {}

    std::string getKey() const override { return m_layerFile; }
    const std::string& getLayerFile() const { return m_layerFile; }

private:
    std::string m_layerFile;
};

}  // namespace gameres
