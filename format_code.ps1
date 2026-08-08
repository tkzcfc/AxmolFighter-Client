param(
    [int]$Jobs = 0,
    [switch]$ListFiles
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$ignoreFiles = @(
    'Source/net/client_game.pb.h',
    'Source/net/client_game.pb.cc',
    'Source/net/client_battle.pb.h',
    'Source/net/client_battle.pb.cc',
    'Source/net/client_town.pb.h',
    'Source/net/client_town.pb.cc',
    'Source/net/game_types.pb.h',
    'Source/net/game_types.pb.cc',
    'Source/net/gateway_client.pb.h',
    'Source/net/gateway_client.pb.cc'
)

$ignoreDirs = @(
    'Source/3rd'
)

function Get-FormatJobCount {
    param([int]$Requested)

    if ($Requested -gt 0) {
        return $Requested
    }

    $envJobs = $env:FORMAT_JOBS
    if ($envJobs -match '^\d+$' -and [int]$envJobs -gt 0) {
        return [int]$envJobs
    }

    if ($env:NUMBER_OF_PROCESSORS -match '^\d+$' -and [int]$env:NUMBER_OF_PROCESSORS -gt 0) {
        return [int]$env:NUMBER_OF_PROCESSORS
    }

    return [System.Environment]::ProcessorCount
}

function Get-RelativeUnixPath {
    param(
        [string]$BasePath,
        [string]$Path
    )

    $baseFull = [System.IO.Path]::GetFullPath($BasePath)
    $pathFull = [System.IO.Path]::GetFullPath($Path)

    if (-not $baseFull.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $baseFull += [System.IO.Path]::DirectorySeparatorChar
    }

    $baseUri = [Uri]$baseFull
    $pathUri = [Uri]$pathFull
    return [Uri]::UnescapeDataString($baseUri.MakeRelativeUri($pathUri).ToString()).Replace('\', '/')
}

function Test-IgnoredPath {
    param([string]$Path)

    $relative = Get-RelativeUnixPath -BasePath $root -Path $Path

    foreach ($file in $ignoreFiles) {
        if ([string]::Equals($relative, $file, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    foreach ($dir in $ignoreDirs) {
        $prefix = $dir.TrimEnd('/') + '/'
        if ($relative.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }

    return $false
}

function Get-FilesByExtension {
    param(
        [string]$Path,
        [string[]]$Extensions
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return @()
    }

    $files = foreach ($extension in $Extensions) {
        Get-ChildItem -LiteralPath $Path -Recurse -File -Filter "*$extension" -ErrorAction SilentlyContinue
    }

    return @($files | Where-Object { -not (Test-IgnoredPath -Path $_.FullName) } | Sort-Object FullName)
}

function New-FormatItem {
    param(
        [System.IO.FileInfo]$File,
        [string]$StyleKey
    )

    [pscustomobject]@{
        Path = $File.FullName
        StyleKey = $StyleKey
    }
}

function Add-Stage {
    param(
        [string]$Title,
        [System.IO.FileInfo[]]$Files,
        [string]$StyleKey,
        [System.Collections.Generic.List[object]]$Items
    )

    Write-Host $Title
    Write-Host ("  {0} files" -f $Files.Count)

    foreach ($file in $Files) {
        if ($ListFiles) {
            Write-Host ("  {0}" -f $file.FullName)
        }
        $Items.Add((New-FormatItem -File $file -StyleKey $StyleKey)) | Out-Null
    }
}

function Write-ResponseFileLine {
    param(
        [string]$Path,
        [string]$Value
    )

    $escaped = $Value.Replace('\', '\\').Replace('"', '\"')
    Add-Content -LiteralPath $Path -Value ('"{0}"' -f $escaped)
}

function Invoke-FormatBatches {
    param(
        [object[]]$Items,
        [int]$JobCount,
        [string]$JobDir
    )

    if ($Items.Count -eq 0) {
        return @()
    }

    for ($i = 0; $i -lt $Items.Count; $i++) {
        $bucket = ($i % $JobCount) + 1
        $item = $Items[$i]
        $responseFile = Join-Path $JobDir ("q{0}_{1}.rsp" -f $bucket, $item.StyleKey)
        Write-ResponseFileLine -Path $responseFile -Value $item.Path
    }

    $workerIds = Get-ChildItem -LiteralPath $JobDir -Filter 'q*_*.rsp' |
        ForEach-Object {
            if ($_.BaseName -match '^q(\d+)_') {
                [int]$Matches[1]
            }
        } |
        Sort-Object -Unique

    $workers = foreach ($workerId in $workerIds) {
        Start-Job -Name "clang-format-$workerId" -ArgumentList $root, $JobDir, $workerId -ScriptBlock {
            param(
                [string]$Root,
                [string]$JobDir,
                [int]$WorkerId
            )

            Set-Location -LiteralPath $Root
            $failed = New-Object System.Collections.Generic.List[string]

            $styles = @(
                @{ Key = 'DEFAULT'; Args = @() },
                @{ Key = 'IOSCPP'; Args = @('--style=file:proj.ios_mac/.clang-format') },
                @{ Key = 'OBJC'; Args = @('--style=file:proj.ios_mac/.clang-format-objc') }
            )

            foreach ($style in $styles) {
                $responseFile = Join-Path $JobDir ("q{0}_{1}.rsp" -f $WorkerId, $style.Key)
                if (-not (Test-Path -LiteralPath $responseFile)) {
                    continue
                }

                $arguments = @()
                $arguments += $style.Args
                $arguments += '-i'
                $arguments += "@$responseFile"

                & clang-format @arguments
                if ($LASTEXITCODE -ne 0) {
                    foreach ($line in Get-Content -LiteralPath $responseFile) {
                        $failed.Add($line.Trim('"')) | Out-Null
                    }
                }
            }

            if ($failed.Count -gt 0) {
                $errorFile = Join-Path $JobDir ("q{0}.err" -f $WorkerId)
                Set-Content -LiteralPath $errorFile -Value $failed
            }
        }
    }

    if ($workers.Count -gt 0) {
        Wait-Job -Job $workers | Out-Null
        $jobOutput = Receive-Job -Job $workers 2>&1
        if ($jobOutput) {
            $jobOutput | ForEach-Object { Write-Host $_ }
        }
        Remove-Job -Job $workers
    }

    $errorFiles = Get-ChildItem -LiteralPath $JobDir -Filter '*.err' -ErrorAction SilentlyContinue
    if ($errorFiles) {
        return @($errorFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName })
    }

    return @()
}

$Jobs = Get-FormatJobCount -Requested $Jobs
if ($Jobs -lt 1) {
    $Jobs = 1
}

$clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue
if (-not $clangFormat) {
    throw 'clang-format was not found in PATH.'
}

$jobDir = Join-Path ([System.IO.Path]::GetTempPath()) ("format_code_{0}" -f ([Guid]::NewGuid().ToString('N')))
New-Item -ItemType Directory -Path $jobDir | Out-Null

try {
    Push-Location -LiteralPath $root

    Write-Host '========================================'
    Write-Host '       Code Formatting Tool'
    Write-Host '========================================'
    Write-Host ''
    Write-Host ("[Config] Ignored files : {0}" -f ($ignoreFiles -join ' '))
    Write-Host ("[Config] Ignored dirs  : {0}" -f ($ignoreDirs -join ' '))
    Write-Host ("[Config] Parallel jobs : {0}" -f $Jobs)
    Write-Host ''

    $items = New-Object System.Collections.Generic.List[object]

    $sourceFiles = Get-FilesByExtension -Path (Join-Path $root 'Source') -Extensions @('.cpp', '.h', '.mm')
    Add-Stage -Title '[1/3] Formatting Source ...' -Files $sourceFiles -StyleKey 'DEFAULT' -Items $items

    $iosCppFiles = Get-FilesByExtension -Path (Join-Path $root 'proj.ios_mac') -Extensions @('.cpp', '.h')
    Add-Stage -Title '[2/3] Formatting proj.ios_mac (*.cpp, *.h) ...' -Files $iosCppFiles -StyleKey 'IOSCPP' -Items $items

    $objcFiles = Get-FilesByExtension -Path (Join-Path $root 'proj.ios_mac') -Extensions @('.mm')
    Add-Stage -Title '[3/3] Formatting proj.ios_mac (*.mm) ...' -Files $objcFiles -StyleKey 'OBJC' -Items $items

    Write-Host ''
    $failedFiles = Invoke-FormatBatches -Items $items.ToArray() -JobCount $Jobs -JobDir $jobDir

    $stopwatch.Stop()
    Write-Host ("[Time] Elapsed: {0:hh\:mm\:ss\.fff}" -f $stopwatch.Elapsed)
    Write-Host ''
    Write-Host '========================================'

    if ($failedFiles.Count -gt 0) {
        Write-Host '       Formatting completed with errors!'
        Write-Host '========================================'
        Write-Host 'Failed files:'
        $failedFiles | Sort-Object -Unique | ForEach-Object { Write-Host $_ }
        exit 1
    }

    Write-Host '       Formatting complete!'
    Write-Host '========================================'
}
finally {
    Pop-Location
    Remove-Item -LiteralPath $jobDir -Recurse -Force -ErrorAction SilentlyContinue
}
