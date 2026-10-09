# AxmolFighter-Client

Game client written in C++ on Axmol (custom fork). It uses CMake, FairyGUI, protobuf-lite, yasio and Spine 3.4 (`MG_SPINE_USE_3_4=ON`). It also contains the shared battle core `Source/mugen`; see `Source/mugen/AGENTS.md`.

## Commands (Windows)
- Build: `build.bat` (runs `axmol build -p win32`). To generate the project only: `gen_project.bat`.
- Run: `run.bat [Debug|Release]`. The exe is in `build/bin/AxmolFighter-Client/<cfg>/`, and the working directory is `Content`.
- Offline mode: add `--offline-room=<roomId>`, `--offline-role=<roleId|class>`, `--autotest-frames=N` to the command line.
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
| `Source/3rd/` | behaviac, protobuf, spine 3.4 (vendored; don't edit) |
| `Content/` | Runtime assets. `Content/mugen/config/config.bin` is generated |
