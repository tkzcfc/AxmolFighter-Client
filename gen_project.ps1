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
$axArgs = @('build', '-p', $Platform, '-c')
if ($ExtraArgs) { $axArgs += $ExtraArgs }
Write-Host "axmol $($axArgs -join ' ')"
& axmol @axArgs
exit $LASTEXITCODE
