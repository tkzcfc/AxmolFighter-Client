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
    // 实例私有数据（时装身体）：节点析构时 delete data；可 replaceAtlas 换装
    static MgSkeletonAnimation* createWithOwnedData(MgSkeletonData* data);

    // 共享缓存·同步（固定 atlas：特效/立绘/传送门等）
    static MgSkeletonAnimation* create(int32_t resSpineId);
    static MgSkeletonAnimation* create(std::string_view skeletonFile, float scale = 1.0f);
    static MgSkeletonAnimation* create(std::string_view skeletonFile, const std::string& atlasFile, float scale = 1.0f);

    // 实例私有·异步（UI 临时展示；返回的节点自持数据，回调主线程触发）
    static void createAsync(int32_t resSpineId, std::function<void(MgSkeletonAnimation*)> onDone);
    static void createAsync(std::string skeletonFile,
                            std::vector<std::string> atlasFiles,
                            float scale,
                            std::function<void(MgSkeletonAnimation*)> onDone);

    MgSkeletonData* skeletonData() const { return m_data; }
    bool ownsData() const { return m_ownsData; }
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
