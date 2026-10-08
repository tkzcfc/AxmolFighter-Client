#pragma once

#include <cstdint>
#include <memory>

namespace gameres
{

class ResourceLoader;
class SpineResource;

// 按角色 id 预热身体 Spine。路径和缩放来自 mugen::AvatarPaths::resolveRoleSpine
// （role.resSpineId → res_spine），和运行时查询 SpineSkeletonCache 用同一个 key。
//
// roleId 不存在或没有配置 Spine 时不添加任何资源，返回空指针。
std::shared_ptr<SpineResource> addRoleSpine(ResourceLoader& loader, int32_t roleId);

}  // namespace gameres
