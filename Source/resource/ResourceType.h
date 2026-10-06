#pragma once

#include <cstdint>
#include <string>

namespace gameres
{

// 资源类型。内置类型见下；自定义类型从 CustomBegin 往后编号，
// 配合 ResourceLoaderRegistry::registerType<T>() 注册加载函数。
enum class ResourceType : uint16_t
{
    Texture = 0,
    SpriteFrames,
    Spine,
    FguiPackage,
    Audio,
    Config,
    Map,

    CustomBegin = 100,
};

std::string toString(ResourceType type);

}  // namespace gameres
