# AxmolFighter-Client

Game client written in C++ on Axmol (custom fork). It uses CMake, FairyGUI, protobuf-lite, yasio and Spine 3.4 (`MG_SPINE_USE_3_4=ON`). It also contains the shared battle core `Source/mugen`; see `Source/mugen/AGENTS.md`.

## Commands (Windows)
- Build: `pwsh build.ps1 [-Platform win32|osx|linux] [-Config Debug|Release]` (runs `axmol build`; platform defaults to the host OS). To generate the project only: `pwsh gen_project.ps1 [-Platform ...]`.
- Run: `pwsh run.ps1 [-Config Debug|Release]`. The binary is in `build*/bin/AxmolFighter-Client/<cfg>/` (`.exe` on Windows, `.app` on macOS), and the working directory is `Content`.
- Offline mode: add `--offline-room=<roomId>`, `--offline-role=<roleId|class>`, `--autotest-frames=N` to the command line (Windows/Linux/macOS; parsed by `Source/LaunchOptions`). `run.ps1` forwards extra arguments, e.g. `pwsh run.ps1 -Config Debug --offline-room=<roomId>`.
- Tests: `axmol build -p win32 -xc -D_AX_TESTS=ON` (doctest, in `Tests/`).
- Headless tests: `pwsh TestsHeadless/run_headless_tests.ps1 [-Filter <doctest-filter>] [-Config Release|Debug]`.
- Check rules: `pwsh TestsHeadless/check_rules.ps1 [-Paths core,conf,avatar,sim]`.
- Sim CLI: `mugen_sim_cli replay <file.mgrp> [--compare <hash-file>]`, `mugen_sim_cli run-room <roomId> [--frames N]`, `mugen_sim_cli record-room <roomId> --frames N --output <file.mgrp>`.
- Format: `format_code.ps1` (clang-format). It skips `Source/3rd` and the generated `*.pb.*` files.
- Sources are collected with GLOB `CONFIGURE_DEPENDS`, so **re-run configure after adding files**.

## Layout
| Path | Contents |
|---|---|
| `Source/AppDelegate.cpp`, `MainScene.cpp`, `AppContext.cpp` | Entry points |
| `Source/net/` | yasio networking and **generated** `.pb.*` files (don't hand-edit; regenerate with Tools/protoc) |
| `Source/ui/` | FairyGUI code, `gameui` namespace (avoids clashing with `ax::ui`) |
| `Source/mugen/` | Battle core shared with the server |
| `Source/resource/` | Generic async resource loader, `gameres` namespace; see the "资源加载器" section in the root README.md |
| `Source/3rd/` | protobuf、spine 3.4、vfs（vendored 运行时，编进客户端/战斗服/TestsHeadless；客户端 `VFS_WITH_AXMOL=1`）。不要改 3rd 源码 |
| `Content/` | Runtime assets. `Content/mugen/config/config.bin` is generated. 启动时把 `Content` 挂到 VFS 虚根 `/`，`VfsFileUtils` 接管引擎 FileUtils；配置与业务 IO 写 `/mugen/...`。axslc 仍走原生搜索路径。  `Content/native/` stays outside `.vfs` (e.g. video). |


## VFS mount topology

| Platform | Mount | Backend | Flags |
|---|---|---|---|
| Desktop (Win/Linux/macOS) | `/` | `mountNative` → real Content root (`getDefaultResourceRootPath()`) | ReadOnly |
| Android | `/` | `assets/content.vfs` archive if present, else `AndroidAssetBackend` (AssetManager). **Never** `mountNative("/", "assets/")` | ReadOnly |
| All | `/user` | `mountNative` → `getNativeWritableAbsolutePath()` | WriteTarget |

`VfsFileUtils::install()` sets `_defaultResRootPath = "/"`, `_writablePath = "/user/"`, and `getNativeWritableAbsolutePath()` remains the real OS writable directory for modules that bypass FileUtils (audio `realPath`, etc.). VFS resolution does **not** walk Axmol search paths: non-absolute paths are slash-normalized and prefixed with `/` before VFS lookup (e.g. `mugen/xxx.png` → `/mugen/xxx.png`), so relative keys work whether `/` is `content.vfs` or Android AssetManager. Absolute `/mugen/...` keys still work. Paths under `native/` skip VFS and use platform FileUtils; `install()` also `addSearchPath("native")` on `m_native` so game code can keep `video/xxx.mp4`. On other VFS misses (and native absolute paths), fall back to native FileUtils with the **original** path. Shared code keeps using `io::vfs()`. The deleted `res_zhcn` tree must not be re-added as a search path.
Android packaging: with `CONTENT_VFS_PACK=true` (`proj.android/gradle.properties`), most of `Content/` is packed into `assets/content.vfs`; `Content/native/` (e.g. `native/video/`) is copied loose to `assets/native` because MediaPlayer needs native `file://` paths. Audio may stay inside the `.vfs`. See `Tools/pack_content_vfs.md`.
