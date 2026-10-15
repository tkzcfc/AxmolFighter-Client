# usage: pwsh build.ps1 [-Platform win32|osx|linux|ios|android|wasm] [-Config Debug|Release] [extra axmol args...]
param(
    [string]$Platform,
    [string]$Config,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ExtraArgs
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '_platform.ps1')
Set-Location $PSScriptRoot
if (-not $Platform) { $Platform = Get-DefaultAxPlatform }
$axArgs = @('build', '-p', $Platform)
if ($Config) { $axArgs += @('-c', $Config) }
if ($ExtraArgs) { $axArgs += $ExtraArgs }
Write-Host "axmol $($axArgs -join ' ')"
& axmol @axArgs
exit $LASTEXITCODE
