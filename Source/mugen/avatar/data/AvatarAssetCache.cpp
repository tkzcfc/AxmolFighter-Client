#include "AvatarAssetCache.h"

#include "mugen/core/io/FileUtils.h"

#include <cstdio>

#include "mugen/core/serialize/BinaryArchive.h"
#include "mugen/core/serialize/ByteBuffer.h"
#if !defined(MG_NO_VFS)
#include "vfs/VirtualFileSystem.h"
#endif

#include <algorithm>

NS_MG_BEGIN

namespace
{
// 进程级共享的角色资源缓存实例。缓存加载完成后只读，因此允许用全局单例；
// 每场战斗各自可变的状态不允许放在这里。
AvatarAssetCache* s_instance = nullptr;

}  // namespace

// 返回进程共享的缓存实例，首次调用时惰性创建。
AvatarAssetCache* AvatarAssetCache::getInstance()
{
    if (!s_instance)
        s_instance = new (std::nothrow) AvatarAssetCache();
    return s_instance;
}

// 销毁进程共享的缓存实例并清空指针。
void AvatarAssetCache::destroy()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

// 清空全部缓存项并复位加载状态，用于重新加载或加载失败回滚。
void AvatarAssetCache::clear()
{
    m_isLoaded = false;
    aniDatas.clear();
    combatTimelines.clear();
    motionMaps.clear();
}

// 返回归档是否已成功加载。
bool AvatarAssetCache::isLoaded() const
{
    return m_isLoaded;
}

// Insert AniData; key is a standard VFS path (/xxx/yyy).
void AvatarAssetCache::putAniData(const std::string& key, const AniData& value)
{
    aniDatas[key] = value;
}

// Insert CombatTimeline; key is a standard VFS path (/xxx/yyy).
void AvatarAssetCache::putCombatTimeline(const std::string& key, const CombatTimeline& value)
{
    combatTimelines[key] = value;
}

// Insert MotionMap; key is a standard VFS path (/xxx/yyy).
void AvatarAssetCache::putMotionMap(const std::string& key, const MotionMap& value)
{
    motionMaps[key] = value;
}

// Lookup by exact VFS path key; nullptr if missing.
template <typename T>
const T* AvatarAssetCache::findByPath(const std::map<std::string, T>& map, const std::string& path) const
{
    // Keys are standard VFS paths (/xxx/yyy); exact match only.
    const auto it = map.find(path);
    return it != map.end() ? &it->second : nullptr;
}

// 按路径查询帧动画，未命中返回 nullptr。
const AniData* AvatarAssetCache::getAniData(const std::string& path) const
{
    return findByPath(aniDatas, path);
}

// 按路径查询战斗时间轴，未命中返回 nullptr。
const CombatTimeline* AvatarAssetCache::getCombatTimeline(const std::string& path) const
{
    return findByPath(combatTimelines, path);
}

// 按路径查询动作图，未命中返回 nullptr。
const MotionMap* AvatarAssetCache::getMotionMap(const std::string& path) const
{
    return findByPath(motionMaps, path);
}

// 遍历全部动作图，让每个动作图通过本缓存的查询接口绑定其战斗时间轴；
// 任一条绑定失败则整体加载失败。
bool AvatarAssetCache::bindTimelines()
{
    for (auto& kv : motionMaps)
    {
        if (!kv.second.bindTimelines([this](const std::string& boxPath) { return getCombatTimeline(boxPath); }))
        {
#if defined(OLUA_AUTOCONF)
            std::fprintf(stderr, "AvatarAssetCache: bind motion failed '%s'\n", kv.first.c_str());
#else
            MG_LOG_E("AvatarAssetCache: bind motion failed '{}'", kv.first);
#endif
            return false;
        }
    }
    return true;
}

// 从归档文件加载角色资源。文件头依次是 4 字节魔数、格式版本和结构哈希；
// 三者任一不匹配都拒绝加载。任何一步失败都调用 clear 回滚，避免残留半份数据。
bool AvatarAssetCache::load(const std::string& path)
{
#if defined(MG_NO_VFS)
    (void)path;
    return false;
#else
    clear();

    std::vector<uint8_t> bytes;
    if (io::vfs()->readFile(path, bytes) != vfs::Error::OK || bytes.empty())
    {
        MG_LOG_E("Failed to load avatar assets: {}", path);
        return false;
    }

    ByteBuffer buffer(bytes.data(), static_cast<uint32_t>(bytes.size()));
    const size_t size = bytes.size();
    uint8_t magic[4]{};
    uint32_t version    = 0;
    uint64_t schemaHash = 0;
    if (size < 16 || !buffer.getByte(magic[0]) || !buffer.getByte(magic[1]) || !buffer.getByte(magic[2]) ||
        !buffer.getByte(magic[3]) || !buffer.getUint32(version) || !buffer.getUint64(schemaHash) ||
        !std::equal(std::begin(magic), std::end(magic), kAvatarMagic.begin()) || version != kAvatarVersion ||
        schemaHash != avatarSchemaHash())
    {
        MG_LOG_E("Avatar asset header/schema mismatch: {}", path);
        clear();
        return false;
    }

    BinaryReader reader(buffer);
    if (!reader.read(*this) || !bindTimelines())
    {
#if defined(OLUA_AUTOCONF)
        std::fprintf(stderr, "AvatarAssetCache deserialize failed: %s", path.c_str());
#else
        MG_LOG_E("AvatarAssetCache deserialize failed: {}", path);
#endif
        clear();
        return false;
    }

    MG_LOG_I("Loaded avatar assets: ani={} box={} motion={}", aniDatas.size(), combatTimelines.size(),
             motionMaps.size());
    m_isLoaded = true;
    return true;
#endif
}

// 将当前缓存写入归档文件：头部与 load 对称，依次写魔数、版本和结构哈希，再写反射数据。
bool AvatarAssetCache::saveToFile(const std::string& path) const
{
#if defined(MG_NO_VFS)
    (void)path;
    return false;
#else
    ByteBuffer buffer;
    for (uint8_t byte : kAvatarMagic)
        buffer.writeByte(byte);
    buffer.writeUint32(kAvatarVersion);
    buffer.writeUint64(avatarSchemaHash());
    BinaryWriter writer(buffer);
    writer.write(*this);
    writer.finish();
    return io::vfs()->writeFile(path, buffer.data(), buffer.len()) == vfs::Error::OK;
#endif
}

NS_MG_END
