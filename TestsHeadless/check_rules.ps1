param(
    [string]$Paths = ''
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$clientRoot = Split-Path -Parent $scriptRoot
$sourceRoot = Join-Path $clientRoot 'Source'
$mugenRoot = Join-Path $sourceRoot 'mugen'
$simRoot = Join-Path $mugenRoot 'sim'
$renderRoot = Join-Path $mugenRoot 'render'
$uiRoot = Join-Path $sourceRoot 'ui'
$systemsHeader = Join-Path $simRoot 'Systems.h'

# English product name is held here and never printed. A hit is reported as the generic rule name.
$script:ProductNamePattern = 'heiyue|黑月|BlackMoon'
$script:DrivePathPattern = '(?<![A-Za-z0-9_])[A-Za-z]:[\\/][^\s''"`]*'
$script:PlanDocPattern = '(?i)(Reference[/\\]|mugen-rewrite|\.omo\b)'

$script:Violations = [System.Collections.Generic.List[string]]::new()

# This checker evaluates branches as the headless target compiles them.
# RUNTIME_IN_AXMOL=0 is undefined by StdC.h before the engine include branch;
# OLUA_AUTOCONF is not supplied by the standalone headless project.
$script:HeadlessDefinedMacros = @{
    RUNTIME_IN_AXMOL = $false
    OLUA_AUTOCONF = $false
}

function Add-Violation {
    param(
        [string]$File,
        [int]$Line,
        [string]$Rule
    )
    $relative = $File
    if ($File.StartsWith($clientRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relative = $File.Substring($clientRoot.Length).TrimStart('\', '/')
    }
    $script:Violations.Add("${relative}:${Line}: ${Rule}")
}

function Get-ScopeParts {
    if (-not $Paths) {
        return @()
    }
    $names = @()
    foreach ($part in ($Paths -split ',')) {
        $name = $part.Trim()
        if ($name) {
            $names += $name
        }
    }
    return ,$names
}

function Get-ScopeRoots {
    $parts = Get-ScopeParts
    if ($parts.Count -eq 0) {
        return @($sourceRoot)
    }
    $roots = @()
    foreach ($name in $parts) {
        $candidate = Join-Path $mugenRoot $name
        if (-not (Test-Path -LiteralPath $candidate)) {
            $candidate = Join-Path $sourceRoot $name
        }
        if (-not (Test-Path -LiteralPath $candidate)) {
            Write-Error "check scope not found: $name"
            exit 1
        }
        $roots += $candidate
    }
    return $roots
}

function Get-SourceFiles {
    param([string[]]$Roots)
    $files = @()
    foreach ($root in $Roots) {
        if (Test-Path -LiteralPath $root -PathType Leaf) {
            $files += Get-Item -LiteralPath $root
            continue
        }
        $files += Get-ChildItem -LiteralPath $root -Recurse -File -Include *.h, *.hpp, *.cpp, *.c, *.cc |
            Where-Object { $_.FullName -notmatch '[\\/]3rd[\\/]' }
    }
    return $files
}

function Add-UniqueFiles {
    param(
        [System.Collections.Generic.List[System.IO.FileInfo]]$Files,
        [System.IO.FileInfo[]]$Extra
    )
    $seen = @{}
    foreach ($file in $Files) {
        $seen[$file.FullName] = $true
    }
    foreach ($file in $Extra) {
        if (-not $seen.ContainsKey($file.FullName)) {
            $Files.Add($file)
            $seen[$file.FullName] = $true
        }
    }
}

function Test-SimInScope {
    $parts = Get-ScopeParts
    if ($parts.Count -eq 0) {
        return $true
    }
    return ($parts -contains 'sim') -or ($parts -contains 'render') -or ($parts -contains 'ui')
}

function Strip-Comments {
    param([string]$Text)
    $normalized = $Text.Replace("`r`n", "`n").Replace("`r", "`n")
    $withoutBlock = [regex]::Replace($normalized, '(?s)/\*.*?\*/', {
            param($match)
            return (' ' * $match.Value.Length)
        })
    $lines = $withoutBlock.Split([char]10)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $lines[$i] = [regex]::Replace($lines[$i], '//.*$', '')
    }
    return ,$lines
}

function Get-ActiveLines {
    param([string[]]$Lines)
    $active = New-Object bool[] $Lines.Count
    $region = New-Object System.Collections.Generic.Stack[bool]
    $taken = New-Object System.Collections.Generic.Stack[bool]
    $region.Push($true)
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $trimmed = $Lines[$i].Trim()
        if ($trimmed -match '^#\s*endif\b') {
            if ($region.Count -gt 1) {
                [void]$region.Pop()
                [void]$taken.Pop()
            }
        }
        elseif ($trimmed -match '^#\s*(ifdef|ifndef|if)\b') {
            $parent = $region.Peek()
            $arm = $false
            if ($parent -and (Test-Directive $trimmed)) { $arm = $true }
            $region.Push($arm)
            $taken.Push($arm)
        }
        elseif ($trimmed -match '^#\s*(elif|else)\b') {
            $parentActive = $true
            if ($region.Count -gt 1) {
                [void]$region.Pop()
                $already = $taken.Pop()
                $parentActive = $region.Peek()
            }
            else {
                $already = $false
            }
            $arm = $false
            if ($parentActive -and -not $already) {
                if ($trimmed -match '^#\s*else\b') {
                    $arm = $true
                }
                else {
                    $arm = [bool](Test-Directive $trimmed)
                }
            }
            $region.Push($arm)
            $taken.Push($already -or $arm)
        }
        $active[$i] = $region.Peek()
    }
    return ,$active
}

function Test-Directive {
    param([string]$Line)
    $expr = $Line -replace '^#\s*(if|ifdef|ifndef|elif)\b', ''
    $expr = $expr.Trim()
    if ($Line -match '^#\s*ifndef\b') {
        if (Test-MacroDefined $expr) { return $false }
        return $true
    }
    if ($Line -match '^#\s*ifdef\b') {
        if (Test-MacroDefined $expr) { return $true }
        return $false
    }
    $defined = [regex]::Replace($expr, 'defined\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)', {
            param($match)
            if (Test-MacroDefined $match.Groups[1].Value) { '1' } else { '0' }
        })
    $defined = [regex]::Replace($defined, 'defined\s+([A-Za-z_][A-Za-z0-9_]*)', {
            param($match)
            if (Test-MacroDefined $match.Groups[1].Value) { '1' } else { '0' }
        })
    $defined = [regex]::Replace($defined, '\b([A-Za-z_][A-Za-z0-9_]*)\b', {
            param($match)
            $name = $match.Groups[1].Value
            if ($name -eq 'defined') { return $name }
            if (Test-MacroDefined $name) { '1' } else { '0' }
        })
    # 比较运算符必须优先于裸 > < 转换，且先于 ! 的替换，避免把 > 误作 PowerShell 重定向。
    $defined = $defined -replace '>=', '-ge' -replace '<=', '-le' -replace '!=', '-ne' -replace '==', '-eq' `
                        -replace '>', '-gt' -replace '<', '-lt'
    $defined = $defined -replace '&&', '-and' -replace '\|\|', '-or' -replace '!', '-not '
    try {
        return [bool](Invoke-Expression $defined)
    }
    catch {
        return $true
    }
}

function Test-MacroDefined {
    param([string]$Name)
    if ($script:HeadlessDefinedMacros.ContainsKey($Name)) {
        return [bool]$script:HeadlessDefinedMacros[$Name]
    }
    return $false
}

function Get-SimEntrypoints {
    $names = [System.Collections.Generic.List[string]]::new()
    if (-not (Test-Path -LiteralPath $systemsHeader)) {
        return ,$names
    }
    foreach ($line in [System.IO.File]::ReadAllLines($systemsHeader)) {
        if ($line -match '\bvoid\s+(run[A-Za-z0-9_]+)\s*\(\s*World') {
            $names.Add($Matches[1])
        }
    }
    return ,$names
}

function Test-CommentCitations {
    param(
        [System.IO.FileInfo]$File,
        [string[]]$RawLines
    )
    $full = $File.FullName.Replace('\', '/')
    if ($full -notmatch '/mugen/') {
        return
    }
    for ($i = 0; $i -lt $RawLines.Count; $i++) {
        $line = $RawLines[$i]
        $lineNo = $i + 1
        if ($line -match $script:ProductNamePattern) {
            Add-Violation $File.FullName $lineNo 'product name'
        }
        if ($line -match $script:DrivePathPattern) {
            Add-Violation $File.FullName $lineNo 'drive-letter path'
        }
        if ($line -match $script:PlanDocPattern) {
            Add-Violation $File.FullName $lineNo 'plan-document path'
        }
    }
}

function Test-ForbiddenTokens {
    param(
        [System.IO.FileInfo]$File,
        [string[]]$Lines
    )
    $full = $File.FullName.Replace('\', '/')
    $inCoreOrSim = $full -match '/mugen/(core|sim)/'
    $checkDeterminismTokens = $inCoreOrSim
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        $lineNo = $i + 1
        if ($checkDeterminismTokens) {
            if ($line -match '^\s*#\s*include\s*[<"][^>"]*(axmol|spine|imgui)') {
                Add-Violation $File.FullName $lineNo 'core/sim include axmol|spine|imgui'
            }
            if ($line -match '(^|[^A-Za-z0-9_])(float|double)([^A-Za-z0-9_]|$)' -and
                $line -notmatch '\btoFloatForRender\s*\(') {
                Add-Violation $File.FullName $lineNo 'core/sim float|double'
            }
            if ($line -match 'std::function') {
                Add-Violation $File.FullName $lineNo 'core/sim std::function'
            }
            if ($line -match 'unordered_map|unordered_set') {
                Add-Violation $File.FullName $lineNo 'core/sim unordered_map|unordered_set'
            }
        }
        if ($line -match 'MG_LOG_[DIWE]\(' -and $line -match '[\u4e00-\u9fff]') {
            Add-Violation $File.FullName $lineNo 'log macro argument contains Chinese'
        }
    }
}

function Test-SimSystemIsolation {
    param(
        [System.IO.FileInfo]$File,
        [string[]]$Lines,
        [string[]]$Entrypoints
    )
    $full = $File.FullName.Replace('\', '/')
    if ($full -notmatch '/mugen/sim/.+System\.cpp$') {
        return
    }
    $definedHere = @{}
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match '\bvoid\s+(run[A-Za-z0-9_]+)\s*\(\s*World') {
            $definedHere[$Matches[1]] = $true
        }
    }
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        $lineNo = $i + 1
        if ($line -match '^\s*#\s*include\s*[<"]([^>"]+)[>"]') {
            $inc = $Matches[1].Replace('\', '/')
            $leaf = Split-Path $inc -Leaf
            if ($leaf -match '^[A-Za-z0-9_]*System\.(h|hpp|cpp)$' -and $leaf -ne 'Systems.h') {
                Add-Violation $File.FullName $lineNo 'sim system include of *System.h/*System.cpp'
            }
        }
        foreach ($name in $Entrypoints) {
            if ($definedHere.ContainsKey($name)) {
                continue
            }
            if ($line -match "(?:^|[^A-Za-z0-9_])(?:mugen::sim::)?${name}\s*\(") {
                Add-Violation $File.FullName $lineNo "sim system calls other entrypoint $name"
            }
        }
    }
}

function Test-PresentationBoundary {
    param(
        [System.IO.FileInfo]$File,
        [string[]]$Lines
    )
    $full = $File.FullName.Replace('\', '/')
    if ($full -notmatch '/mugen/render/|/Source/ui/') {
        return
    }
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        $line = $Lines[$i]
        $lineNo = $i + 1
        if ($line -notmatch '^\s*#\s*include\s*"mugen/sim/([^"\\]+(?:/[^"\\]+)*)"') {
            continue
        }
        $inc = $Matches[1].Replace('\', '/')
        $allowed = ($inc -eq 'World.h') -or
                   ($inc -eq 'WorldSetup.h') -or
                   ($inc -eq 'BattleBootstrap.h') -or
                   ($inc -like 'view/*') -or
                   ($inc -eq 'present/PresentationEvent.h') -or
                   ($inc -like 'input/*') -or
                   ($inc -like 'net/*') -or
                   ($inc -like 'rollback/*') -or
                   ($inc -like 'replay/*')
        if (-not $allowed) {
            Add-Violation $File.FullName $lineNo 'P-7 render/ui include of sim internals'
        }
    }
}

$roots = Get-ScopeRoots
$fileList = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
foreach ($item in (Get-SourceFiles $roots)) {
    $fileList.Add($item)
}
if (Test-SimInScope) {
    if (Test-Path -LiteralPath $renderRoot) {
        Add-UniqueFiles $fileList (Get-SourceFiles @($renderRoot))
    }
    if (Test-Path -LiteralPath $uiRoot) {
        Add-UniqueFiles $fileList (Get-SourceFiles @($uiRoot))
    }
}

$entrypoints = Get-SimEntrypoints
foreach ($file in $fileList) {
    $text = [System.IO.File]::ReadAllText($file.FullName)
    $rawLines = $text.Replace("`r`n", "`n").Replace("`r", "`n").Split([char]10)
    Test-CommentCitations $file $rawLines
    $lines = Strip-Comments $text
    $active = Get-ActiveLines $lines
    $checked = for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($active[$i]) { $lines[$i] } else { '' }
    }
    Test-ForbiddenTokens $file $checked
    Test-SimSystemIsolation $file $checked $entrypoints
    Test-PresentationBoundary $file $lines
}

if ($script:Violations.Count -gt 0) {
    $script:Violations | Sort-Object | ForEach-Object { Write-Output $_ }
    exit 1
}
exit 0
