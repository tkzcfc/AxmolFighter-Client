#pragma once

#include "mugen/core/StdC.h"
#include "mugen/sim_legacy/World.h"

#ifdef RUNTIME_IN_AXMOL

#    include <array>
#    include <random>
#    include <string>
#    include <vector>

NS_MG_BEGIN

// 伤害飘字：位图字体标签池，数字外观与动画时序见 .cpp。
class HurtNumber
{
public:
    explicit HurtNumber(ax::Node& root) : m_root(root) {}
    // 消费伤害数值与闪避提示事件，创建飘字。
    void consume(const sim_legacy::PresentationEvent& event, sim_legacy::World& world);
    // 按渲染帧推进全部飘字动画。
    void update(float delta);
    // 摘除全部飘字与池内标签。
    void clear();

private:
    // 飘字使用的位图字体种类，同时是标签池下标。
    enum class Face : int32_t
    {
        // 玩家方打出的普通伤害。
        kNormal = 0,
        // 敌方打到玩家方的伤害。
        kHero,
        // 暴击伤害。
        kCrit,
        // 未命中/闪避文字。
        kWord,
        kCount
    };
    // 一条正在播放的飘字。
    struct Item
    {
        ax::Label* label = nullptr;
        Face face        = Face::kNormal;
        // 飘字锚点（世界坐标），动画在其上叠加上升位移。
        float x = 0.0f;
        float y = 0.0f;
        // 已执行的动画步数与当前步内累计毫秒。
        int step       = 0;
        float carryMs  = 0.0f;
        // 当前不透明度（0-255）。
        int alpha = 255;
    };

    // 在锚点创建一条飘字；text 为数字串，Face::kWord 时为文字字符。
    void spawn(Face face, const std::string& text, float x, float y);
    // 执行第 item.step 步动画。
    static void applyStep(Item& item);
    // 取一个同字体的空闲标签，没有则新建。
    ax::Label* acquireLabel(Face face);
    // 回收标签到池中。
    void recycle(Item& item);
    // 随机偏移，范围 [-30, 30] 的整数。
    float randomOffset();

    ax::Node& m_root;
    std::vector<Item> m_active;
    std::array<std::vector<ax::Label*>, static_cast<size_t>(Face::kCount)> m_pool;
    std::minstd_rand m_random{0x7A11u};
};

NS_MG_END

#endif
