param(
    [string]$Revision = '',
    [string]$OutDir = ''
)

$ErrorActionPreference = 'Stop'
$root       = Split-Path -Parent $MyInvocation.MyCommand.Path
$clientRoot = Split-Path -Parent $root
$cli        = Join-Path $root 'build/mugen_sim_cli'
$replayDir  = Join-Path $root 'data/replays'
$scenarioDir = Join-Path $root 'data/scenarios'
$traceDir   = if ($OutDir) { $OutDir } else { Join-Path $replayDir 'traces' }

if (-not (Test-Path -LiteralPath $cli)) {
    Write-Error "mugen_sim_cli not found; build it first (Release)"
    exit 1
}
if (-not $Revision) {
    $Revision = (& git -C $clientRoot rev-parse --short HEAD).Trim()
}
if (-not (Test-Path -LiteralPath $traceDir)) {
    New-Item -ItemType Directory -Force -Path $traceDir | Out-Null
}

# 回放基线：三个 .mgrp 回放。
foreach ($name in @('arena30002', 'copy101_room2010101', 'defense2071501')) {
    & $cli replay (Join-Path $replayDir "$name.mgrp") --trace (Join-Path $traceDir "$name.trace") --revision $Revision | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Error "replay baseline failed: $name"; exit $LASTEXITCODE }
}

# 战斗场景基线：防御房间内按职业脚本驱动。
foreach ($spec in @(@('class1_swordman', 1), @('class2_ranger', 2), @('class4_mage', 4))) {
    $name = $spec[0]; $occ = $spec[1]
    $script = Join-Path $scenarioDir "$name.script"
    if (-not (Test-Path -LiteralPath $script)) { Write-Error "scenario script missing: $script"; exit 1 }
    & $cli run-room 2071501 --frames 700 --class $occ --level 1000 --script $script --trace (Join-Path $traceDir "$name.trace") --revision $Revision | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Error "scenario baseline failed: $name"; exit $LASTEXITCODE }
}
