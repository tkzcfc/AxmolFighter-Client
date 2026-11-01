# Pack Content/ into Content/content.vfs for Android APK assets.
# Prefers Tools/bin/<platform>/vfstool(.exe); else PATH / VFSTOOL.
# Usage: pwsh Tools/pack_content_vfs.ps1 [-ContentDir <path>] [-OutFile <path>]
#
# native/ is excluded: assets that must stay outside .vfs (e.g. video for MediaPlayer
# file:// paths under assets/native/...). Gradle copies Content/native into APK
# assets/native separately alongside content.vfs when CONTENT_VFS_PACK=true.
param(
    [string]$ContentDir = (Join-Path $PSScriptRoot "..\Content"),
    [string]$OutFile = (Join-Path $PSScriptRoot "..\Content\content.vfs"),
    [string]$VfsToolPath = $env:VFSTOOL,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ContentDir = (Resolve-Path $ContentDir).Path
$OutFile = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutFile)

function Get-HostBinPlatformDir {
    $arch = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString().ToLowerInvariant()
    if ($arch -eq "x64") { $arch = "x64" }
    elseif ($arch -eq "arm64") { $arch = "arm64" }
    elseif ($arch -eq "x86") { $arch = "x86" }

    if ($IsWindows -or $env:OS -match "Windows") {
        return "windows-$arch"
    }
    if ($IsMacOS) {
        return "darwin-$arch"
    }
    if ($IsLinux) {
        return "linux-$arch"
    }
    return "windows-x64"
}

function Resolve-VfsTool {
    param([string]$Explicit)
    if ($Explicit -and (Test-Path -LiteralPath $Explicit)) {
        return (Resolve-Path -LiteralPath $Explicit).Path
    }

    $platform = Get-HostBinPlatformDir
    $exeName = if ($IsWindows -or $env:OS -match "Windows") { "vfstool.exe" } else { "vfstool" }
    $bundled = Join-Path $PSScriptRoot "bin\$platform\$exeName"
    if (Test-Path -LiteralPath $bundled) {
        return (Resolve-Path -LiteralPath $bundled).Path
    }

    $cmd = Get-Command vfstool -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

$vfstoolPath = Resolve-VfsTool -Explicit $VfsToolPath
if (-not $vfstoolPath) {
    $platform = Get-HostBinPlatformDir
    $hint = Join-Path $PSScriptRoot "bin\$platform"
    Write-Host @"
vfstool not found.

Place host tools under Tools/bin/<platform>/, e.g.:
  $hint\vfstool.exe   (Windows)
  $hint\vfstool       (Linux / macOS)

Or put vfstool on PATH, or set VFSTOOL=.../vfstool(.exe).

Build from VirtualFileSystem with VFS_BUILD_TOOLS=ON, then copy into Tools/bin/<platform>/.
"@
    exit 1
}

Write-Host "Using vfstool: $vfstoolPath"

if (-not $Force -and (Test-Path -LiteralPath $OutFile)) {
    $outTime = (Get-Item -LiteralPath $OutFile).LastWriteTimeUtc
    $stale = $false
    Get-ChildItem -LiteralPath $ContentDir -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object {
        $rel = $_.FullName.Substring($ContentDir.Length).TrimStart('\', '/')
        if ($rel -eq 'content.vfs') { return }
        if ($rel -eq 'native' -or $rel.StartsWith('native\') -or $rel.StartsWith('native/')) { return }
        if ($_.LastWriteTimeUtc -gt $outTime) { $script:stale = $true }
    }
    if (-not $stale) {
        $len = (Get-Item -LiteralPath $OutFile).Length
        Write-Host "Up-to-date: $OutFile ($len bytes) — skip pack"
        exit 0
    }
}

Write-Host "Packing $ContentDir -> $OutFile"
Write-Host "NOTE: Content/native is excluded from the archive; copy it into APK assets/native separately (Gradle copy*ContentToAssets does this when CONTENT_VFS_PACK=true)."
& $vfstoolPath pack $ContentDir -o $OutFile --exclude native --exclude "native/**" --exclude content.vfs --exclude "content.vfs"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "Wrote $OutFile"
