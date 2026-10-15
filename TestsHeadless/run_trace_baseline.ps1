param(
    [string]$Revision = '',
    [string]$OutDir = '',
    [ValidateSet('full', 'effects')]
    [string]$Mode = 'full',
    [switch]$AiOff
)

$ErrorActionPreference = 'Stop'
$root       = Split-Path -Parent $MyInvocation.MyCommand.Path
$clientRoot = Split-Path -Parent $root
$cli        = Join-Path $root 'build/mugen_sim_cli'
$replayDir  = Join-Path $root 'data/replays'
$scenarioDir = Join-Path $root 'data/scenarios'
$traceName  = if ($Mode -eq 'effects') { if ($AiOff) { 'effects' } else { 'effects_ai' } } else { 'traces' }
$traceDir   = if ($OutDir) { $OutDir } else { Join-Path $replayDir $traceName }

if (-not (Test-Path -LiteralPath $cli)) {
    Write-Error "mugen_sim_cli not found; build it first (Release)"
    exit 1
}
if (-not $Revision) {
    # 基线头固定记录产生 trace 的 mugen 代码提交；随 HEAD 漂移会破坏字节一致性。
    # AI 关闭与完整基线来自 c9321cf；AI 开启的 effects 基线来自 c238bc1（怪物 AI 切换为规则表）。
    $Revision = if ($Mode -eq 'effects' -and -not $AiOff) { 'c238bc1' } else { 'c9321cf' }
}
if (-not (Test-Path -LiteralPath $traceDir)) {
    New-Item -ItemType Directory -Force -Path $traceDir | Out-Null
}
$ai = @()
if ($AiOff) { $ai = @('--ai', 'off') }

# 回放基线：三个 .mgrp 回放。
foreach ($name in @('arena30002', 'copy101_room2010101', 'defense2071501')) {
    & $cli replay (Join-Path $replayDir "$name.mgrp") @ai --trace (Join-Path $traceDir "$name.trace") --revision $Revision | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Error "replay baseline failed: $name"; exit $LASTEXITCODE }
}

# 战斗场景基线：按职业脚本驱动（防御房），另加副本/竞技场房间。
foreach ($spec in @(
    @('class1_swordman', 1, 2071501),
    @('class2_ranger', 2, 2071501),
    @('class3_fighter', 3, 2071501),
    @('class4_mage', 4, 2071501),
    @('copy_room_class1', 1, 2010101),
    @('arena_room_class1', 1, 30002)
)) {
    $name = $spec[0]; $occ = $spec[1]; $room = $spec[2]
    $script = Join-Path $scenarioDir "$name.script"
    if (-not (Test-Path -LiteralPath $script)) { Write-Error "scenario script missing: $script"; exit 1 }
    & $cli run-room $room --frames 700 --class $occ --level 1000 --script $script @ai --trace (Join-Path $traceDir "$name.trace") --revision $Revision | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Error "scenario baseline failed: $name"; exit $LASTEXITCODE }
}
