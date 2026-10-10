param(
    [string]$Paths = ''
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$clientRoot = Split-Path -Parent $scriptRoot
$sourceRoot = Join-Path $clientRoot 'Source'
$mugenRoot = Join-Path $sourceRoot 'mugen'

# English product name is held here and never printed. A hit is reported as the generic rule name.
$script:ProductNamePattern = 'heiyue|黑月|BlackMoon'

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

function Get-ScopeRoots {
    if (-not $Paths) {
        return @($sourceRoot)
    }
    $roots = @()
    foreach ($part in ($Paths -split ',')) {
        $name = $part.Trim()
        if (-not $name) {
            continue
        }
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
        $files += Get-ChildItem -LiteralPath $root -Recurse -File -Include *.h, *.hpp, *.cpp, *.c, *.cc
    }
    return $files
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
        if ($line -match $script:ProductNamePattern) {
            Add-Violation $File.FullName $lineNo 'product name'
        }
        if ($line -match 'MG_LOG_[DIWE]\(' -and $line -match '[\u4e00-\u9fff]') {
            Add-Violation $File.FullName $lineNo 'log macro argument contains Chinese'
        }
    }
}

$roots = Get-ScopeRoots
    $files = Get-SourceFiles $roots
foreach ($file in $files) {
    $text = [System.IO.File]::ReadAllText($file.FullName)
    $lines = Strip-Comments $text
    $active = Get-ActiveLines $lines
    $checked = for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($active[$i]) { $lines[$i] } else { '' }
    }
    Test-ForbiddenTokens $file $checked
}

if ($script:Violations.Count -gt 0) {
    $script:Violations | Sort-Object | ForEach-Object { Write-Output $_ }
    exit 1
}
exit 0
