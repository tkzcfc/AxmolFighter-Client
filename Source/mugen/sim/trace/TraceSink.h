#pragma once

#include "mugen/core/ecs/Entity.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace mugen::sim
{
class World;
}

namespace mugen::sim::trace
{
// 语义追踪输出：逐行记录可对比的战斗行为，供后续重构（角色 BT 转 FSM、技能模型、AI）做行为等价性验证。
// 该对象由驱动拥有，世界仅持有非拥有原始指针；不参与快照/哈希/克隆，挂载为空时零开销。
class TraceSink
{
public:
    // 追加一行文本并自动补齐换行。
    void write(std::string_view line);
    // 返回累积的全部文本。
    const std::string& text() const { return m_text; }
    // 清空累积文本。
    void clear() { m_text.clear(); }

private:
    std::string m_text;
};
// 追踪记录的临时视图，不存入组件、不拥有输出缓冲，生命周期限于单次调用。
class TraceRecorder
{
public:
    // 构造指向指定世界实例的追踪视图。
    explicit TraceRecorder(World& world);
    // 在一帧结束时输出帧边界、各实体状态行与表现事件行。
    void emitFrame();
    // 事实发生当下追加一行，不改变逐帧状态行。未挂载追踪时直接返回。
    // cast <实体> <技能配置> <槽名> <管线>
    void noteCast(Entity entity, int32_t configId, std::string_view slot, int32_t pipe);
    // castend <实体> <技能配置>
    void noteCastEnd(Entity entity, int32_t configId);
    // dmg <攻击者> <目标> <命中来源配置> <伤害> <暴击 0/1>
    void noteDamage(Entity attacker, Entity target, int32_t sourceConfigId, int64_t value, bool critical);
    // buff + <实体> <效果配置>
    void noteBuffAdd(Entity entity, int32_t buffId);
    // buff - <实体> <效果配置>
    void noteBuffRemove(Entity entity, int32_t buffId);
    // spawn <主人> <特效配置>
    void noteSpawn(Entity owner, int32_t effectConfigId);
    // death <实体>
    void noteDeath(Entity entity);

private:
    // 未挂载追踪时不拼接文本。
    bool _tracing() const;
    // 向已挂载的追踪追加一行。
    void _writeEvent(const std::string& line);

    // 本场战斗世界。
    World& m_world;
};
}  // namespace mugen::sim::trace
