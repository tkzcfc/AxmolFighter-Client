#pragma once

#include "FileUtils.h"
#include "vfs/Error.h"
#include "vfs/VirtualFileSystem.h"

#include <string>
#include <string_view>

NS_MG_BEGIN

namespace io
{

// Normalize a native directory path to UTF-8 with '/' separators and no trailing slash.
inline std::string normalizeNativeRoot(std::string path)
{
    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }
    while (!path.empty() && path.back() == '/')
        path.pop_back();
    return path;
}

inline bool hasMountPoint(const vfs::VirtualFileSystem& fs, std::string_view point)
{
    for (const auto& mount : fs.mounts())
    {
        if (mount.mountPoint == point)
            return true;
        // mountNative("/user") stores as "/user"; accept a trailing-slash spelling too.
        if (!point.empty() && point.back() == '/' && mount.mountPoint == point.substr(0, point.size() - 1))
            return true;
        if (!mount.mountPoint.empty() && mount.mountPoint.back() != '/' &&
            std::string(mount.mountPoint) + "/" == point)
            return true;
    }
    return false;
}

/**
 * Mount the read-only Content directory at virtual `/` via a native backend.
 * Desktop (Win32/Linux/macOS) only — never call this with Android's "assets/" root.
 * @return vfs::Error::OK when mounted or already present.
 */
inline vfs::Error mountContentRoot(vfs::VirtualFileSystem& fs, std::string contentRoot)
{
    if (hasMountPoint(fs, "/"))
        return vfs::Error::OK;

    contentRoot = normalizeNativeRoot(std::move(contentRoot));
    if (contentRoot.empty())
        return vfs::Error::INVALID_PATH;

    vfs::NativeOptions nativeOpts;
    nativeOpts.readOnly   = true;
    nativeOpts.createRoot = false;
#if !defined(NDEBUG)
    nativeOpts.strictCase = true;
#endif
    const vfs::MountOptions mountOpts{0, vfs::MountFlags::ReadOnly};
    vfs::Error err{};
    if (fs.mountNative("/", contentRoot, mountOpts, nativeOpts, &err) == 0)
        return err != vfs::Error::OK ? err : vfs::Error::IO_ERROR;
    return vfs::Error::OK;
}

/**
 * Mount the native writable directory at virtual `/user` as the WriteTarget.
 * @return vfs::Error::OK when mounted or already present.
 */
inline vfs::Error mountUserWritable(vfs::VirtualFileSystem& fs, std::string nativeWritable)
{
    if (hasMountPoint(fs, "/user") || hasMountPoint(fs, "/user/"))
        return vfs::Error::OK;

    nativeWritable = normalizeNativeRoot(std::move(nativeWritable));
    if (nativeWritable.empty())
        return vfs::Error::INVALID_PATH;

    vfs::NativeOptions nativeOpts;
    nativeOpts.readOnly   = false;
    nativeOpts.createRoot = true;
    const vfs::MountOptions mountOpts{0, vfs::MountFlags::WriteTarget};
    vfs::Error err{};
    if (fs.mountNative("/user", nativeWritable, mountOpts, nativeOpts, &err) == 0)
        return err != vfs::Error::OK ? err : vfs::Error::IO_ERROR;
    return vfs::Error::OK;
}

}  // namespace io

NS_MG_END
