#pragma once

#include "mugen/core/ecs/Types.h"

#include <cstdint>
#include <deque>
#include <string>

namespace mugen
{
class GameWord;
}

namespace gameui
{

// 客户端战斗同步：固定帧本地预测 + 全量 snapshot 和解。
// 只负责世界推进与输入历史，不直接做网络收发。
class BattleNetSync
{
public:
    static constexpr float FIXED_DT                = 1.0f / 30.0f;
    static constexpr std::size_t MAX_INPUT_HISTORY = 120;

    struct PendingInput
    {
        std::uint32_t frame = 0;
        std::uint32_t mask  = 0;
    };

    void start(mugen::GameWord* world, std::uint32_t battleId, std::uint32_t serverFrame, std::uint32_t localActorId);
    void stop();

    void setLocalInputMask(std::uint32_t mask);
    void update(float delta);

    // 应用服务器快照；worldDump 为 GameWord 序列化数据。
    void applySnapshot(const std::string& worldDump, std::uint32_t serverFrame, std::uint32_t lastProcessedClientFrame);

    std::uint32_t battleId() const { return m_battleId; }
    std::uint32_t localActorId() const { return m_localActorId; }
    std::uint32_t clientFrame() const { return m_clientFrame; }

    // 取走一个待发送的输入帧；返回 false 表示没有新输入。
    bool consumePendingInput(PendingInput& out);

private:
    struct InputCmd
    {
        std::uint32_t frame = 0;
        std::uint32_t mask  = 0;
    };

    void predictStep();

private:
    mugen::GameWord* m_world        = nullptr;
    std::uint32_t m_battleId        = 0;
    std::uint32_t m_localActorId    = 0;
    std::uint32_t m_clientFrame     = 0;
    std::uint32_t m_lastServerFrame = 0;
    std::uint32_t m_localInputMask  = 0;
    float m_accumulator             = 0.0f;
    std::deque<InputCmd> m_inputHistory;
    std::deque<PendingInput> m_pendingInputs;
    bool m_running = false;
};

}  // namespace gameui
