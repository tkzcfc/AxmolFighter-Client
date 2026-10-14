param(
    [string]$Revision = ''
)

$ErrorActionPreference = 'Stop'
$root      = Split-Path -Parent $MyInvocation.MyCommand.Path
$cli       = Join-Path $root 'build/mugen_sim_cli'
$committed = Join-Path $root 'data/replays/traces'
$tmp       = Join-Path $root 'build/trace_check'

if (-not (Test-Path -LiteralPath $cli)) {
    Write-Error "mugen_sim_cli not found; build it first (Release)"
    exit 1
}

# 在临时目录重新生成全部基线，再与已提交基线逐行对比。
Remove-Item -Recurse -Force -LiteralPath $tmp -ErrorAction SilentlyContinue
& (Join-Path $root 'run_trace_baseline.ps1') -Revision $Revision -OutDir $tmp
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$failed = $false
foreach ($file in (Get-ChildItem -LiteralPath $committed -Filter *.trace)) {
    $candidate = Join-Path $tmp $file.Name
    if (-not (Test-Path -LiteralPath $candidate)) {
        Write-Output "MISSING baseline: $($file.Name)"
        $failed = $true
        continue
    }
    & $cli trace-diff $candidate $file.FullName
    if ($LASTEXITCODE -ne 0) {
        $failed = $true
    }
    else {
        Write-Output "OK: $($file.Name)"
    }
}
if ($failed) { exit 1 }
exit 0
