#pragma once

#include "resource/Resource.h"

#include <string>

namespace gameres
{

// 预热一个音频文件（ax::AudioEngine::preload）。
class AudioResource : public Resource
{
public:
    explicit AudioResource(std::string path) : Resource(ResourceType::Audio), m_path(std::move(path)) {}

    std::string getKey() const override { return m_path; }
    const std::string& getPath() const { return m_path; }

private:
    std::string m_path;
};

}  // namespace gameres
