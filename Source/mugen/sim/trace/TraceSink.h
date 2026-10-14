#pragma once

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
// 在一帧结束时输出帧边界、各实体状态行与表现事件行。
void emitFrame(TraceSink& sink, World& world);
}  // namespace mugen::sim::trace
