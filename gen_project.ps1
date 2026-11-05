# Generate the native project only (no compile).
# usage: pwsh gen_project.ps1 [-Platform win32|osx|linux|ios|android|wasm] [extra axmol args...]
param(
    [string]$Platform,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ExtraArgs
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_platform.ps1')
Set-Location $PSScriptRoot
if (-not $Platform) { $Platform = Get-DefaultAxPlatform }
# IMPORTANT: do not put `-c` into a string array and splat it.
# PowerShell array splatting (@('-c')) passes a plain string, so axmol/build.ps1's
# [switch]$configOnly never binds, and 1kiss proceeds to `cmake --build`.
# Passing `-c` as a real switch argument preserves configOnly=true (generate only).
Write-Host "axmol build -p $Platform -c $($ExtraArgs -join ' ')"
if ($ExtraArgs) {
    & axmol build -p $Platform -c @ExtraArgs
} else {
    & axmol build -p $Platform -c
}
exit $LASTEXITCODE
