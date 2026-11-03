#include "mugen/render/battle/PetFollower.h"

#ifdef RUNTIME_IN_AXMOL

#    include <algorithm>
#    include <cmath>
#    include <cstring>

NS_MG_BEGIN

namespace
{
constexpr float kTargetOffsetX          = 70.0f;
constexpr float kTargetOffsetZ          = 10.0f;
constexpr float kDeadBandXMin           = -10.0f;
constexpr float kDeadBandXMax           = 0.0f;
constexpr float kDeadBandZMin           = -5.0f;
constexpr float kDeadBandZMax           = 0.0f;
constexpr float kAccelDistanceX         = 100.0f;
constexpr float kAccelDistanceZ         = 30.0f;
constexpr float kPreventOvershootX      = 10.0f;
constexpr float kPreventOvershootZ      = 5.0f;
constexpr float kTurnAroundDistance     = 5.0f;
constexpr float kAttenuation            = 0.9f;
constexpr float kMaxMoveSpeed           = 0.55f;  // px/ms
constexpr float kSafetySnapDistance     = 500.0f;
constexpr float kSpecIdleTriggerMs      = 10000.0f;
constexpr float kVelocityEpsilon        = 1e-4f;
constexpr float kFollowingVisualScale   = 1.2f;
constexpr float kIdleVisualScale        = 1.0f;
constexpr float kSpecialIdleVisualScale = 1.0f;

float normalizeToward(float value)
{
    return value < 0.0f ? -1.0f : 1.0f;
}

void dampNearTarget(float delta, float preventDistance, float dtMs, float& velocity)
{
    if (dtMs <= 0.0f || std::abs(delta) > preventDistance)
        return;
    const float maxSpeed = std::abs(delta) / dtMs;
    velocity             = std::clamp(velocity, -maxSpeed, maxSpeed) * kAttenuation;
    if (std::abs(velocity) < kVelocityEpsilon)
        velocity = 0.0f;
}
}  // namespace

void PetFollower::setPet(int32_t petId, int32_t spineId)
{
    if (m_skeleton && m_petId == petId && m_spineId == spineId)
        return;
    clear();

    m_skeleton = MgSkeletonAnimation::createFromCache(spineId);
    if (!m_skeleton)
    {
        MG_LOG_E("PetFollower: failed to create spine {} for pet {}", spineId, petId);
        return;
    }
    m_root.addChild(m_skeleton);
    m_skeleton->setCompleteListener([this](const MgTrackEntry& entry) { onAnimationComplete(entry); });

    m_petId           = petId;
    m_spineId         = spineId;
    m_x               = 0.0f;
    m_z               = 0.0f;
    m_vx              = 0.0f;
    m_vz              = 0.0f;
    m_towardX         = 1.0f;
    m_idleMs          = 0.0f;
    m_positioned      = false;
    m_specPlaying     = false;
    m_hasLastHeroPose = false;
    m_motion          = Motion::Follow;
    setMotion(Motion::Idle);
    MG_LOG_I("PetFollower: pet={} spine={}", m_petId, m_spineId);
}

void PetFollower::clear()
{
    if (m_skeleton)
    {
        m_skeleton->setCompleteListener(nullptr);
        m_skeleton->removeFromParent();
        m_skeleton = nullptr;
    }
    m_petId          = 0;
    m_spineId        = 0;
    m_x              = 0.0f;
    m_z              = 0.0f;
    m_vx             = 0.0f;
    m_vz             = 0.0f;
    m_towardX        = 1.0f;
    m_scale          = kIdleVisualScale;
    m_idleMs         = 0.0f;
    m_positioned     = false;
    m_specPlaying    = false;
    m_hasLastHeroPose = false;
    m_motion         = Motion::Idle;
}

void PetFollower::present(const HeroPose& heroPose, float renderDelta)
{
    if (!m_skeleton)
        return;

    const float dtMs       = std::max(0.0f, renderDelta) * 1000.0f;
    const float heroTowardX = normalizeToward(heroPose.towardX);
    const float heroTowardZ = normalizeToward(heroPose.towardZ);
    const float targetX    = heroPose.x - kTargetOffsetX * heroTowardX;
    const float targetZ    = heroPose.z - kTargetOffsetZ * heroTowardZ;

    if (!m_positioned)
    {
        m_x          = targetX;
        m_z          = targetZ;
        m_towardX    = heroTowardX;
        m_positioned = true;
    }

    float heroVx = 0.0f;
    float heroVz = 0.0f;
    if (m_hasLastHeroPose && dtMs > 0.0f)
    {
        heroVx = (heroPose.x - m_lastHeroPose.x) / dtMs;
        heroVz = (heroPose.z - m_lastHeroPose.z) / dtMs;
    }
    m_lastHeroPose  = heroPose;
    m_hasLastHeroPose = true;

    const float heroDx  = heroPose.x - m_x;
    const float heroDz  = heroPose.z - m_z;
    const float heroDistSq = heroDx * heroDx + heroDz * heroDz;
    if (heroDistSq > kSafetySnapDistance * kSafetySnapDistance)
    {
        m_x       = targetX;
        m_z       = targetZ;
        m_vx      = 0.0f;
        m_vz      = 0.0f;
        m_towardX = heroTowardX;
    }

    float deltaX = targetX - m_x;
    float deltaZ = targetZ - m_z;
    const float relX = (m_x - targetX) * heroTowardX;
    const float relZ = (m_z - targetZ) * heroTowardZ;
    if (relX >= kDeadBandXMin && relX <= kDeadBandXMax)
        deltaX = 0.0f;
    if (relZ >= kDeadBandZMin && relZ <= kDeadBandZMax)
        deltaZ = 0.0f;

    m_vx = heroVx + (deltaX / kAccelDistanceX) * kMaxMoveSpeed;
    m_vz = heroVz + (deltaZ / kAccelDistanceZ) * kMaxMoveSpeed;
    const float speed = std::sqrt(m_vx * m_vx + m_vz * m_vz);
    if (speed > kMaxMoveSpeed && speed > 0.0f)
    {
        const float ratio = kMaxMoveSpeed / speed;
        m_vx *= ratio;
        m_vz *= ratio;
    }

    dampNearTarget(deltaX, kPreventOvershootX, dtMs, m_vx);
    dampNearTarget(deltaZ, kPreventOvershootZ, dtMs, m_vz);

    if (dtMs > 0.0f)
    {
        m_x += m_vx * dtMs;
        m_z += m_vz * dtMs;
    }

    if (std::abs(targetX - m_x) > kTurnAroundDistance)
        m_towardX = targetX >= m_x ? 1.0f : -1.0f;

    const bool still = std::abs(m_vx) < kVelocityEpsilon && std::abs(m_vz) < kVelocityEpsilon;
    if (still)
    {
        m_x       = targetX;
        m_z       = targetZ;
        m_towardX = heroTowardX;
        m_idleMs += dtMs;
        if (!m_specPlaying && m_idleMs >= kSpecIdleTriggerMs)
        {
            m_idleMs -= kSpecIdleTriggerMs;
            const int roll = static_cast<int>(m_random() % 100) + 1;
            if (roll > 30)
            {
                m_specPlaying = true;
                setMotion(Motion::SpecIdle);
            }
        }
        if (!m_specPlaying)
            setMotion(Motion::Idle);
    }
    else
    {
        m_idleMs      = 0.0f;
        m_specPlaying = false;
        setMotion(Motion::Follow);
    }

    m_skeleton->setPosition(m_x, m_z);
    m_skeleton->setLocalZOrder(-static_cast<int>(std::floor(m_z)));
    m_skeleton->setScaleX(m_scale * m_towardX);
    m_skeleton->setScaleY(m_scale);
}

void PetFollower::setMotion(Motion motion)
{
    if (!m_skeleton || m_motion == motion)
        return;

    const char* animation = "stand";
    bool loop             = true;
    switch (motion)
    {
    case Motion::Idle:
        animation = "stand";
        loop      = true;
        m_scale   = kIdleVisualScale;
        break;
    case Motion::Follow:
        animation = "running";
        loop      = true;
        m_scale   = kFollowingVisualScale;
        break;
    case Motion::SpecIdle:
        animation = "special";
        loop      = false;
        m_scale   = kSpecialIdleVisualScale;
        break;
    }

    if (!m_skeleton->setAnimation(0, animation, loop))
        MG_LOG_W("PetFollower: animation '{}' missing for pet {}", animation, m_petId);
    m_motion = motion;
}

void PetFollower::onAnimationComplete(const MgTrackEntry& entry)
{
    if (!m_specPlaying || !entry.animationName())
        return;
    if (std::strcmp(entry.animationName(), "special") != 0)
        return;
    m_specPlaying = false;
    setMotion(Motion::Idle);
}

NS_MG_END

#endif
