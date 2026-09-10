#pragma once

#include "mugen/render/spine/MgSkeletonData.h"
#include "mugen/render/spine/MgTrackEntry.h"

#ifdef RUNTIME_IN_AXMOL

NS_MG_BEGIN

class MgSkeletonAnimation : public ax::Node
{
public:
    using CompleteListener = std::function<void(const MgTrackEntry& entry)>;

    static MgSkeletonAnimation* createWithData(MgSkeletonData* data);
    static MgSkeletonAnimation* createWithFile(const std::string& skeletonFile,
                                               const std::string& atlasFile,
                                               float scale = 1.0f);
    static MgSkeletonAnimation* createWithFile(const std::string& skeletonFile,
                                               const std::vector<std::string>& atlasFiles,
                                               float scale = 1.0f);

    MgSkeletonData* skeletonData() const { return m_data; }
    bool isValid() const;

    void setAutoUpdate(bool enabled);

    MgTrackEntry setAnimation(int trackIndex, const std::string& name, bool loop);
    MgAnimation findAnimation(const std::string& name) const;
    MgTrackEntry getCurrent(int trackIndex = 0);
    void keepCurrentTrackAlive(int trackIndex = 0);
    void seekCurrentTrack(int trackIndex, float timeSeconds);
    void setSkin(const std::string& name);
    void setSlotsToSetupPose();
    void setTimeScale(float scale);
    void clearTracks();
    void setCompleteListener(const CompleteListener& listener);
    void setUpdateOnlyIfVisible(bool value);

    void update(float dt) override;
    void onEnter() override;
    void onExit() override;
    ax::Rect getBoundingBox() const override;

protected:
    MgSkeletonAnimation() = default;
    ~MgSkeletonAnimation() override;

    bool initWithData(MgSkeletonData* data);

private:
    MgSkeletonData* m_data = nullptr;
    ax::Node* m_inner      = nullptr;
    bool m_autoUpdate      = true;
    bool m_ownsData        = false;
};

NS_MG_END

#endif
