#pragma once

#include "mugen/core/StdC.h"

#ifdef RUNTIME_IN_AXMOL

#    include "spine/SkeletonAnimation.h"

NS_MG_BEGIN

// 屏蔽自动 scheduleUpdate 的 Spine 节点，由上层手动驱动
class ManualSkeletonAnimation : public spine::SkeletonAnimation
{
public:
    // 从骨骼/图集文件创建
    static ManualSkeletonAnimation* createWithFile(const std::string& skeletonFile,
                                                   const std::string& atlasFile,
                                                   float scale = 1.0f);

    // 仅调用 Node::onEnter，不启动自动更新
    virtual void onEnter() override;

    // 仅调用 Node::onExit
    virtual void onExit() override;

protected:
    ManualSkeletonAnimation() = default;
    virtual ~ManualSkeletonAnimation() override;
};

NS_MG_END

#endif  // RUNTIME_IN_AXMOL
