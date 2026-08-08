#pragma once

#include "mugen/core/ecs/Component.h"
#include "mugen/conf/Config.h"
#include "mugen/core/math/DamageBox.h"
#include "mugen/avatar/MotionPlayer.h"

NS_MG_BEGIN

class AvatarComponent : public Component
{
public:
    typedef Component Super;

public:
    AvatarComponent() {}

    virtual ~AvatarComponent() {}

    MG_FORCEINLINE void play(const std::string& name, int32_t loop, bool force)
    {
        if (!force && playback.isPlaying() && name == playback.getCurrentMotionName())
            return;
        playback.play(name, "", loop < 0);
        animationFinished = false;
    }

    const std::vector<DamageBox>& getAttackBoxes() const { return attackBoxes; }

    const std::vector<DamageBox>& getDamageBoxes() const { return damageBoxes; }

    float getSpineScale() const
    {
        if (resSpine && resSpine->scale > 0.0f)
            return resSpine->scale;
        return spineScale > 0.0f ? spineScale : 1.0f;
    }

    const std::string& getSpineSkeleton() const
    {
        // spawn 时写入的路径（含城镇 *_city 覆盖）优先于表配置
        if (!spineSkeleton.empty())
            return spineSkeleton;
        return resSpine ? resSpine->spine : spineSkeleton;
    }

    const std::string& getSpineAtlas() const
    {
        if (!spineAtlas.empty())
            return spineAtlas;
        return resSpine && !resSpine->atlas.empty() ? resSpine->atlas : spineAtlas;
    }

public:
    const RoleConfig* roleConfig                   = nullptr;
    const ResSpineConfig* resSpine                 = nullptr;
    const BehaviorTemplateConfig* behaviorTemplate = nullptr;
    // 兼容旧序列化：角色 id（>0 时反序列化从 Config 恢复指针）
    int32_t roleId = 0;

    // 烘培路径缓存（spawn 时写入，避免渲染层再解析）
    std::string spineSkeleton;
    std::string spineAtlas;
    std::string defaultSkin;
    std::string defaultAnimationPath;
    std::string motionFile;
    float spineScale = 1.0f;

    MotionPlayer playback;

    bool animationFinished = true;
    float animationSpeed   = 1.0f;

    std::vector<DamageBox> attackBoxes;
    std::vector<DamageBox> damageBoxes;

    MG_DEFINE_SERIALIZABLE_CUSTOM(serializeCustomImpl,
                                  deserializeCustomImpl,
                                  roleId,
                                  spineSkeleton,
                                  spineAtlas,
                                  defaultSkin,
                                  defaultAnimationPath,
                                  motionFile,
                                  spineScale,
                                  playback,
                                  animationFinished,
                                  animationSpeed,
                                  attackBoxes,
                                  damageBoxes)

private:
    void serializeCustomImpl(ByteBuffer& byteBuffer) const;

    bool deserializeCustomImpl(ByteBuffer& byteBuffer);
};

NS_MG_END
