#pragma once

#include "mugen/sim/bt/RoleTree.h"
#include "mugen/sim/role/brain/RoleContext.h"

#include <type_traits>
#include <utility>

namespace mugen::sim::behavior::bridge
{
namespace detail
{
// 检测结构体是否提供 onDisplacement 回调。
template <class T, class = void>
struct has_displacement : std::false_type {};
template <class T>
struct has_displacement<T, std::void_t<decltype(T::onDisplacement(std::declval<role::brain::RoleContext&>(),
                                                                  std::declval<typename T::Param>(),
                                                                  std::declval<typename T::State&>()))>> : std::true_type
{};
}  // namespace detail

// 临时桥接，阶段 1.3 删除。
template <class T>
BTLeafCallbacks<BTCtx, Param, LeafState> leafCallbacks()
{
    BTLeafCallbacks<BTCtx, Param, LeafState> result;
    result.onEnter = [](BTCtx& ctx, const Param& param, LeafState& state) {
        // 首进时叶子状态还是 monostate，先置为对应类型；后续进入保留旧状态以支持静态计时继承。
        if (!std::holds_alternative<typename T::State>(state))
            state = typename T::State{};
        role::brain::RoleContext rc{ctx.world, ctx.entity};
        rc.setNode(ctx.m_node);
        T::enter(rc, std::get<typename T::Param>(param), std::get<typename T::State>(state));
    };
    result.onUpdate = [](BTCtx& ctx, const Param& param, LeafState& state) {
        role::brain::RoleContext rc{ctx.world, ctx.entity};
        rc.setNode(ctx.m_node);
        return T::update(rc, std::get<typename T::Param>(param), std::get<typename T::State>(state));
    };
    result.onExit = [](BTCtx& ctx, const Param& param, LeafState& state) {
        role::brain::RoleContext rc{ctx.world, ctx.entity};
        rc.setNode(ctx.m_node);
        T::exit(rc, std::get<typename T::Param>(param), std::get<typename T::State>(state));
    };
    if constexpr (detail::has_displacement<T>::value)
    {
        result.onDisplacementEvent = [](BTCtx& ctx, const Param& param, LeafState& state) {
            role::brain::RoleContext rc{ctx.world, ctx.entity};
            rc.setNode(ctx.m_node);
            rc.setEvent(ctx.event);
            return T::onDisplacement(rc, std::get<typename T::Param>(param), std::get<typename T::State>(state));
        };
    }
    return result;
}
}  // namespace mugen::sim::behavior::bridge
