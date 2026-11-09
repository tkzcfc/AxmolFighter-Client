#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf::battle
{
// 时间轴 cue 的种类，与 §6.2 的 cue 表一一对应。
enum class ClipCueKind : int32_t
{
    None = 0,
    SpawnEffect = 1,  // 生成特效（effectIds / effectFrames）
    CameraShake = 2,  // 震屏（cameraId / cameraFrame）
    Sound = 3,        // 动作音效（soundId，第 0 帧）
    Ghost = 4,        // 残影开关（ghost）
    Tips = 5,         // 提示开关（tipsShow）
    Dialog = 6,       // 对话开关（dialogShow）
    NameShow = 7,     // 名字显示开关（nameShow）
    DisplaySpine = 8, // 演出 Spine（displaySpineIds / displaySpineFrame / displaySpineFrameCount）
    TimeStop = 9,     // 定身（staticStartFrame / staticTarget / staticTime / staticResetTime）
    Transform = 10,   // 变身（transformFrame / transformId / transformType）
    Summon = 11,      // 召唤（summonFrame / summonId / summonTime）
};

// 一段按帧排序的时间轴 cue。所有载荷字段按 kind 使用，未用的保持默认。
struct ClipCue
{
    // 触发帧（逻辑帧）。
    int32_t frame = 0;
    ClipCueKind kind = ClipCueKind::None;
    // SpawnEffect：特效 id。
    int32_t effectId = 0;
    // SpawnEffect：自定义方向（customVec）。
    std::vector<int32_t> customVec;
    // SpawnEffect：是否继承角色方向（extendRoleVec）。
    int32_t extendRoleVec = 0;
    // CameraShake：镜头配置 id。
    int32_t cameraId = 0;
    // Sound：音效 id 列表。
    std::vector<int32_t> soundIds;
    // Ghost / Tips / Dialog / NameShow：开关值或提示/对话 id。
    int32_t value = 0;
    // DisplaySpine：分组（displaySpineIds）。
    std::vector<std::vector<int32_t>> displaySpineGroups;
    int32_t displaySpineFrameCount = 0;
    // TimeStop：目标 / 时长 / 复位时间（staticTarget / staticTime / staticResetTime）。
    int32_t staticTarget = 0;
    int32_t staticTime = 0;
    int32_t staticResetTime = 0;
    // Transform：目标角色 id / 类型。
    int32_t transformId = 0;
    int32_t transformType = 0;
    // Summon：召唤角色 id / 存活时间（毫秒）。
    int32_t summonId = 0;
    int64_t summonTime = 0;

    MG_REFLECT(ClipCue,
               frame,
               kind,
               effectId,
               customVec,
               extendRoleVec,
               cameraId,
               soundIds,
               value,
               displaySpineGroups,
               displaySpineFrameCount,
               staticTarget,
               staticTime,
               staticResetTime,
               transformId,
               transformType,
               summonId,
               summonTime)
};

// 撞墙策略（由 obstruct 派生）。
enum class ClipWallPolicy : int32_t
{
    Default = 0,    // 默认贴墙
    Pass = 1,       // 穿过（obstruct 0）
    StopAtWall = 2, // 停在墙边（obstruct -1）
    EndClip = 3,    // 结束片段并记录阻挡（obstruct 1）
};

// 落地策略（由 floor 派生）。
enum class ClipFloorPolicy : int32_t
{
    Default = 0, // 默认（落地反弹）
    EndClip = 1, // 落地结束片段（floor 0）
};

// 片段控制方式（由 control 派生）。
enum class ClipControlMode : int32_t
{
    None = -1,        // 无控制
    FreeMove = 0,     // 自由移动（带控制速度）
    SteerAtEnter = 1, // 进入时按摇杆设方向与朝向
    TurnOnly = 2,     // 只改朝向
    MoveNoTurn = 3,   // 可移动不改朝向
};

// 位移策略。
struct ClipDisplacementPolicy
{
    ClipWallPolicy wall = ClipWallPolicy::Default;
    ClipFloorPolicy floor = ClipFloorPolicy::Default;
    // brake 时结束片段（loops = -1 的动作）。
    int32_t endOnBrake = 0;

    MG_REFLECT(ClipDisplacementPolicy, wall, floor, endOnBrake)
};

// 片段控制。
struct ClipControl
{
    ClipControlMode mode = ClipControlMode::None;
    Fixed velocity; // controlVelocity

    MG_REFLECT(ClipControl, mode, velocity)
};

// 片段进入时的瞬移放置。
struct ClipPlacement
{
    int32_t posType = 0;               // actionPosType
    std::vector<Fixed> relativePos;    // relativeActionPos
    int32_t orientation = 0;           // actionOrientation

    MG_REFLECT(ClipPlacement, posType, relativePos, orientation)
};

// 取消窗口。
struct ClipCancel
{
    // 普通窗口帧（interruptFrame，-1 表示没有窗口）。
    int32_t normalFrame = -1;
    // 至尊窗口帧（interruptExtraFrame，-1 表示没有窗口）。
    int32_t extraFrame = -1;

    MG_REFLECT(ClipCancel, normalFrame, extraFrame)
};

// 角色动作片段，由 action.lua 降级（见 DESIGN.md §6.2）。
struct ClipDef
{
    // 配置行 id（源表 id）。
    int32_t id = 0;
    // 动作名或动作索引（action）。
    std::variant<int32_t, std::string> motion;
    // 播放倍率（actionScaleTime）。
    Fixed rate;
    // 循环次数（loop；1、N、小数、-1 = 无限）。
    Fixed loops;
    // 尾部延时（毫秒，actionDelayTime）。
    int32_t tailDelayMs = 0;
    // 位移配置 id。
    int32_t displacementId = 0;
    // 位移策略。
    ClipDisplacementPolicy displacementPolicy;
    // 控制方式。
    ClipControl control;
    // 进入时的瞬移放置。
    ClipPlacement placement;
    // 取消窗口。
    ClipCancel cancel;
    // 片段 Buff（进入时准入，退出时移除）。
    std::vector<int32_t> clipBuffs;
    // 按 frame 升序、同帧按声明顺序排序的 cue 列表。
    std::vector<ClipCue> cues;

    MG_REFLECT(ClipDef,
               id,
               motion,
               rate,
               loops,
               tailDelayMs,
               displacementId,
               displacementPolicy,
               control,
               placement,
               cancel,
               clipBuffs,
               cues)
};

// 特效动作片段，由 action_effect.lua 降级，与角色片段共用一种格式（见 DESIGN.md §6.2）。
// 与 ClipDef 分离为独立类型，因为两个源表的 id 空间重叠。
struct EffectClipDef
{
    // 配置行 id（源表 id）。
    int32_t id = 0;
    // 动作名或动作索引（action）。
    std::variant<int32_t, std::string> motion;
    // 播放倍率（actionScaleTime）。
    Fixed rate;
    // 循环次数（loop）。
    Fixed loops;
    // 尾部延时（毫秒，actionDelayTime）。
    int32_t tailDelayMs = 0;
    // 位移配置 id。
    int32_t displacementId = 0;
    // 位移策略。
    ClipDisplacementPolicy displacementPolicy;
    // 控制方式。
    ClipControl control;
    // 取消窗口（只有普通窗口）。
    ClipCancel cancel;
    // 按受击者 sound_type 索引的命中音效（soundId2）。
    std::vector<std::vector<int32_t>> hitSounds;
    // 按 frame 升序排序的 cue 列表。
    std::vector<ClipCue> cues;

    MG_REFLECT(EffectClipDef,
               id,
               motion,
               rate,
               loops,
               tailDelayMs,
               displacementId,
               displacementPolicy,
               control,
               cancel,
               hitSounds,
               cues)
};
}  // namespace mugen::conf::battle
