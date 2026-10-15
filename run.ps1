# Launch the built client with Content as working directory.
# usage: pwsh run.ps1 [-Config Debug|Release|MinSizeRel|RelWithDebInfo]
param([string]$Config)
$ErrorActionPreference = 'Stop'
$AppName = 'AxmolFighter-Client'
$cacheFile = Join-Path $PSScriptRoot 'run.ps1.txt'
$content = Join-Path $PSScriptRoot 'Content'

if (-not $Config -and (Test-Path $cacheFile)) { $Config = (Get-Content $cacheFile -Raw).Trim() }
if (-not $Config) { $Config = Read-Host 'Build config (Debug,Release,MinSizeRel,RelWithDebInfo) [Debug]' }
if (-not $Config) { $Config = 'Debug' }
Set-Content -Path $cacheFile -Value $Config

$buildDirs = Get-ChildItem -Path $PSScriptRoot -Directory -Filter 'build*' | Sort-Object LastWriteTime -Descending
$exe = $null
foreach ($dir in $buildDirs) {
    $base = Join-Path $dir.FullName "bin/$AppName"
    $candidates = if ($IsWindows -or $env:OS -eq 'Windows_NT') {
        @("$base/$Config/$AppName.exe", "$base/$AppName.exe")
    } elseif ($IsMacOS) {
        @("$base/$Config/$AppName.app/Contents/MacOS/$AppName", "$base/$AppName.app/Contents/MacOS/$AppName")
    } else {
        @("$base/$Config/$AppName", "$base/$AppName")
    }
    $exe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($exe) { break }
}
if (-not $exe) { throw "Cannot find built $AppName ($Config) under build*/bin/$AppName. Run build.ps1 first." }

Write-Host "Launching $exe (cwd: $content)"
Start-Process -FilePath $exe -WorkingDirectory $content
