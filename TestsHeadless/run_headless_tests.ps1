param(
    [string]$Filter = '',
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root 'build'

if (-not $env:AX_ROOT) {
    Write-Error 'AX_ROOT is not set.'
    exit 1
}

$ninja = Get-Command ninja -ErrorAction SilentlyContinue
$configureArgs = @(
    '-S', $root,
    '-B', $buildDir,
    "-DAX_ROOT=$($env:AX_ROOT)",
    "-DCMAKE_BUILD_TYPE=$Config"
)
if ($ninja) {
    $configureArgs += @('-G', 'Ninja')
}

& cmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$buildArgs = @('--build', $buildDir, '--config', $Config, '--target', 'mugen_tests', '--parallel')
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$testName = if ($IsWindows) { 'mugen_tests.exe' } else { 'mugen_tests' }
$testExe = $null
foreach ($candidate in @(
    (Join-Path $buildDir "$Config\$testName"),
    (Join-Path $buildDir $testName)
)) {
    if (Test-Path -LiteralPath $candidate) {
        $testExe = $candidate
        break
    }
}
if (-not $testExe) {
    Write-Error "mugen_tests executable not found under $buildDir"
    exit 1
}

$testArgs = @()
if ($Filter) {
    $testArgs += "--test-case=$Filter"
}
& $testExe @testArgs
$testExit = $LASTEXITCODE
if ($testExit -ne 0) {
    exit $testExit
}
if ($Filter) {
    $matched = & $testExe @('--count', "--test-case=$Filter")
    $countLine = $matched | Where-Object { $_ -match 'unskipped test cases passing the current filters: (\d+)' } | Select-Object -Last 1
    if ($countLine -match ': (\d+)' -and [int]$Matches[1] -eq 0) {
        Write-Error "no test matched filter: $Filter"
        exit 1
    }
}
exit 0
