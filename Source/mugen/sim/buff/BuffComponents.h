#pragma once
#include "mugen/core/reflect/Reflect.h"
#include <cstdint>
#include <vector>

namespace mugen::sim
{
// 常驻 op 种类（DESIGN §5.4；S1-B3 只实现 Flag，其余 op 留到 S2）。
enum class BuffOpKind : uint8_t
{
    None = 0,
    // StatusFlags 引用计数 +1（霸体、无敌、眩晕等限制状态）。
    Flag = 1,
};

// 单个常驻 op：激活时应用，失活时按来源撤销。
struct BuffOp
{
    BuffOpKind m_kind = BuffOpKind::None;
    // Flag op 的目标限制状态（mugen::ExtraState 数值）。
    int32_t m_flag = 0;
    MG_REFLECT(BuffOp, m_kind, m_flag)
};

// 单个 Buff 实例。
struct BuffInstance
{
    // 实例序号。
    uint32_t m_serial = 0;
    // 当前配置标识。
    int32_t m_id = 0;
    // 首次准入的配置标识。
    int32_t m_initialId = 0;
    // 常驻 op 门闩：重复激活不重复叠加（DESIGN §5.1 latched）。
    bool m_latched = false;
    // 已请求移除（下一帧 BuffTick 发出 Removed 并回收）。
    bool m_removing = false;
    // 常驻 op 列表。
    std::vector<BuffOp> m_persistent;
    MG_REFLECT(BuffInstance, m_serial, m_id, m_initialId, m_latched, m_removing, m_persistent)
};

// 角色持有的 Buff 列表。
struct BuffList
{
    // 全部活动实例。
    std::vector<BuffInstance> m_instances;
    // 下一个实例序号。
    uint32_t m_nextSerial = 1;
    MG_REFLECT(BuffList, m_instances, m_nextSerial)
};
}  // namespace mugen::sim
