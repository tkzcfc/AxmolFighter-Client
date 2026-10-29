#pragma once

#include "axmol.h"
#include "vfs/VirtualFileSystem.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// 全局 FileUtils：逻辑路径 / `...` 路径交给 VFS；
// 持有原平台 FileUtils，作为原生存储路径的回落
//（axslc、UI 工具、以及直接传入实文件系统路径的第三方模块）。
// Android：Content/native/ 下的 video/ 不进 content.vfs，MediaPlayer 需要靠原生 FileUtils 得到 file://。
// VFS 查找前按 Axmol 资源路径把正反斜杠规范斜杠并前补 `/` 再查 VFS
//（于是 mugen/xxx.png 与 /mugen/xxx.png 等价，无论假根在 content.vfs 还是 AssetManager）。
// 未命中、以及 native/ 下的原生存储路径，再回落原平台 FileUtils（用原始路径）。
// 战斗读写/配置逻辑仍通过 io::vfs() 读写；初始化时挂载。
class VfsFileUtils : public ax::FileUtils
{
public:
    /**
     * 挂载 Content 与 /user，并接管 FileUtils::getInstance()。
     * @return 新实例；失败时返回 nullptr，调用方应中止启动流程。
     *         Android：优先 content.vfs 只读挂载到 `/`，否则 AssetManager 挂 `/`；`/user` 始终挂载。
     *         可重复调用：先 destroyInstance 拆掉旧委托（含其 m_native），再新建，避免重复 addSearchPath / 泄漏。
     *         install 会在 m_native 上挂 native/ 与 axslc（与 ProgramManager 一致），因 fullPath 回落走 m_native。
     */
    static VfsFileUtils* install();

    vfs::VirtualFileSystem& vfs() const { return *m_fs; }

    ~VfsFileUtils() override;

    bool init() override;

    ax::FileUtils::Status getContents(std::string_view filename, ax::ResizableBuffer* buffer) const override;

    std::unique_ptr<ax::IFileStream> openFileStream(std::string_view filePath,
                                                    ax::IFileStream::Mode mode) const override;

    int64_t getFileSize(std::string_view filepath) const override;

    std::string getWritablePath() const override;
    std::string getNativeWritableAbsolutePath() const override;

    std::string fullPathForFilename(std::string_view filename) const override;

    bool isFileExistInternal(std::string_view filename) const override;
    bool isDirectoryExistInternal(std::string_view dirPath) const override;

    std::vector<std::string> listFiles(std::string_view dirPath) const override;
    void listFilesRecursively(std::string_view dirPath, std::vector<std::string>* files) const override;

    bool createDirectories(std::string_view dirPath) const override;
    bool removeDirectory(std::string_view dirPath) const override;
    bool removeFile(std::string_view filepath) const override;
    bool renameFile(std::string_view oldfullpath, std::string_view newfullpath) const override;
    bool renameFile(std::string_view path, std::string_view oldname, std::string_view name) const override;

private:
    VfsFileUtils(std::shared_ptr<vfs::VirtualFileSystem> fs, ax::FileUtils* native, std::string nativeWritable);

    /** 规范斜杠；可交 VFS 的逻辑路径且非原生专用路径时前补 `/`；原生专用路径返回空，表示跳过 VFS。 */
    std::string toVfsPath(std::string_view filename) const;
    ax::FileUtils::Status readVfs(const std::string& vfsPath, ax::ResizableBuffer* buffer) const;

    std::shared_ptr<vfs::VirtualFileSystem> m_fs;
    ax::FileUtils* m_native = nullptr;
    std::string m_nativeWritable;
};
