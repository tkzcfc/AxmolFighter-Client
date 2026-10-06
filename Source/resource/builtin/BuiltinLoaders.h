#pragma once

namespace gameres
{
class ResourceLoaderRegistry;

// 注册所有内置资源类型（Texture / SpriteFrames / Spine / FguiPackage / Audio / Config / Map）。
// 由 ResourceLoaderRegistry::getInstance() 首次调用时自动注册，不需要手动调用。
void registerBuiltinLoaders(ResourceLoaderRegistry& registry);

}  // namespace gameres
