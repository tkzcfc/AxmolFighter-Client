param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',
    [string]$OutDir = ''
)

# 用旧实现（mugen_legacy_cli）按场景清单重新生成全部效果摘要 golden（附录 A）。
$ErrorActionPreference = 'Stop'
$root     = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root 'build'
$goldenDir = Join-Path $root 'data/golden_summaries'
$manifest = Join-Path $goldenDir 'scenarios.txt'
$outDir   = if ($OutDir) { $OutDir } else { $goldenDir }

if (-not $env:AX_ROOT) {
    Write-Error 'AX_ROOT is not set.'
    exit 1
}

$ninja = Get-Command ninja -ErrorAction SilentlyContinue
$configureArgs = @('-S', $root, '-B', $buildDir, "-DAX_ROOT=$($env:AX_ROOT)", "-DCMAKE_BUILD_TYPE=$Config")
if ($ninja) { $configureArgs += @('-G', 'Ninja') }
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& cmake --build $buildDir --config $Config --target mugen_legacy_cli --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$cliName = if ($IsWindows) { 'mugen_legacy_cli.exe' } else { 'mugen_legacy_cli' }
$cli = $null
foreach ($candidate in @((Join-Path $buildDir "$Config/$cliName"), (Join-Path $buildDir $cliName))) {
    if (Test-Path -LiteralPath $candidate) { $cli = $candidate; break }
}
if (-not $cli) { Write-Error "mugen_legacy_cli not found under $buildDir"; exit 1 }

if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}
& $cli export-summary --all --manifest $manifest --out-dir $outDir
if ($LASTEXITCODE -ne 0) { Write-Error 'export-summary failed'; exit $LASTEXITCODE }
Write-Output "summaries written to $outDir"
