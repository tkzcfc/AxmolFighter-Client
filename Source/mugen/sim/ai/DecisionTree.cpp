#include "DecisionTree.h"
#include <array>

namespace mugen::sim::ai
{
namespace
{
// Entity 导出树使用的节点类型。
enum class Kind : uint8_t { Selector, Sequence, IfElse, Condition, Method, Assignment };
// 只读节点定义；数组下标就是 XML 节点编号。
struct Node
{
    // 节点分派类型。
    Kind m_kind = Kind::Method;
    // 有序直接子节点。
    std::array<uint8_t, 4> m_children{};
    // 有效子节点数。
    uint8_t m_count = 0;
    // 条件或动作调用的方法。
    Method m_method = Method::CheckAttack;
    // 动作的返回值覆盖；Invalid 表示直接使用方法返回值。
    Result m_resultOption = Result::Invalid;
};
// 编译期手工拓扑；不解析 XML，也不链接外部决策运行库。
constexpr auto entityNodes()
{
    std::array<Node, 37> nodes{};
    // XML 0：警戒分支优先于未警戒分支。
    nodes[0] = {Kind::Selector, {10, 31}, 2};
    // XML 10/3/13：攻击、等待结束，再选择警戒游走或追击。
    nodes[10] = {Kind::Sequence, {3, 13}, 2};
    nodes[3] = {Kind::Sequence, {4, 5}, 2};
    nodes[13] = {Kind::IfElse, {7, 29, 2}, 3};
    // XML 31/8/32/36/17：受击、发现目标、巡逻三条路径。
    nodes[31] = {Kind::IfElse, {11, 8, 17}, 3};
    nodes[8] = {Kind::IfElse, {18, 32, 36}, 3};
    nodes[32] = {Kind::Sequence, {24, 25, 26, 27}, 4};
    nodes[36] = {Kind::Sequence, {20, 21, 22}, 3};
    nodes[17] = {Kind::Sequence, {1, 14, 15, 16}, 4};
    // XML 34 是自定义附件，7/11/18 是普通条件；均比较方法返回值是否为 Failure。
    nodes[34] = {Kind::Condition, {}, 0, Method::CheckAttack};
    nodes[7] = {Kind::Condition, {}, 0, Method::CheckChase};
    nodes[11] = {Kind::Condition, {}, 0, Method::CheckHit};
    nodes[18] = {Kind::Condition, {}, 0, Method::CheckTarget};
    // XML 动作节点均使用 ResultOption=Invalid。
    nodes[4].m_method = Method::ExecuteAttack;
    nodes[5].m_method = Method::CheckAttackEnd;
    nodes[29].m_method = Method::ExecuteAlert;
    nodes[2].m_method = Method::ExecuteChase;
    nodes[24].m_method = Method::ExecutePatrol;
    nodes[25].m_method = Method::CheckMoveEnd;
    nodes[26].m_method = Method::ExecuteRandAttack;
    nodes[27].m_method = Method::CheckAttackEnd;
    nodes[20].m_method = Method::ExecuteFirstAttack;
    nodes[21].m_method = Method::CheckAttackEnd;
    nodes[1].m_method = Method::CheckHitEnd;
    nodes[14].m_method = Method::ExecuteFirstAttack;
    nodes[15].m_method = Method::CheckAttackEnd;
    // XML 22/16：只赋值 Alert；没有重置 NoAlert 的节点。
    nodes[22].m_kind = Kind::Assignment;
    nodes[16].m_kind = Kind::Assignment;
    return nodes;
}
// 全局仅共享不可变拓扑。
constexpr auto kNodes = entityNodes();
}

Result tickEntityTree(DecisionState& state, void* context, InvokeMethod invoke)
{
    if (state.m_stack.empty())
        state.m_stack.push_back({0});
    // Running 表示分派当前帧；终态表示刚完成的子节点结果。
    Result result = Result::Running;
    while (!state.m_stack.empty())
    {
        auto& frame = state.m_stack.back();
        const auto& node = kNodes[frame.m_node];
        if (!frame.m_entered)
        {
            // XML 附件 12/9 的 Phase=Enter：恢复运行叶子时不重查。
            const bool rejected = (frame.m_node == 10 && state.m_alert != AlertStatus::Alert) ||
                                  (frame.m_node == 31 && state.m_alert != AlertStatus::NoAlert);
            frame.m_entered = true;
            if (rejected)
            {
                state.m_stack.pop_back();
                result = Result::Failure;
                continue;
            }
        }
        if (node.m_kind == Kind::Method)
        {
            const auto returned = invoke(context, node.m_method);
            result = node.m_resultOption == Result::Invalid ? returned : node.m_resultOption;
            if (result == Result::Running)
                return result;
        }
        else if (node.m_kind == Kind::Condition)
            result = invoke(context, node.m_method) == Result::Failure ? Result::Success : Result::Failure;
        else if (node.m_kind == Kind::Assignment)
        {
            state.m_alert = AlertStatus::Alert;
            result = Result::Success;
        }
        else
        {
            bool dispatch = result == Result::Running;
            if (!dispatch)
            {
                if (node.m_kind == Kind::IfElse && frame.m_child == 0)
                {
                    frame.m_child = result == Result::Success ? 1 : 2;
                    dispatch = true;
                }
                else if ((node.m_kind == Kind::Sequence && result == Result::Success) ||
                         (node.m_kind == Kind::Selector && result == Result::Failure))
                    dispatch = ++frame.m_child < node.m_count;
            }
            if (dispatch)
            {
                // XML custom 34：只在 Sequence(10) 分派子节点前检查，不抢占缓存叶子。
                if (frame.m_node == 10 && invoke(context, kNodes[34].m_method) == Result::Failure)
                    result = Result::Failure;
                else
                {
                    const auto child = node.m_children[frame.m_child];
                    state.m_stack.push_back({child});
                    result = Result::Running;
                    continue;
                }
            }
        }
        state.m_stack.pop_back();
    }
    return result;
}
} // namespace mugen::sim::ai
