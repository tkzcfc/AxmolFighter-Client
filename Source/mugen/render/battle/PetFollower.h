#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "mugen/render/spine/MgSkeletonAnimation.h"

#    include <cstdint>
#    include <random>

NS_MG_BEGIN

// 战斗表现层宠物跟随器：只驱动渲染节点，不写模拟状态。
class PetFollower
{
public:
    // 主角插值姿态（屏幕投影前的世界坐标）。
    struct HeroPose
    {
        float x       = 0.0f;
        float y       = 0.0f;
        float z       = 0.0f;
        float towardX = 1.0f;
        float towardZ = 1.0f;
    };

    explicit PetFollower(ax::Node& root) : m_root(root) {}

    // 绑定宠物骨架；同 id 重入直接复用。
    void setPet(int32_t petId, int32_t spineId);
    // 清理当前宠物节点。
    void clear();
    // 每帧按主角姿态推进跟随与动作状态。
    void present(const HeroPose& heroPose, float renderDelta);

private:
    enum class Motion : uint8_t
    {
        Idle,
        Follow,
        SpecIdle,
    };

    // 切换宠物动作和缩放。
    void setMotion(Motion motion);
    // 处理 special 动作播完后的回落。
    void onAnimationComplete(const MgTrackEntry& entry);

    ax::Node& m_root;
    MgSkeletonAnimation* m_skeleton = nullptr;
    int32_t m_petId                 = 0;
    int32_t m_spineId               = 0;
    float m_x                       = 0.0f;
    float m_z                       = 0.0f;
    float m_vx                      = 0.0f;
    float m_vz                      = 0.0f;
    float m_towardX                 = 1.0f;
    float m_scale                   = 1.0f;
    float m_idleMs                  = 0.0f;
    bool m_positioned               = false;
    bool m_specPlaying              = false;
    bool m_hasLastHeroPose          = false;
    Motion m_motion                 = Motion::Idle;
    HeroPose m_lastHeroPose;
    std::minstd_rand m_random{0x6A09E667u};
};

NS_MG_END

#endif
