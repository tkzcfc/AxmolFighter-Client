#pragma once

#include "mugen/conf/GameDef.h"
#include "mugen/core/ecs/Component.h"
#include <array>

NS_MG_BEGIN

// 输入组件
class InputComponent : public Component
{
public:
    typedef Component Super;

public:
    InputComponent() : lastKeyDown(0), keyDown(0), keyPressedDurationMs{}
    {
        keyLastDownTimestampMs.fill(0);
        keyLastUpTimestampMs.fill(0);
    }
    virtual ~InputComponent() {}

    // 查询输入位持续按下的时长（毫秒），slotIndex 为输入位偏移 index，范围 [1, INPUT_SLOT_MAX)。
    inline int64_t queryKeyPressedDurationMs(int32_t slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= INPUT_SLOT_MAX)
        {
            MG_ASSERT(false);
            return 0;
        }
        return keyPressedDurationMs[slotIndex];
    }

    // 查询输入位最后一次按下的时间戳，slotIndex 为输入位偏移 index，范围 [1, INPUT_SLOT_MAX)。
    inline int64_t queryKeyLastDownTimestampMs(int32_t slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= INPUT_SLOT_MAX)
        {
            MG_ASSERT(false);
            return 0;
        }
        return keyLastDownTimestampMs[slotIndex];
    }

    // 查询输入位是否按下，slotIndex 为输入位偏移 index，范围 [1, INPUT_SLOT_MAX)。
    inline bool isKeyDown(int32_t slotIndex) const { return MG_BIT_HAS_ANY(keyDown, 1 << slotIndex); }

    // 查询输入位是否在上一帧按下，slotIndex 为输入位偏移 index，范围 [1, INPUT_SLOT_MAX)。
    inline bool isLastKeyDown(int32_t slotIndex) const { return MG_BIT_HAS_ANY(lastKeyDown, 1 << slotIndex); }

public:
    // 上一帧按下的按键状态，使用位掩码表示，1 << 输入位偏移 index。
    uint32_t lastKeyDown;
    // 当前帧所有按键按下状态，使用位掩码表示，1 << 输入位偏移 index。
    uint32_t keyDown;
    // 每个输入位的按下持续时长（毫秒），索引即输入位偏移 index。
    std::array<int64_t, INPUT_SLOT_MAX> keyPressedDurationMs;
    // 存放每个输入为最后一次按下的时间戳（毫秒），索引即输入位偏移 index。
    std::array<int64_t, INPUT_SLOT_MAX> keyLastDownTimestampMs;
    // 存放每个输入位最后一次抬起的时间戳（毫秒），用于双击检测。索引即输入位偏移 index。
    std::array<int64_t, INPUT_SLOT_MAX> keyLastUpTimestampMs;

    MG_DEFINE_SERIALIZABLE(lastKeyDown, keyDown, keyPressedDurationMs, keyLastDownTimestampMs, keyLastUpTimestampMs);
};

NS_MG_END
