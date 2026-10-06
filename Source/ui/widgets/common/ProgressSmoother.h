#pragma once

namespace gameui
{

// 把"真实进度"转换成"让人感觉一直在走"的显示进度，不依赖引擎，方便单元测试。
//
//   - 追赶阶段：显示进度落后于真实进度时，按指数缓动追赶（由快到慢），
//     另设一个很低的最低速度，保证总能追上，不会无限接近却到不了。
//   - 爬行阶段：追上之后真实进度不动（比如卡在某个耗时任务上），显示进度仍然
//     以越来越慢的速度继续往前挪一点，上限是 `target + (kCreepCeiling - target) * kCreepShare`，
//     且不超过 kCreepCeiling，这样既让玩家感觉在动，又不会爬得太多。
//   - 收尾阶段：真实加载全部完成后（调用方传入 finished=true），以较快的匀速冲到 100%。
//
// 显示进度只增不减。
class ProgressSmoother
{
public:
    // target: 当前真实进度，会被截断到 [0, 1]；finished: 真实加载是否已经全部结束。
    // 一旦传入过 finished=true，之后即便再传 false 也会保持收尾状态（不应该发生，但做个保护）。
    void update(float dt, float target, bool finished);

    float getDisplay() const { return m_display; }
    // 真实加载已完成，且显示进度也已经走到 100%
    bool isDone() const { return m_finished && m_display >= 1.0f; }

private:
    float m_display = 0.0f;
    bool m_finished = false;

    // 追赶阶段：每秒追上剩余距离的比例（指数缓动系数）
    static constexpr float kCatchUpRate = 6.0f;
    // 追赶阶段的最低速度（进度/秒），避免无限趋近但追不上
    static constexpr float kMinCatchUpSpeed = 0.12f;
    // 爬行阶段：每秒爬完剩余爬行空间的比例
    static constexpr float kCreepRate = 0.6f;
    // 爬行上限（绝对值），显示进度在完成前永远不会超过它
    static constexpr float kCreepCeiling = 0.95f;
    // 爬行上限相对真实进度的占比：ceiling = target + (kCreepCeiling - target) * kCreepShare
    static constexpr float kCreepShare = 0.3f;
    // 收尾阶段的速度（进度/秒）；1.0f / kFinishSpeed 约等于冲满全程所需的秒数
    static constexpr float kFinishSpeed = 2.0f;  // 约 0.5 秒冲到 100%
};

}  // namespace gameui
