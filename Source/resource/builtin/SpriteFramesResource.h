#pragma once

#include "resource/Resource.h"

#include <string>

namespace gameres
{

// 预热一个 plist 精灵图集（ax::SpriteFrameCache::addSpriteFramesWithFile）。
// 纹理路径为空时按引擎规则推断：优先 plist 里的 metadata.textureFileName，否则取同名 .png。
// 纹理作为子资源（TextureResource）加载，由 ResourceLoader 持有。
class SpriteFramesResource : public Resource
{
public:
    explicit SpriteFramesResource(std::string plist, std::string texture = {})
        : Resource(ResourceType::SpriteFrames), m_plist(std::move(plist)), m_texture(std::move(texture))
    {}

    std::string getKey() const override { return m_plist; }

    const std::string& getPlist() const { return m_plist; }
    const std::string& getTexturePath() const { return m_texture; }

private:
    std::string m_plist;
    std::string m_texture;
};

}  // namespace gameres
