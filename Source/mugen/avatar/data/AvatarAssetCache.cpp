#include "AvatarAssetCache.h"

#include "mugen/core/io/FileUtils.h"
#include "mugen/core/serialize/BinaryArchive.h"
#include "mugen/core/serialize/ByteBuffer.h"

#include <algorithm>
#include <cstdio>

NS_MG_BEGIN

namespace
{
// 进程级共享的角色资源缓存实例。缓存加载完成后只读，因此允许用全局单例；
// 每场战斗各自可变的状态不允许放在这里。
AvatarAssetCache* s_instance = nullptr;

// 规范化路径并去掉末尾的 '/'，作为缓存 key 的统一形态。
std::string trimSlash(std::string path)
{
    path = io::normalizePath(std::move(path));
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path;
}
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

// 追加资源查找目录：先去重再放到末尾，靠后的目录在解析相对 key 时优先。
void AvatarAssetCache::addSearchPath(const std::string& path)
{
    std::string normalized = trimSlash(path);
    if (normalized.empty())
        return;
    m_searchPaths.erase(std::remove(m_searchPaths.begin(), m_searchPaths.end(), normalized), m_searchPaths.end());
    m_searchPaths.push_back(std::move(normalized));
}

// 清空资源查找目录，此后只按原始 key 命中，不再拼接查找路径。
void AvatarAssetCache::clearSearchPaths()
{
    m_searchPaths.clear();
}

// 从归档文件加载角色资源。文件头依次是 4 字节魔数、格式版本和结构哈希；
// 三者任一不匹配都拒绝加载。任何一步失败都调用 clear 回滚，避免残留半份数据。
bool AvatarAssetCache::load(const std::string& path)
{
    clear();

    size_t size  = 0;
    uint8_t* raw = io::getFileData(path, size);
    if (raw == nullptr || size == 0)
    {
        MG_LOG_E("Failed to load avatar assets: {}", path);
        return false;
    }

    ByteBuffer buffer;
    buffer.fastSet(raw, static_cast<uint32_t>(size));
    uint8_t magic[4]{};       // 魔数，固定为 MGAV
    uint32_t version    = 0;  // 归档格式版本
    uint64_t schemaHash = 0;  // 缓存结构的哈希，用于拒绝不兼容数据
    // 头部至少 4+4+8 字节；逐字段读取失败同样视为格式错误。
    if (size < 16 || !buffer.getByte(magic[0]) || !buffer.getByte(magic[1]) || !buffer.getByte(magic[2]) ||
        !buffer.getByte(magic[3]) || !buffer.getUint32(version) || !buffer.getUint64(schemaHash) ||
        !std::equal(std::begin(magic), std::end(magic), kAvatarMagic.begin()) || version != kAvatarVersion ||
        schemaHash != avatarSchemaHash())
    {
        MG_LOG_E("Avatar asset header/schema mismatch: {}", path);
        clear();
        return false;
    }

    // 反序列化后还必须完成动作图与时间轴的绑定，二者都成功才算加载完成。
    BinaryReader reader(buffer);
    if (!reader.read(*this) || !bindTimelines())
    {
#if defined(OLUA_AUTOCONF)
        std::fprintf(stderr, "AvatarAssetCache deserialize failed: %s\n", path.c_str());
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
}

// 返回归档是否已成功加载。
bool AvatarAssetCache::isLoaded() const
{
    return m_isLoaded;
}

// 将当前缓存写入归档文件：头部与 load 对称，依次写魔数、版本和结构哈希，再写反射数据。
bool AvatarAssetCache::saveToFile(const std::string& path) const
{
    ByteBuffer buffer;
    for (uint8_t byte : kAvatarMagic)
        buffer.writeByte(byte);
    buffer.writeUint32(kAvatarVersion);
    buffer.writeUint64(avatarSchemaHash());
    BinaryWriter writer(buffer);
    writer.write(*this);
    writer.finish();
    return io::writeDataToFile(reinterpret_cast<const char*>(buffer.data()), buffer.len(), path);
}

// 写入帧动画缓存项，key 先规范化再入库。
void AvatarAssetCache::putAniData(const std::string& key, const AniData& value)
{
    aniDatas[io::normalizePath(key)] = value;
}

// 写入战斗时间轴缓存项，key 先规范化再入库。
void AvatarAssetCache::putCombatTimeline(const std::string& key, const CombatTimeline& value)
{
    combatTimelines[io::normalizePath(key)] = value;
}

// 写入动作图缓存项，key 先规范化再入库。
void AvatarAssetCache::putMotionMap(const std::string& key, const MotionMap& value)
{
    motionMaps[io::normalizePath(key)] = value;
}

// 先按规范化后的原始 key 直接查找，再依次拼接各查找目录（靠后的优先）命中；未找到返回 nullptr。
template <typename T>
const T* AvatarAssetCache::findByPath(const std::map<std::string, T>& map, const std::string& path) const
{
    const std::string key = io::normalizePath(path);
    auto it               = map.find(key);
    if (it != map.end())
        return &it->second;

    for (const std::string& searchPath : m_searchPaths)
    {
        it = map.find(searchPath + "/" + key);
        if (it != map.end())
            return &it->second;
    }
    return nullptr;
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

NS_MG_END
