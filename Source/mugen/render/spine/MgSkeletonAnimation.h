#pragma once

#include "mugen/render/spine/MgSkeletonData.h"
#include "mugen/render/spine/MgTrackEntry.h"

#ifdef RUNTIME_IN_AXMOL

#    include <string>
#    include <vector>

NS_MG_BEGIN

// 皮肤嫁接时被覆盖的原 attachment（backend 私有语义）
struct MgSkinSlotOriginal
{
    void* skin       = nullptr;
    void* attachment = nullptr;
    int slotIndex    = -1;
    std::string name;
};
using MgSkinSlotOriginals = std::vector<MgSkinSlotOriginal>;

class MgSkeletonAnimation : public ax::Node
{
public:
    using CompleteListener = std::function<void(const MgTrackEntry& entry)>;
    using CreateCallback   = std::function<void(MgSkeletonAnimation*)>;

    static MgSkeletonAnimation* createWithData(MgSkeletonDataPtr data);

    // 从共享缓存（SpineSkeletonCache）取或建骨架数据；数据只读，不可 replaceAtlas / graftSkinSlots（atlas
    // 为空时取骨架同名 .atlas）
    static MgSkeletonAnimation* createFromCache(int32_t resSpineId);
    static MgSkeletonAnimation* createFromCache(std::string_view skeletonFile,
                                                const std::string& atlasFile = "",
                                                float scale                  = 1.0f);

    // 从共享缓存异步取或建
    static void createFromCacheAsync(int32_t resSpineId, CreateCallback onDone);

    // 实例私有·异步（可 replaceAtlas / 皮肤嫁接）
    static void createExclusiveAsync(std::string skeletonFile,
                                     std::vector<std::string> atlasFiles,
                                     float scale,
                                     CreateCallback onDone);

    MgSkeletonData* skeletonData() const { return m_data.get(); }
    const MgSkeletonDataPtr& skeletonDataPtr() const { return m_data; }
    bool isDataExclusive() const { return m_data->isExclusive(); }
    bool isValid() const;

    void setAutoUpdate(bool enabled);

    MgTrackEntry setAnimation(int trackIndex, const std::string& name, bool loop);
    MgAnimation findAnimation(const std::string& name) const;
    MgTrackEntry getCurrent(int trackIndex = 0);
    void keepCurrentTrackAlive(int trackIndex = 0);
    void seekCurrentTrack(int trackIndex, float timeSeconds);
    // 切换皮肤并重置到 setup pose；已有嫁接会重新应用到新皮肤
    void setSkin(const std::string& name);
    void setSlotsToSetupPose();
    void setTimeScale(float scale);
    void clearTracks();
    void setCompleteListener(const CompleteListener& listener);
    void setUpdateOnlyIfVisible(bool value);

    // 替换图集（须 exclusive）；嫁接先撤销、换完再重新应用，避免 donor 的 attachment 被重指
    bool replaceAtlas(const std::vector<std::string>& atlasFiles);
    bool replaceAtlas(MgAtlasHandlePtr atlas);

    // 皮肤嫁接（须 exclusive）：把 donor 皮肤 srcSkinName 中 names 同名 attachment 覆盖到本骨架。
    // 节点持有 donor 直到对应 names 被 clear，donor 的 attachment 不会先于嫁接失效
    bool graftSkinSlots(const MgSkeletonDataPtr& donor, const char* srcSkinName, const std::vector<std::string>& names);
    void clearSkinSlots(const std::vector<std::string>& names);

    void update(float dt) override;
    void onEnter() override;
    void onExit() override;
    ax::Rect getBoundingBox() const override;

protected:
    MgSkeletonAnimation() = default;
    ~MgSkeletonAnimation() override;

    bool initWithData(MgSkeletonDataPtr data);

private:
    struct Graft
    {
        MgSkeletonDataPtr donor;
        std::string skin;
        std::vector<std::string> names;
    };

    // 撤销全部嫁接（返回原记录，供 reapply）
    std::vector<Graft> takeGrafts();
    void applyGrafts(std::vector<Graft> grafts);

    MgSkeletonDataPtr m_data;
    ax::Node* m_inner = nullptr;
    bool m_autoUpdate = true;
    std::vector<Graft> m_grafts;
    MgSkinSlotOriginals m_slotOriginals;
};

NS_MG_END

#endif
