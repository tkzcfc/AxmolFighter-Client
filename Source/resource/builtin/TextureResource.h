#pragma once

#include "resource/Resource.h"

#include "renderer/Texture2D.h"
#include "base/RefPtr.h"

#include <string>

namespace gameres
{

// 预热一张纹理到引擎的 TextureCache（见 ax::TextureCache::addImageAsync）。
class TextureResource : public Resource
{
public:
    explicit TextureResource(std::string path) : Resource(ResourceType::Texture), m_path(std::move(path)) {}

    std::string getKey() const override { return m_path; }

    const std::string& getPath() const { return m_path; }

    void setTexture(ax::Texture2D* texture) { m_texture = texture; }
    ax::Texture2D* getTexture() const { return m_texture; }

    void releaseHold() override { m_texture.reset(); }

private:
    std::string m_path;
    ax::RefPtr<ax::Texture2D> m_texture;
};

}  // namespace gameres
