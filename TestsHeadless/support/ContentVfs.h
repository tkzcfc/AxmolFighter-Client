#pragma once

#include "mugen/core/io/ContentMount.h"
#include "mugen/core/io/FileUtils.h"
#include "vfs/Error.h"
#include "vfs/VirtualFileSystem.h"

#include <filesystem>

namespace mugen::test
{

// Mount Content at `/` and a temp dir at `/user`, then io::setVfs. Safe to call repeatedly.
inline bool ensureContentVfs()
{
    static vfs::VirtualFileSystem fs;
    static bool mounted = false;
    if (!mounted)
    {
        const auto content = std::filesystem::path(MUGEN_CONTENT_DIR);
        if (io::mountContentRoot(fs, content.string()) != vfs::Error::OK)
            return false;
        const auto userDir = std::filesystem::temp_directory_path() / "mugen_vfs_user";
        std::error_code ec;
        std::filesystem::create_directories(userDir, ec);
        if (io::mountUserWritable(fs, userDir.string()) != vfs::Error::OK)
            return false;
        mounted = true;
    }
    io::setVfs(&fs);
    return true;
}

}  // namespace mugen::test
