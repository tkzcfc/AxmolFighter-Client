# usage: pwsh build.ps1 [-Platform win32|osx|linux|ios|android|wasm] [-Config Debug|MinSizeRel|RelWithDebInfo|Release] [extra axmol args...]
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
if ($Config) {
    # axmol's -c means "configure only"; the build config is selected with -O <index>.
    $configs = @('Debug', 'MinSizeRel', 'RelWithDebInfo', 'Release')
    $index = [Array]::FindIndex($configs, [Predicate[string]] { param($c) $c -ieq $Config })
    if ($index -lt 0) { throw "Unknown -Config '$Config'. Use one of: $($configs -join ', ')" }
    $axArgs += @('-O', "$index")
}
if ($ExtraArgs) { $axArgs += $ExtraArgs }
Write-Host "axmol $($axArgs -join ' ')"
& axmol @axArgs
exit $LASTEXITCODE
