# vfs

Vendored runtime snapshot of VirtualFileSystem (no tools, tests, bench, or examples).

Compiled into the client, battle server, and TestsHeadless as source files — not a separate `vfs` library. `config_converter` does not compile this tree.

Include path is `Source/3rd`, so callers use `#include "vfs/VirtualFileSystem.h"`.

The client defines `VFS_WITH_AXMOL=1` so `vfs::IFileStream` is `ax::IFileStream`. Other targets leave that define off.

`3rd/lz4` and `3rd/xxhash` are the archive backends' dependencies. Do not edit this tree except when updating the vendor snapshot.
