#pragma once

#include "mugen/core/reflect/Reflect.h"

namespace mugen::conf
{
// res_sound 表映射的音效资源配置。
struct ResSoundConfig
{
    // 文件路径、标识、循环标记和音量。
    std::string fileName;
    int32_t id   = 0;
    int32_t loop = 0;
    Fixed volume;

    MG_REFLECT(ResSoundConfig, fileName, id, loop, volume)
};

// sound_ui 表映射的界面音效配置。
struct SoundUiConfig
{
    // 按钮名称、按钮音效、编辑音效、操作音效和界面名称。
    std::vector<std::variant<int32_t, std::string>> buttonName;
    std::vector<int32_t> soundIdButton;
    int32_t soundIdEd = 0;
    std::variant<int32_t, std::string> soundIdOp;
    std::string viewName;

    MG_REFLECT(SoundUiConfig, buttonName, soundIdButton, soundIdEd, soundIdOp, viewName)
};

// sound_spine 表映射的角色动作音效配置。
struct SoundSpineConfig
{
    // 动作名称、标识、音效 id 与骨架 id。
    std::vector<std::string> actionIds;
    int32_t id = 0;
    std::vector<int32_t> soundIds;
    int32_t spineId = 0;

    MG_REFLECT(SoundSpineConfig, actionIds, id, soundIds, spineId)
};

// sound_spine_bgm 表映射的角色背景音效配置。
struct SoundSpineBgmConfig
{
    // 动作名称、标识、音效 id 与骨架 id。
    std::vector<std::string> actionIds;
    int32_t id = 0;
    std::vector<int32_t> soundIds;
    int32_t spineId = 0;

    MG_REFLECT(SoundSpineBgmConfig, actionIds, id, soundIds, spineId)
};

// sound_map_spine 表映射的地图动作音效配置。
struct SoundMapSpineConfig
{
    // 动作名称、标识、音效 id 与骨架名称。
    std::vector<std::string> actionIds;
    int32_t id = 0;
    std::vector<int32_t> soundIds;
    std::variant<int32_t, std::string> spineName;

    MG_REFLECT(SoundMapSpineConfig, actionIds, id, soundIds, spineName)
};

// sound_talk 表映射的对话音效配置。
struct SoundTalkConfig
{
    // 淡入时间、标识、等级限制、位置、声音、文本和类型。
    std::vector<int32_t> fadeTime;
    int32_t id = 0;
    std::vector<int32_t> levelLimit;
    std::vector<int32_t> param;
    std::vector<int32_t> position;
    std::vector<int32_t> talkSound;
    std::vector<std::vector<int32_t>> talkText;
    int32_t talkType = 0;

    MG_REFLECT(SoundTalkConfig, fadeTime, id, levelLimit, param, position, talkSound, talkText, talkType)
};

// sound_sendmessage 表映射的消息发送音效配置。
struct SoundSendMessageConfig
{
    // 标识、消息类型及成功、失败音效。
    int32_t id            = 0;
    int32_t sendmessageId = 0;
    int32_t soundIdFalse  = 0;
    int32_t soundIdTrue   = 0;

    MG_REFLECT(SoundSendMessageConfig, id, sendmessageId, soundIdFalse, soundIdTrue)
};
}  // namespace mugen::conf
