#pragma once

#include <cstdint>
#include <memory>

namespace gameres
{

class ResourceLoader;
class SpineResource;

// 按角色 id 预热其身体 Spine，和 mugen::actor_spawner::resolveRoleSpine 共用同一套
// 路径/缩放规则（城镇 *_city 覆盖、/hero/ 默认缩放 0.25），确保预加载和运行时查询
// SpineSkeletonCache 用的是同一个 key。
//
// preferCity 应该和 GameWord 的模式一致：城镇传 true，副本/战斗传 false。
// roleId 不存在或没有配置 Spine 时不添加任何资源，返回空指针。
std::shared_ptr<SpineResource> addRoleSpine(ResourceLoader& loader, int32_t roleId, bool preferCity);

}  // namespace gameres
