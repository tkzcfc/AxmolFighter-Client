# Packing Content as `content.vfs` (Android)

## Why

On Android, APK `assets/` is not a real filesystem directory. The client mounts either:

1. `assets/content.vfs` via `ArchiveBackend` (preferred), or
2. the whole AssetManager tree via `AndroidAssetBackend` (dev fallback).

`.vfs` must stay **uncompressed** in the APK (`aaptOptions.noCompress` includes `vfs`).

**Native vs VFS assets:** Anything under `Content/native/` must stay outside `.vfs` and go through the platform `FileUtils` (`m_native`). Video is the main case — Axmol `MediaPlayer` opens `file://` + `fullPathForFilename`, which needs a real AssetManager / OS path. Therefore `native/` is **not** packed into `content.vfs` and ships as loose `assets/native/...` (videos live at `native/video/...`). Audio can live inside `.vfs` (game `FileUtils` / VFS streams it).

**Game path convention:** `VfsFileUtils::install()` adds search path `native` on the platform FileUtils, so call sites keep using paths like `video/xxx.mp4` (resolved under `Content/native` / `assets/native`). Paths that start with `native/` skip VFS and go straight to `m_native`. Do not mount `native` into the VFS root.

## Pack (manual)

```powershell
pwsh Tools/pack_content_vfs.ps1
```

Requires `vfstool` on PATH. The pack excludes `native/` (and `native/**`) plus any nested `content.vfs`. After packing, `Content/native` must still be copied into APK `assets/native` (Gradle does this when packing is enabled).

## Gradle switch (`CONTENT_VFS_PACK`)

In `proj.android/gradle.properties`:

```
CONTENT_VFS_PACK=false
```

| Value | `copy*ContentToAssets` behavior |
|---|---|
| `true` | Runs `Tools/pack_content_vfs.ps1` (fails the task if `vfstool` is missing or pack fails), copies `Content/content.vfs` into `build/assets/`, and copies `Content/native` into `build/assets/native` when that directory exists. |
| `false` (default) | Does **not** run the pack script. Copies the whole loose `Content/` tree into `build/assets` (excludes `**/*.gz` and `content.vfs`; includes `native/`). |

`axslc` is always copied afterward either way.

## Runtime mount (already in `VfsFileUtils::install`)

- Try `assets->archiveSource("content.vfs")` then `mount("/", ArchiveBackend)`.
- Else `mount("/", AndroidAssetBackend)`.
- Always `mountNative("/user", getNativeWritableAbsolutePath(), WriteTarget)`.
- `m_native->addSearchPath("native")` so `video/...` resolves under `assets/native` / `Content/native`.
- Paths under `native/` are not forced through VFS; fall through to native FileUtils for real `file://` paths.
