# Pack Content/ into Content/content.vfs for Android APK assets.
# Requires `vfstool` on PATH (build from VirtualFileSystem with VFS_BUILD_TOOLS=ON).
# Usage: pwsh Tools/pack_content_vfs.ps1 [-ContentDir <path>] [-OutFile <path>]
#
# native/ is excluded: assets that must stay outside .vfs (e.g. video for MediaPlayer
# file:// paths under assets/native/...). Gradle copies Content/native into APK
# assets/native separately alongside content.vfs when CONTENT_VFS_PACK=true.
param(
    [string]$ContentDir = (Join-Path $PSScriptRoot "..\Content"),
    [string]$OutFile = (Join-Path $PSScriptRoot "..\Content\content.vfs")
)

$ErrorActionPreference = "Stop"
$ContentDir = (Resolve-Path $ContentDir).Path
$OutFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutFile)

$vfstool = Get-Command vfstool -ErrorAction SilentlyContinue
if (-not $vfstool) {
    Write-Host @"
vfstool not found on PATH.

Build it from the VirtualFileSystem repo:
  cmake -S <vfs-src> -B <vfs-build> -DVFS_BUILD_TOOLS=ON
  cmake --build <vfs-build> --target vfstool
  # then add the output directory to PATH

TODO(CI): invoke this script (or an equivalent CMake custom target) before
proj.android merge*Assets so the APK contains assets/content.vfs uncompressed
(see aaptOptions.noCompress 'vfs').
"@
    exit 1
}

Write-Host "Packing $ContentDir -> $OutFile"
Write-Host "NOTE: Content/native is excluded from the archive; copy it into APK assets/native separately (Gradle copy*ContentToAssets does this when CONTENT_VFS_PACK=true)."
& $vfstool.Source pack $ContentDir -o $OutFile --exclude native --exclude "native/**" --exclude content.vfs --exclude "content.vfs"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Wrote $OutFile"
