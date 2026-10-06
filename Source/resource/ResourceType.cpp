#include "resource/ResourceType.h"

namespace gameres
{

std::string toString(ResourceType type)
{
    switch (type)
    {
    case ResourceType::Texture:
        return "Texture";
    case ResourceType::SpriteFrames:
        return "SpriteFrames";
    case ResourceType::Spine:
        return "Spine";
    case ResourceType::FguiPackage:
        return "FguiPackage";
    case ResourceType::Audio:
        return "Audio";
    case ResourceType::Config:
        return "Config";
    default:
        return "Custom#" + std::to_string(static_cast<uint16_t>(type));
    }
}

}  // namespace gameres
