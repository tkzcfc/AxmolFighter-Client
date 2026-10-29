#include "io/VfsFileUtils.h"

#include "mugen/core/io/ContentMount.h"
#include "mugen/core/io/FileUtils.h"
#include "vfs/Error.h"
#include "vfs/VirtualFileSystem.h"
#include "vfs/DirEntry.h"

#if (AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID)
#    include "platform/android/FileUtils-android.h"
#    include "vfs/android/AndroidAssetBackend.h"
#    include "vfs/archive/ArchiveBackend.h"
#endif

#include <cctype>
#include <cstdint>

namespace
{
bool looksNativeAbsolute(std::string_view path)
{
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':')
        return true;
    // UNC / Windows 扩展路径
    if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\')
        return true;
    if (path.size() >= 2 && path[0] == '/' && path[1] == '/')
        return true;
    return false;
}

bool startsWithPath(std::string_view path, std::string_view prefix)
{
    if (path.size() < prefix.size())
        return false;
    if (path.compare(0, prefix.size(), prefix) != 0)
        return false;
    return path.size() == prefix.size() || path[prefix.size()] == '/';
}

/** 把反斜杠规范为正斜杠。 */
std::string normalizeSlashes(std::string_view filename)
{
    std::string path(filename);
    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }
    return path;
}

/** Content/native（APK assets/native）下的路径跳过 VFS，只走平台 FileUtils。 */
bool isNativeContentRel(std::string_view path)
{
    std::string_view rel = path;
    if (!rel.empty() && rel.front() == '/')
        rel.remove_prefix(1);
    return startsWithPath(rel, "native");
}
}  // namespace

VfsFileUtils::VfsFileUtils(std::shared_ptr<vfs::VirtualFileSystem> fs, ax::FileUtils* native,
                           std::string nativeWritable)
    : m_fs(std::move(fs))
    , m_native(native)
    , m_nativeWritable(std::move(nativeWritable))
{
}

VfsFileUtils::~VfsFileUtils()
{
    delete m_native;
    m_native = nullptr;
}

VfsFileUtils* VfsFileUtils::install()
{
    // 拆掉旧委托：若先前已是 VfsFileUtils，析构会释放其 m_native，避免泄漏与重复 addSearchPath。
    // destroyInstance == AX_SAFE_DELETE(s_sharedFileUtils)；setDelegate 也会 delete 旧实例。
    ax::FileUtils::destroyInstance();

    auto* native                 = ax::FileUtils::getInstance();
    const std::string content    = native->getDefaultResourceRootPath();
    const std::string nativeUser = native->getNativeWritableAbsolutePath();
    auto fs                      = std::make_shared<vfs::VirtualFileSystem>();

#if (AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID)
    // 禁止 mountNative("/", "assets/")：那是把 APK assets 当成真实文件系统目录。
    // 优先 content.vfs 只读挂到 `/`，否则用 Android AssetManager 挂 `/`。
    vfs::AndroidAssetBackend::setAssetManager(ax::FileUtilsAndroid::getAssetManager());
    vfs::Error assetErr{};
    auto assets = vfs::AndroidAssetBackend::create({}, nullptr, &assetErr);
    if (!assets)
    {
        AXLOGE("VfsFileUtils: AndroidAssetBackend::create failed: {} ({})", vfs::errorName(assetErr),
               vfs::errorMessage(assetErr));
        return nullptr;
    }

    vfs::Error srcErr{};
    auto archiveSrc = assets->archiveSource("content.vfs", &srcErr);
    vfs::Error mountErr{};
    if (archiveSrc)
    {
        auto archive = vfs::ArchiveBackend::create(archiveSrc, {}, &mountErr);
        if (!archive || fs->mount("/", archive, vfs::MountOptions{0, vfs::MountFlags::ReadOnly}, &mountErr) == 0)
        {
            AXLOGE("VfsFileUtils: failed to mount assets/content.vfs at /: {} ({})", vfs::errorName(mountErr),
                   vfs::errorMessage(mountErr));
            return nullptr;
        }
        AXLOGI("VfsFileUtils: mounted assets/content.vfs at /");
    }
    else
    {
        if (fs->mount("/", assets, vfs::MountOptions{0, vfs::MountFlags::ReadOnly}, &mountErr) == 0)
        {
            AXLOGE("VfsFileUtils: failed to mount Android assets at /: {} ({})", vfs::errorName(mountErr),
                   vfs::errorMessage(mountErr));
            return nullptr;
        }
        AXLOGI("VfsFileUtils: mounted Android AssetManager at / (no content.vfs)");
    }
#else
    const vfs::Error contentErr = mugen::io::mountContentRoot(*fs, content);
    if (contentErr != vfs::Error::OK)
    {
        AXLOGE("VfsFileUtils: failed to mount '{}' at /: {} ({})", content, vfs::errorName(contentErr),
               vfs::errorMessage(contentErr));
        return nullptr;
    }
    AXLOGI("VfsFileUtils: mounted {} at /", content);
#endif

    const vfs::Error userErr = mugen::io::mountUserWritable(*fs, nativeUser);
    if (userErr != vfs::Error::OK)
    {
        AXLOGE("VfsFileUtils: failed to mount writable '{}' at /user: {} ({})", nativeUser, vfs::errorName(userErr),
               vfs::errorMessage(userErr));
        return nullptr;
    }
    AXLOGI("VfsFileUtils: mounted {} at /user", nativeUser);

    // Content/native（APK：assets/native）不进 VFS，由原生 FileUtils 解析相对路径。
    native->addSearchPath("native", /*front=*/true);
    AXLOGI("VfsFileUtils: native FileUtils search path +native (for Content/native / assets/native)");

    // 编译后的着色器在 exeDir/axslc（或 APK assets/axslc）。ProgramManager 会向当前
    // FileUtils 委托加搜索路径，但 VfsFileUtils::fullPathForFilename 在 VFS 未命中时
    // 回落到 m_native，须在此同步挂上，否则启动后找不到 shader。
#if (AX_TARGET_PLATFORM == AX_PLATFORM_WIN32) || (AX_TARGET_PLATFORM == AX_PLATFORM_LINUX)
    const std::string axslcDir = ax::FileUtils::getAppRoot() + "axslc";
    native->addSearchPath(axslcDir);
    AXLOGI("VfsFileUtils: native FileUtils search path +{} (compiled shaders)", axslcDir);
#else
    native->addSearchPath("axslc");
    AXLOGI("VfsFileUtils: native FileUtils search path +axslc (compiled shaders)");
#endif

    auto* self = new VfsFileUtils(fs, native, nativeUser);
    self->init();
    // getInstance() 已把 native 放进 s_sharedFileUtils；setDelegate 会 delete 旧实例。
    // 先卸掉指针，避免 setDelegate 删掉我们要持有的 m_native（由 ~VfsFileUtils 负责释放）。
    s_sharedFileUtils = nullptr;
    ax::FileUtils::setDelegate(self);
    mugen::io::setVfs(fs.get());
    return self;
}

bool VfsFileUtils::init()
{
    _defaultResRootPath  = "/";
    _writablePath        = "/user/";
    _searchPathArray     = {"/"};
    _originalSearchPaths = {"/"};
    return true;
}

std::string VfsFileUtils::toVfsPath(std::string_view filename) const
{
    std::string path = normalizeSlashes(filename);
    if (path.empty() || looksNativeAbsolute(path))
        return {};
    // native/ 跳过 VFS：MediaPlayer / 平台解码用 m_native + search path "native" 解析
    if (isNativeContentRel(path))
        return {};
    // 交 VFS 的逻辑路径且不是 `/` 开头时，前补 `/`（于是 mugen/xxx.png 与 /mugen/xxx.png）
    if (path.front() != '/')
        path.insert(path.begin(), '/');
    return path;
}

ax::FileUtils::Status VfsFileUtils::readVfs(const std::string& vfsPath, ax::ResizableBuffer* buffer) const
{
    vfs::Error err{};
    auto stream = m_fs->openFileStream(vfsPath, ax::IFileStream::Mode::READ, &err);
    if (!stream)
        return err == vfs::Error::NOT_FOUND ? ax::FileUtils::Status::NotExists : ax::FileUtils::Status::OpenFailed;

    const int64_t size = stream->size();
    if (size < 0)
        return ax::FileUtils::Status::ObtainSizeFailed;
    if (size > static_cast<int64_t>(UINT32_MAX))
        return ax::FileUtils::Status::TooLarge;

    buffer->resize(static_cast<size_t>(size));
    if (size == 0)
        return ax::FileUtils::Status::OK;

    size_t done        = 0;
    const size_t total = static_cast<size_t>(size);
    while (done < total)
    {
        const int n = stream->read(static_cast<char*>(buffer->buffer()) + done,
                                   static_cast<unsigned int>(total - done));
        if (n <= 0)
            break;
        done += static_cast<size_t>(n);
    }
    if (done != total)
    {
        buffer->resize(done);
        return ax::FileUtils::Status::ReadFailed;
    }
    return ax::FileUtils::Status::OK;
}

ax::FileUtils::Status VfsFileUtils::getContents(std::string_view filename, ax::ResizableBuffer* buffer) const
{
    const std::string vfsPath = toVfsPath(filename);
    if (!vfsPath.empty() && m_fs->isFile(vfsPath))
        return readVfs(vfsPath, buffer);
    return m_native->getContents(filename, buffer);
}

std::unique_ptr<ax::IFileStream> VfsFileUtils::openFileStream(std::string_view filePath,
                                                             ax::IFileStream::Mode mode) const
{
    const std::string vfsPath = toVfsPath(filePath);
    if (!vfsPath.empty())
    {
        if (mode == ax::IFileStream::Mode::READ)
        {
            if (m_fs->isFile(vfsPath))
                return m_fs->openFileStream(vfsPath, mode);
        }
        else
        {
            // 写：有 VFS 路径则先试 VFS（可写性由 VirtualFileSystem / 后端决定）；失败再回落 m_native
            auto stream = m_fs->openFileStream(vfsPath, mode);
            if (stream)
                return stream;
        }
    }
    return m_native->openFileStream(filePath, mode);
}

int64_t VfsFileUtils::getFileSize(std::string_view filepath) const
{
    const std::string vfsPath = toVfsPath(filepath);
    if (!vfsPath.empty() && m_fs->isFile(vfsPath))
        return m_fs->fileSize(vfsPath);
    return m_native->getFileSize(filepath);
}

std::string VfsFileUtils::getWritablePath() const
{
    return _writablePath.empty() ? std::string{"/user/"} : _writablePath;
}

std::string VfsFileUtils::getNativeWritableAbsolutePath() const
{
    return m_nativeWritable.empty() ? m_native->getNativeWritableAbsolutePath() : m_nativeWritable;
}

std::string VfsFileUtils::fullPathForFilename(std::string_view filename) const
{
    if (filename.empty())
        return {};

    const std::string normalized = normalizeSlashes(filename);
    if (looksNativeAbsolute(normalized))
        return m_native->fullPathForFilename(filename);

    const std::string vfsPath = toVfsPath(filename);
    if (!vfsPath.empty() && m_fs->isFile(vfsPath))
        return vfsPath;

    return m_native->fullPathForFilename(filename);
}

bool VfsFileUtils::isFileExistInternal(std::string_view filename) const
{
    const std::string vfsPath = toVfsPath(filename);
    if (!vfsPath.empty() && m_fs->isFile(vfsPath))
        return true;
    // VFS 未命中（含 native/ 经 toVfsPath 返回空）：回落 m_native，不按项目目录前缀提前 false
    return m_native->isFileExistInternal(filename);
}

bool VfsFileUtils::isDirectoryExistInternal(std::string_view dirPath) const
{
    const std::string vfsPath = toVfsPath(dirPath);
    if (!vfsPath.empty() && m_fs->isDir(vfsPath))
        return true;
    return m_native->isDirectoryExistInternal(dirPath);
}

std::vector<std::string> VfsFileUtils::listFiles(std::string_view dirPath) const
{
    const std::string vfsPath = toVfsPath(dirPath);
    if (!vfsPath.empty() && m_fs->isDir(vfsPath))
    {
        std::vector<std::string> files;
        std::string dir = vfsPath;
        if (!dir.empty() && dir.back() != '/')
            dir += '/';
        for (const auto& entry : m_fs->list(dir))
            files.push_back(dir + entry.name + (entry.type == vfs::FileType::DIR ? "/" : ""));
        return files;
    }
    return m_native->listFiles(dirPath);
}

void VfsFileUtils::listFilesRecursively(std::string_view dirPath, std::vector<std::string>* files) const
{
    if (!files)
        return;
    const std::string vfsPath = toVfsPath(dirPath);
    if (!vfsPath.empty() && m_fs->isDir(vfsPath))
    {
        m_fs->walk(vfsPath, [files](const std::string& fullPath, const vfs::DirEntry& entry) {
            files->push_back(entry.type == vfs::FileType::DIR ? fullPath + "/" : fullPath);
            return true;
        });
        return;
    }
    m_native->listFilesRecursively(dirPath, files);
}

bool VfsFileUtils::createDirectories(std::string_view dirPath) const
{
    const std::string vfsPath = toVfsPath(dirPath);
    if (!vfsPath.empty())
    {
        if (m_fs->createDirs(vfsPath) == vfs::Error::OK)
            return true;
    }
    return m_native->createDirectories(dirPath);
}

bool VfsFileUtils::removeDirectory(std::string_view dirPath) const
{
    const std::string vfsPath = toVfsPath(dirPath);
    if (!vfsPath.empty())
    {
        if (m_fs->removeDir(vfsPath, true) == vfs::Error::OK)
            return true;
    }
    return m_native->removeDirectory(dirPath);
}

bool VfsFileUtils::removeFile(std::string_view filepath) const
{
    const std::string vfsPath = toVfsPath(filepath);
    if (!vfsPath.empty())
    {
        if (m_fs->removeFile(vfsPath) == vfs::Error::OK)
            return true;
    }
    return m_native->removeFile(filepath);
}

bool VfsFileUtils::renameFile(std::string_view oldfullpath, std::string_view newfullpath) const
{
    const std::string oldVfs = toVfsPath(oldfullpath);
    const std::string newVfs = toVfsPath(newfullpath);
    if (!oldVfs.empty() && !newVfs.empty())
    {
        if (m_fs->moveFile(oldVfs, newVfs) == vfs::Error::OK)
            return true;
    }
    return m_native->renameFile(oldfullpath, newfullpath);
}

bool VfsFileUtils::renameFile(std::string_view path, std::string_view oldname, std::string_view name) const
{
    std::string base(path);
    if (!base.empty() && base.back() != '/')
        base += '/';
    return renameFile(base + std::string(oldname), base + std::string(name));
}
