<#
.SYNOPSIS
  Adventurer's Lair tbaMUD Builder Source Contract Scanner

.DESCRIPTION
  Read-only static analyzer for the LOCAL tbaMUD source tree.

  It does NOT connect to the MUD, drive OLC, compile, modify source, or save
  game data. It scans C/H source files and produces an uploadable package that
  documents the builder/editor command surface.

  Primary goals:
    * Discover builder-facing OLC/editor source files.
    * Capture exact prompts and whether they contain newline terminators.
    * Capture visible menu text and menu choices.
    * Capture OLC/connection mode definitions and mode transitions.
    * Capture switch/case handlers used by editors.
    * Capture multiline string editor entry points.
    * Capture save confirmation and saved-to-disk behavior.
    * Flag potential destructive/mutating code paths for review.
    * Capture command registrations and editor command names.
    * Produce both human-readable and machine-readable contracts.
    * Produce a line-numbered source evidence bundle.

  This scanner intentionally over-collects. False-positive risk findings are
  acceptable because the purpose is to give ChatGPT/Codex enough exact source
  evidence to build reliable Mudlet OLC automation without guessing.

.EXAMPLE
  # From the repository root:
  powershell -ExecutionPolicy Bypass -File .\builder_source_contract_scan.ps1

.EXAMPLE
  # Explicit repository root:
  .\builder_source_contract_scan.ps1 -RepoRoot "C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED"

.EXAMPLE
  # Put reports somewhere specific:
  .\builder_source_contract_scan.ps1 `
    -RepoRoot "C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED" `
    -OutputRoot "C:\Users\antho\Desktop\TBAMUD\BuilderScans"

.OUTPUTS
  builder_command_contract.txt
  builder_command_contract.json
  builder_source_manifest.csv
  builder_prompts.csv
  builder_menu_entries.csv
  builder_mode_definitions.csv
  builder_mode_transitions.csv
  builder_case_blocks.csv
  builder_multiline_editors.csv
  builder_save_paths.csv
  builder_risk_candidates.csv
  builder_command_registrations.csv
  builder_source_bundle.txt
  builder_state_transitions.dot
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$RepoRoot = (Get-Location).Path,

    [Parameter(Mandatory = $false)]
    [string]$OutputRoot = "",

    [Parameter(Mandatory = $false)]
    [bool]$EmitSourceBundle = $true,

    [Parameter(Mandatory = $false)]
    [bool]$IncludeAllSourceMatches = $true,

    [Parameter(Mandatory = $false)]
    [bool]$IncludeGitMetadata = $true
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# -----------------------------------------------------------------------------
# Utility helpers
# -----------------------------------------------------------------------------

function Write-Info {
    param([string]$Message)
    Write-Host "[BuilderScan] $Message" -ForegroundColor Cyan
}

function Write-Warn {
    param([string]$Message)
    Write-Host "[BuilderScan] WARNING: $Message" -ForegroundColor Yellow
}

function Convert-ToRelativePath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    $base = [System.IO.Path]::GetFullPath($BasePath)
    $target = [System.IO.Path]::GetFullPath($TargetPath)

    if (-not $base.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $base += [System.IO.Path]::DirectorySeparatorChar
    }

    $baseUri = New-Object System.Uri($base)
    $targetUri = New-Object System.Uri($target)
    $relativeUri = $baseUri.MakeRelativeUri($targetUri)
    return [System.Uri]::UnescapeDataString($relativeUri.ToString()).Replace('/', '\')
}

function Get-SourceRoot {
    param([string]$StartingRoot)

    $root = [System.IO.Path]::GetFullPath($StartingRoot)

    $direct = Join-Path $root "src\redit.c"
    if (Test-Path -LiteralPath $direct) {
        return (Join-Path $root "src")
    }

    # Handles accidental nesting such as:
    # repo\tbamud_adventurers_lair-main\tbamud_adventurers_lair-main\src
    $hits = @(Get-ChildItem -LiteralPath $root -Filter "redit.c" -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -ieq "src" } |
        Sort-Object { $_.FullName.Length })

    if ($hits.Count -eq 0) {
        throw "Could not find src\redit.c beneath '$root'. Run this against the tbaMUD repository/source tree."
    }

    if ($hits.Count -gt 1) {
        Write-Warn "Found multiple src\redit.c files. Using the shortest path: $($hits[0].Directory.FullName)"
    }

    return $hits[0].Directory.FullName
}

function Get-FileText {
    param([string]$Path)
    return [System.IO.File]::ReadAllText($Path)
}

function Get-FileLines {
    param([string]$Path)
    return [System.IO.File]::ReadAllLines($Path)
}

function Get-Sha256 {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Decode-CString {
    param([string]$Raw)

    if ($null -eq $Raw) { return "" }

    $s = $Raw
    $s = $s -replace '\\r', "`r"
    $s = $s -replace '\\n', "`n"
    $s = $s -replace '\\t', "`t"
    $s = $s -replace '\\"', '"'
    $s = $s -replace "\\'", "'"
    $s = $s -replace '\\\\', '\'
    return $s
}

function Get-CStringLiterals {
    param([string]$Statement)

    $parts = New-Object System.Collections.Generic.List[string]
    $matches = [regex]::Matches($Statement, '"((?:\\.|[^"\\])*)"')
    foreach ($m in $matches) {
        $parts.Add((Decode-CString $m.Groups[1].Value))
    }

    return $parts.ToArray()
}

function Get-FunctionContextMap {
    param([string[]]$Lines)

    # This is deliberately heuristic. We only need a useful nearby function
    # name for evidence tables, not a full C parser.
    $map = @{}
    $current = ""
    $pending = ""

    for ($i = 0; $i -lt $Lines.Length; $i++) {
        $line = $Lines[$i]

        # One-line or first-line function definitions. Exclude control flow.
        if ($line -match '^\s*(?!if\b|for\b|while\b|switch\b|return\b|else\b)(?:static\s+)?(?:inline\s+)?(?:const\s+)?[A-Za-z_][A-Za-z0-9_\s\*\(\),]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*(\{)?\s*$') {
            $candidate = $Matches[1]
            if ($candidate -notmatch '^(if|for|while|switch|sizeof)$') {
                $pending = $candidate
                if ($line -match '\{') {
                    $current = $candidate
                    $pending = ""
                }
            }
        }
        elseif ($pending -ne "" -and $line -match '^\s*\{\s*$') {
            $current = $pending
            $pending = ""
        }

        $map[$i + 1] = $current
    }

    return $map
}

function Get-StatementAt {
    param(
        [string[]]$Lines,
        [int]$StartIndex,
        [int]$MaxLines = 40
    )

    $buf = New-Object System.Collections.Generic.List[string]
    $end = [Math]::Min($Lines.Length - 1, $StartIndex + $MaxLines - 1)

    for ($j = $StartIndex; $j -le $end; $j++) {
        $buf.Add($Lines[$j])
        if ($Lines[$j] -match ';\s*(/\*.*\*/\s*)?$') {
            break
        }
    }

    return ($buf -join "`n")
}

function Get-Excerpt {
    param(
        [string[]]$Lines,
        [int]$LineNumber,
        [int]$Before = 2,
        [int]$After = 5
    )

    $start = [Math]::Max(1, $LineNumber - $Before)
    $end = [Math]::Min($Lines.Length, $LineNumber + $After)
    $out = New-Object System.Collections.Generic.List[string]

    for ($n = $start; $n -le $end; $n++) {
        $out.Add(("{0,6}: {1}" -f $n, $Lines[$n - 1]))
    }

    return ($out -join "`n")
}

function Join-Unique {
    param(
        [object[]]$Values,
        [string]$Separator = " | "
    )

    $clean = @($Values |
        Where-Object { $null -ne $_ -and "$_".Trim() -ne "" } |
        ForEach-Object { "$_".Trim() } |
        Select-Object -Unique)

    return ($clean -join $Separator)
}

# -----------------------------------------------------------------------------
# Resolve repository/source/output paths
# -----------------------------------------------------------------------------

$SourceRoot = Get-SourceRoot $RepoRoot
$ResolvedRepoRoot = Split-Path -Parent $SourceRoot

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $ResolvedRepoRoot "builder_scan_output"
}

$OutputRoot = [System.IO.Path]::GetFullPath($OutputRoot)
$RunOutput = Join-Path $OutputRoot "builder_source_scan_$timestamp"

New-Item -ItemType Directory -Path $RunOutput -Force | Out-Null

Write-Info "Repository root: $ResolvedRepoRoot"
Write-Info "Source root:     $SourceRoot"
Write-Info "Output folder:   $RunOutput"

# -----------------------------------------------------------------------------
# Git metadata (read-only)
# -----------------------------------------------------------------------------

$GitMetadata = [ordered]@{
    Available = $false
    Commit = ""
    Branch = ""
    Status = @()
}

if ($IncludeGitMetadata) {
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($null -ne $git) {
        try {
            $commit = (& git -C $ResolvedRepoRoot rev-parse HEAD 2>$null)
            $branch = (& git -C $ResolvedRepoRoot branch --show-current 2>$null)
            $status = @(& git -C $ResolvedRepoRoot status --short 2>$null)

            if ($LASTEXITCODE -eq 0 -and $commit) {
                $GitMetadata.Available = $true
                $GitMetadata.Commit = "$commit".Trim()
                $GitMetadata.Branch = "$branch".Trim()
                $GitMetadata.Status = $status
            }
        }
        catch {
            Write-Warn "Git metadata could not be read: $($_.Exception.Message)"
        }
    }
}

# -----------------------------------------------------------------------------
# Discover source files
# -----------------------------------------------------------------------------

$KnownBuilderFiles = @(
    "redit.c",
    "oedit.c",
    "medit.c",
    "zedit.c",
    "sedit.c",
    "qedit.c",
    "dg_olc.c",
    "dg_olc.h",
    "oasis.c",
    "oasis.h",
    "oasis_copy.c",
    "oasis_delete.c",
    "oasis_list.c",
    "genolc.c",
    "genolc.h",
    "improved-edit.c",
    "improved-edit.h",
    "modify.c",
    "interpreter.c",
    "interpreter.h",
    "structs.h",
    "db.c",
    "db.h",
    "aedit.c",
    "cedit.c",
    "hedit.c",
    "hedit.h",
    "msgedit.c",
    "msgedit.h",
    "prefedit.c",
    "prefedit.h",
    "tedit.c",
    "quest.c",
    "quest.h",
    "shop.c",
    "shop.h",
    "constants.c",
    "constants.h",
    "utils.h",
    "screen.h",
    "class.h",
    "spells.h",
    "dg_scripts.h"
)

$BuilderContentRegex =
    'OLC_MODE\s*\(|OLC_[A-Z0-9_]+|CON_[A-Z0-9_]*EDIT|Enter choice|Enter your choice|' +
    'Do you wish to save|saved to disk|string_write\s*\(|send_editor_help\s*\(|' +
    'oasis|redit|oedit|medit|zedit|sedit|qedit|trigedit|hedit|cedit|aedit|msgedit'

$AllSourceFiles = @(Get-ChildItem -LiteralPath $SourceRoot -File -Recurse |
    Where-Object { $_.Extension -in @(".c", ".h") })

$RelevantFiles = New-Object System.Collections.Generic.List[System.IO.FileInfo]

foreach ($file in $AllSourceFiles) {
    $include = $KnownBuilderFiles -contains $file.Name

    if (-not $include -and $IncludeAllSourceMatches) {
        try {
            $text = Get-FileText $file.FullName
            if ($text -match $BuilderContentRegex) {
                $include = $true
            }
        }
        catch {
            Write-Warn "Could not read $($file.FullName): $($_.Exception.Message)"
        }
    }

    if ($include) {
        $RelevantFiles.Add($file)
    }
}

$RelevantFiles = @($RelevantFiles | Sort-Object FullName -Unique)

if ($RelevantFiles.Count -eq 0) {
    throw "No builder/editor source files were discovered."
}

Write-Info "Relevant source files discovered: $($RelevantFiles.Count)"

# -----------------------------------------------------------------------------
# Result collections
# -----------------------------------------------------------------------------

$Manifest = New-Object System.Collections.Generic.List[object]
$Prompts = New-Object System.Collections.Generic.List[object]
$MenuEntries = New-Object System.Collections.Generic.List[object]
$ModeDefinitions = New-Object System.Collections.Generic.List[object]
$ModeTransitions = New-Object System.Collections.Generic.List[object]
$CaseBlocks = New-Object System.Collections.Generic.List[object]
$MultilineEditors = New-Object System.Collections.Generic.List[object]
$SavePaths = New-Object System.Collections.Generic.List[object]
$RiskCandidates = New-Object System.Collections.Generic.List[object]
$CommandRegistrations = New-Object System.Collections.Generic.List[object]
$ChoiceTables = New-Object System.Collections.Generic.List[object]

# -----------------------------------------------------------------------------
# Risk classifier
# -----------------------------------------------------------------------------

function Get-RiskClassification {
    param([string]$Text)

    $reasons = New-Object System.Collections.Generic.List[string]
    $level = "READ_OR_UNKNOWN"

    if ($Text -match '(?i)\b(delete|purge|extract)_[A-Za-z0-9_]*\s*\(' -or
        $Text -match '(?i)\bREMOVE_BIT' -or
        $Text -match '(?i)\bfree\s*\(') {
        $level = "DESTRUCTIVE_REVIEW"
        $reasons.Add("delete/purge/extract/remove/free operation")
    }

    if ($Text -match 'GET_OBJ_VAL\s*\([^\)]*\)\s*=\s*0' -or
        $Text -match 'OLC_OBJ\s*\([^\)]*\)[^;]*=\s*NULL' -or
        $Text -match '(?i)\bmemset\s*\(') {
        $level = "DESTRUCTIVE_REVIEW"
        $reasons.Add("data cleared/reset on code path")
    }

    if ($Text -match '\bSET_BIT' -or
        $Text -match '\bTOGGLE_BIT' -or
        $Text -match 'OLC_VAL\s*\([^\)]*\)\s*=\s*[1-9]' -or
        $Text -match 'OLC_[A-Z0-9_]+\s*\([^\)]*\)[^;]*=') {
        if ($level -eq "READ_OR_UNKNOWN") {
            $level = "MUTATING_REVIEW"
        }
        $reasons.Add("OLC/object/flag mutation")
    }

    if ($Text -match '(?i)\b(save|write)_[A-Za-z0-9_]*(disk|zone|room|mob|obj|shop|quest|trigger)' -or
        $Text -match '(?i)saved to disk') {
        if ($level -eq "READ_OR_UNKNOWN") {
            $level = "SAVE_PATH"
        }
        $reasons.Add("disk/save operation")
    }

    if ($Text -match 'string_write\s*\(' -or $Text -match 'send_editor_help\s*\(') {
        if ($level -eq "READ_OR_UNKNOWN") {
            $level = "MULTILINE_EDITOR"
        }
        $reasons.Add("multiline editor entry")
    }

    return [PSCustomObject]@{
        Risk = $level
        Reasons = (Join-Unique $reasons.ToArray())
    }
}

# -----------------------------------------------------------------------------
# Parse files
# -----------------------------------------------------------------------------

$OutputCallRegex = '(write_to_output|send_to_char|send_to_room|send_editor_help|printf|snprintf|sprintf)\s*\('
$ModeAssignRegex = '(OLC_MODE\s*\(\s*d\s*\)|STATE\s*\(\s*d\s*\)|d->connected)\s*=\s*([A-Z][A-Z0-9_]+)'
$ModeDefineRegex = '^\s*#\s*define\s+([A-Z][A-Z0-9_]*(?:EDIT|OLC)[A-Z0-9_]*)\s+(.+?)\s*$'
$CaseRegex = '^\s*case\s+(.+?)\s*:\s*(?:/\*.*\*/\s*)?$'
$CommandNameRegex = '"(redit|oedit|medit|zedit|sedit|qedit|trigedit|tedit|hedit|cedit|aedit|msgedit|prefedit|redit|rlist|mlist|olist|tlist|slist|qlist)"'

foreach ($file in $RelevantFiles) {
    $relative = Convert-ToRelativePath $ResolvedRepoRoot $file.FullName
    Write-Info "Scanning $relative"

    $lines = Get-FileLines $file.FullName
    $text = $lines -join "`n"
    $functionMap = Get-FunctionContextMap $lines

    $editorScore = 0
    $editorScore += ([regex]::Matches($text, 'OLC_MODE\s*\(')).Count * 3
    $editorScore += ([regex]::Matches($text, 'Enter choice|Enter your choice')).Count * 2
    $editorScore += ([regex]::Matches($text, 'Do you wish to save')).Count * 3
    $editorScore += ([regex]::Matches($text, 'string_write\s*\(')).Count * 2

    $Manifest.Add([PSCustomObject]@{
        File = $relative
        Bytes = $file.Length
        Lines = $lines.Length
        SHA256 = Get-Sha256 $file.FullName
        EditorScore = $editorScore
        KnownBuilderFile = ($KnownBuilderFiles -contains $file.Name)
    })

    # -------------------------------------------------------------------------
    # Mode definitions (#define ...)
    # -------------------------------------------------------------------------
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]

        if ($line -match $ModeDefineRegex) {
            $ModeDefinitions.Add([PSCustomObject]@{
                File = $relative
                Line = $i + 1
                Symbol = $Matches[1]
                Definition = $Matches[2].Trim()
                Function = $functionMap[$i + 1]
                Evidence = $line.Trim()
            })
        }

        # Enum-like mode symbols even if not #defined.
        if ($line -match '^\s*([A-Z][A-Z0-9_]*(?:EDIT|OLC)_[A-Z0-9_]+)\s*(?:=\s*[^,]+)?\s*,?\s*(?:/\*.*\*/)?$') {
            $symbol = $Matches[1]
            if (-not ($ModeDefinitions | Where-Object { $_.File -eq $relative -and $_.Line -eq ($i + 1) })) {
                $ModeDefinitions.Add([PSCustomObject]@{
                    File = $relative
                    Line = $i + 1
                    Symbol = $symbol
                    Definition = "enum/list symbol"
                    Function = $functionMap[$i + 1]
                    Evidence = $line.Trim()
                })
            }
        }
    }

    # -------------------------------------------------------------------------
    # Prompts, output strings, and menu lines
    # -------------------------------------------------------------------------
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -notmatch $OutputCallRegex) {
            continue
        }

        $statement = Get-StatementAt $lines $i 50
        $literals = @(Get-CStringLiterals $statement)

        if ($literals.Count -eq 0) {
            continue
        }

        $decoded = ($literals -join "")
        if ([string]::IsNullOrWhiteSpace($decoded)) {
            continue
        }

        $rawHasCRLF = $statement -match '\\r\\n'
        $decodedEndsNewline = $decoded.EndsWith("`n") -or $decoded.EndsWith("`r")
        $isLikelyPrompt =
            $decoded -match '(?i)(enter|choice|select|which|number|name|keywords|description|save|quit|modify|value|type|flag|level|vnum|room|mob|object|trigger|quest|shop|apply|affection)'

        $risk = Get-RiskClassification $statement

        $Prompts.Add([PSCustomObject]@{
            File = $relative
            Line = $i + 1
            Function = $functionMap[$i + 1]
            OutputFunction = ([regex]::Match($lines[$i], $OutputCallRegex)).Groups[1].Value
            Text = $decoded.Replace("`r", "\r").Replace("`n", "\n")
            LikelyPrompt = $isLikelyPrompt
            ContainsCRLF = $rawHasCRLF
            EndsWithNewline = $decodedEndsNewline
            Risk = $risk.Risk
            Evidence = (Get-Excerpt $lines ($i + 1) 0 3)
        })

        $splitLines = $decoded -split "`r?`n"
        foreach ($menuLine in $splitLines) {
            $m = $menuLine.Trim()
            if ($m -match '^(?:\(?[0-9]+\)?|[A-Za-z0-9])\s*[\)\.\-:]?\s+.+$' -or
                $m -match '^[A-Za-z0-9]\)\s*.+$') {
                $MenuEntries.Add([PSCustomObject]@{
                    File = $relative
                    Line = $i + 1
                    Function = $functionMap[$i + 1]
                    MenuText = $m
                    SourceOutput = $decoded.Replace("`r", "\r").Replace("`n", "\n")
                })
            }
        }

        if ($decoded -match '(?i)do you wish to save|saved to disk|save your changes') {
            $SavePaths.Add([PSCustomObject]@{
                File = $relative
                Line = $i + 1
                Function = $functionMap[$i + 1]
                Kind = "MESSAGE"
                Text = $decoded.Replace("`r", "\r").Replace("`n", "\n")
                EndsWithNewline = $decodedEndsNewline
                Evidence = (Get-Excerpt $lines ($i + 1) 1 4)
            })
        }
    }

    # -------------------------------------------------------------------------
    # Mode transitions
    # -------------------------------------------------------------------------
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]

        $matches = [regex]::Matches($line, $ModeAssignRegex)
        foreach ($m in $matches) {
            $target = $m.Groups[2].Value
            $sourceMode = ""
            $sourceCase = ""

            # Look backward for nearest case within a reasonable parse block.
            for ($b = $i; $b -ge [Math]::Max(0, $i - 120); $b--) {
                if ($lines[$b] -match $CaseRegex) {
                    $sourceCase = $Matches[1].Trim()
                    if ($sourceCase -match '^[A-Z][A-Z0-9_]+$') {
                        $sourceMode = $sourceCase
                    }
                    break
                }
                if ($lines[$b] -match '^\s*switch\s*\(' -and $b -lt $i) {
                    break
                }
            }

            $ModeTransitions.Add([PSCustomObject]@{
                File = $relative
                Line = $i + 1
                Function = $functionMap[$i + 1]
                SourceMode = $sourceMode
                SourceCase = $sourceCase
                TargetMode = $target
                Assignment = $m.Groups[1].Value
                Evidence = $line.Trim()
            })
        }
    }

    # -------------------------------------------------------------------------
    # Multiline editors
    # -------------------------------------------------------------------------
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -notmatch 'string_write\s*\(') {
            continue
        }

        $statement = Get-StatementAt $lines $i 20
        $MultilineEditors.Add([PSCustomObject]@{
            File = $relative
            Line = $i + 1
            Function = $functionMap[$i + 1]
            Call = ($statement -replace '\s+', ' ').Trim()
            Evidence = (Get-Excerpt $lines ($i + 1) 3 5)
        })
    }

    # -------------------------------------------------------------------------
    # Save function calls / disk write paths
    # -------------------------------------------------------------------------
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]

        if ($line -match '(?i)\b([A-Za-z_][A-Za-z0-9_]*(?:save|write)[A-Za-z0-9_]*(?:disk|zone|room|mob|obj|shop|quest|trigger)[A-Za-z0-9_]*)\s*\(' -or
            $line -match '(?i)\b(save_[A-Za-z0-9_]+|write_[A-Za-z0-9_]+_to_disk)\s*\(') {

            $SavePaths.Add([PSCustomObject]@{
                File = $relative
                Line = $i + 1
                Function = $functionMap[$i + 1]
                Kind = "CALL"
                Text = $line.Trim()
                EndsWithNewline = $null
                Evidence = (Get-Excerpt $lines ($i + 1) 2 4)
            })
        }
    }

    # -------------------------------------------------------------------------
    # Command registrations / interpreter command names
    # -------------------------------------------------------------------------
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]

        if ($line -match $CommandNameRegex) {
            $CommandRegistrations.Add([PSCustomObject]@{
                File = $relative
                Line = $i + 1
                Function = $functionMap[$i + 1]
                Command = $Matches[1].ToLowerInvariant()
                Evidence = $line.Trim()
            })
        }

        if ($line -match 'ACMD\s*\(\s*(do_[A-Za-z0-9_]+)\s*\)') {
            $CommandRegistrations.Add([PSCustomObject]@{
                File = $relative
                Line = $i + 1
                Function = $functionMap[$i + 1]
                Command = $Matches[1]
                Evidence = $line.Trim()
            })
        }
    }

    # -------------------------------------------------------------------------
    # Case blocks
    # -------------------------------------------------------------------------
    $caseStarts = New-Object System.Collections.Generic.List[int]

    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i] -match $CaseRegex -or $lines[$i] -match '^\s*default\s*:') {
            $caseStarts.Add($i)
        }
    }

    for ($c = 0; $c -lt $caseStarts.Count; $c++) {
        $startIndex = $caseStarts[$c]
        $endIndex = [Math]::Min($lines.Length - 1, $startIndex + 100)

        if ($c + 1 -lt $caseStarts.Count) {
            $next = $caseStarts[$c + 1]
            if (($next - $startIndex) -le 100) {
                $endIndex = $next - 1
            }
        }

        $blockLines = @()
        for ($j = $startIndex; $j -le $endIndex; $j++) {
            $blockLines += $lines[$j]

            # End a small case at top-level break/return when practical.
            if ($j -gt $startIndex -and $lines[$j] -match '^\s*(break|return(?:\s+[^;]+)?)\s*;\s*(/\*.*\*/)?\s*$') {
                break
            }
        }

        $block = $blockLines -join "`n"
        $label = "default"

        if ($lines[$startIndex] -match $CaseRegex) {
            $label = $Matches[1].Trim()
        }

        $targets = @()
        foreach ($m in [regex]::Matches($block, $ModeAssignRegex)) {
            $targets += $m.Groups[2].Value
        }

        $strings = @(Get-CStringLiterals $block)
        $promptText = Join-Unique $strings

        $risk = Get-RiskClassification $block

        $CaseBlocks.Add([PSCustomObject]@{
            File = $relative
            Line = $startIndex + 1
            Function = $functionMap[$startIndex + 1]
            Case = $label
            TargetModes = (Join-Unique $targets)
            Strings = $promptText.Replace("`r", "\r").Replace("`n", "\n")
            Risk = $risk.Risk
            RiskReasons = $risk.Reasons
            Evidence = ($blockLines -join "`n")
        })

        if ($risk.Risk -ne "READ_OR_UNKNOWN") {
            $RiskCandidates.Add([PSCustomObject]@{
                File = $relative
                Line = $startIndex + 1
                Function = $functionMap[$startIndex + 1]
                Context = "case $label"
                Risk = $risk.Risk
                Reasons = $risk.Reasons
                Evidence = ($blockLines -join "`n")
            })
        }
    }

    # -------------------------------------------------------------------------
    # Additional line-level mutation/destruction candidates
    # -------------------------------------------------------------------------
    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]
        $risk = Get-RiskClassification $line

        if ($risk.Risk -in @("DESTRUCTIVE_REVIEW", "MUTATING_REVIEW")) {
            $RiskCandidates.Add([PSCustomObject]@{
                File = $relative
                Line = $i + 1
                Function = $functionMap[$i + 1]
                Context = "line"
                Risk = $risk.Risk
                Reasons = $risk.Reasons
                Evidence = (Get-Excerpt $lines ($i + 1) 2 3)
            })
        }
    }
}


# -----------------------------------------------------------------------------
# Extract builder choice/name tables from all source files
# -----------------------------------------------------------------------------
#
# tbaMUD editors often display a numbered index into global arrays such as
# room_bits, sector_types, item_types, wear_bits, affected_bits, apply_types,
# position_types, genders, and similar tables. These arrays are as important
# as menu prompts because an updater must know which numeric response maps to
# which semantic choice.
#
# This is a heuristic C string-array extractor, intentionally limited to
# builder-relevant table names.

$ChoiceNameRegex =
    '(?i)(bits|types|flags|position|gender|sex|sector|room|item|wear|extra|' +
    'apply|affect|action|attack|damage|dir|direction|zone|quest|shop|trigger|' +
    'class|race|equipment|connection|color|preference|preference_bits)'

foreach ($file in $AllSourceFiles) {
    $lines = Get-FileLines $file.FullName
    $relative = Convert-ToRelativePath $ResolvedRepoRoot $file.FullName

    for ($i = 0; $i -lt $lines.Length; $i++) {
        $line = $lines[$i]

        # Common forms:
        #   const char *room_bits[] = {
        #   const char * const sector_types[] = {
        #   const char *position_types[] = {
        if ($line -notmatch '^\s*(?:const\s+)?char\s*\*\s*(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*\]\s*=\s*(\{)?\s*$') {
            continue
        }

        $tableName = $Matches[1]
        if ($tableName -notmatch $ChoiceNameRegex) {
            continue
        }

        $startLine = $i + 1
        $buffer = New-Object System.Collections.Generic.List[string]
        $buffer.Add($line)

        $j = $i + 1
        $max = [Math]::Min($lines.Length - 1, $i + 5000)

        while ($j -le $max) {
            $buffer.Add($lines[$j])
            if ($lines[$j] -match '^\s*\};') {
                break
            }
            $j++
        }

        $arrayText = $buffer -join "`n"
        $values = @(Get-CStringLiterals $arrayText)

        for ($v = 0; $v -lt $values.Count; $v++) {
            $ChoiceTables.Add([PSCustomObject]@{
                Table = $tableName
                Index = $v
                Value = $values[$v].Replace("`r", "\r").Replace("`n", "\n")
                File = $relative
                Line = $startLine
            })
        }

        $i = $j
    }
}

# -----------------------------------------------------------------------------
# Deduplicate selected collections
# -----------------------------------------------------------------------------

$Prompts = @($Prompts | Sort-Object File, Line, Text -Unique)
$MenuEntries = @($MenuEntries | Sort-Object File, Line, MenuText -Unique)
$ModeDefinitions = @($ModeDefinitions | Sort-Object Symbol, File, Line -Unique)
$ModeTransitions = @($ModeTransitions | Sort-Object File, Line, TargetMode -Unique)
$CaseBlocks = @($CaseBlocks | Sort-Object File, Line, Case -Unique)
$MultilineEditors = @($MultilineEditors | Sort-Object File, Line -Unique)
$SavePaths = @($SavePaths | Sort-Object File, Line, Kind, Text -Unique)
$RiskCandidates = @($RiskCandidates | Sort-Object Risk, File, Line, Context -Unique)
$CommandRegistrations = @($CommandRegistrations | Sort-Object Command, File, Line -Unique)
$ChoiceTables = @($ChoiceTables | Sort-Object Table, Index, File, Line -Unique)
$Manifest = @($Manifest | Sort-Object -Property @{Expression="EditorScore"; Descending=$true}, @{Expression="File"; Descending=$false})

# -----------------------------------------------------------------------------
# Explicit high-value findings
# -----------------------------------------------------------------------------

$HighValueFindings = New-Object System.Collections.Generic.List[object]

function Add-HighValueFinding {
    param(
        [string]$Title,
        [string]$Severity,
        [string]$Reason,
        [object[]]$EvidenceRows
    )

    $evidence = @()
    foreach ($row in $EvidenceRows) {
        if ($null -ne $row) {
            $evidence += "$($row.File):$($row.Line) $($row.Evidence)"
        }
    }

    $HighValueFindings.Add([PSCustomObject]@{
        Title = $Title
        Severity = $Severity
        Reason = $Reason
        Evidence = ($evidence -join "`n")
    })
}

$oeditValueClear = @($RiskCandidates | Where-Object {
    $_.File -match '(^|\\)src\\oedit\.c$' -and
    $_.Evidence -match 'GET_OBJ_VAL' -and
    $_.Evidence -match '=\s*0'
})

if ($oeditValueClear.Count -gt 0) {
    Add-HighValueFinding `
        -Title "OEDIT value editor may clear object values on entry" `
        -Severity "CRITICAL_FOR_AUTOMATION" `
        -Reason "A read-only scanner must not enter this path unless source review proves it is non-mutating." `
        -EvidenceRows $oeditValueClear
}

$savePromptNoNewline = @($SavePaths | Where-Object {
    $_.Kind -eq "MESSAGE" -and
    $_.Text -match '(?i)do you wish to save' -and
    $_.EndsWithNewline -eq $false
})

if ($savePromptNoNewline.Count -gt 0) {
    Add-HighValueFinding `
        -Title "Save confirmation prompts without terminal newline" `
        -Severity "AUTOMATION_TIMING" `
        -Reason "Mudlet line triggers may not see these prompts as standalone lines; builders need source-backed deterministic handoffs." `
        -EvidenceRows $savePromptNoNewline
}

$keywordNoNewline = @($Prompts | Where-Object {
    $_.Text -match '(?i)enter keywords' -and $_.EndsWithNewline -eq $false
})

if ($keywordNoNewline.Count -gt 0) {
    Add-HighValueFinding `
        -Title "Keyword prompts without terminal newline" `
        -Severity "AUTOMATION_TIMING" `
        -Reason "Keyword prompt plus following menu redraw can be coalesced by Mudlet/telnet output." `
        -EvidenceRows $keywordNoNewline
}

# -----------------------------------------------------------------------------
# Export CSV tables
# -----------------------------------------------------------------------------

$Manifest | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_source_manifest.csv") -NoTypeInformation -Encoding UTF8
$Prompts | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_prompts.csv") -NoTypeInformation -Encoding UTF8
$MenuEntries | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_menu_entries.csv") -NoTypeInformation -Encoding UTF8
$ModeDefinitions | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_mode_definitions.csv") -NoTypeInformation -Encoding UTF8
$ModeTransitions | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_mode_transitions.csv") -NoTypeInformation -Encoding UTF8
$CaseBlocks | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_case_blocks.csv") -NoTypeInformation -Encoding UTF8
$MultilineEditors | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_multiline_editors.csv") -NoTypeInformation -Encoding UTF8
$SavePaths | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_save_paths.csv") -NoTypeInformation -Encoding UTF8
$RiskCandidates | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_risk_candidates.csv") -NoTypeInformation -Encoding UTF8
$CommandRegistrations | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_command_registrations.csv") -NoTypeInformation -Encoding UTF8
$ChoiceTables | Export-Csv -LiteralPath (Join-Path $RunOutput "builder_choice_tables.csv") -NoTypeInformation -Encoding UTF8

# -----------------------------------------------------------------------------
# JSON contract
# -----------------------------------------------------------------------------

$ContractObject = [ordered]@{
    Schema = "adventurers-lair-builder-source-contract-v1"
    Generated = (Get-Date).ToString("o")
    RepositoryRoot = $ResolvedRepoRoot
    SourceRoot = $SourceRoot
    Git = $GitMetadata
    Counts = [ordered]@{
        RelevantFiles = $RelevantFiles.Count
        Prompts = $Prompts.Count
        MenuEntries = $MenuEntries.Count
        ModeDefinitions = $ModeDefinitions.Count
        ModeTransitions = $ModeTransitions.Count
        CaseBlocks = $CaseBlocks.Count
        MultilineEditors = $MultilineEditors.Count
        SavePaths = $SavePaths.Count
        RiskCandidates = $RiskCandidates.Count
        CommandRegistrations = $CommandRegistrations.Count
        ChoiceTableEntries = $ChoiceTables.Count
        HighValueFindings = $HighValueFindings.Count
    }
    HighValueFindings = $HighValueFindings
    SourceManifest = $Manifest
    Prompts = $Prompts
    MenuEntries = $MenuEntries
    ModeDefinitions = $ModeDefinitions
    ModeTransitions = $ModeTransitions
    CaseBlocks = $CaseBlocks
    MultilineEditors = $MultilineEditors
    SavePaths = $SavePaths
    RiskCandidates = $RiskCandidates
    CommandRegistrations = $CommandRegistrations
    ChoiceTables = $ChoiceTables
}

$jsonPath = Join-Path $RunOutput "builder_command_contract.json"
$ContractObject | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

# -----------------------------------------------------------------------------
# GraphViz state transition graph
# -----------------------------------------------------------------------------

$dot = New-Object System.Collections.Generic.List[string]
$dot.Add("digraph BuilderOLC {")
$dot.Add('  rankdir=LR;')
$dot.Add('  node [shape=box,fontname="Consolas"];')

foreach ($edge in $ModeTransitions) {
    if ([string]::IsNullOrWhiteSpace($edge.TargetMode)) { continue }

    $from = $edge.SourceMode
    if ([string]::IsNullOrWhiteSpace($from)) {
        $from = $edge.Function
    }
    if ([string]::IsNullOrWhiteSpace($from)) {
        $from = "UNKNOWN"
    }

    $fromEsc = $from.Replace('"', '\"')
    $toEsc = $edge.TargetMode.Replace('"', '\"')
    $label = ("{0}:{1}" -f $edge.File, $edge.Line).Replace('"', '\"').Replace('\', '/')

    $dot.Add(('  "{0}" -> "{1}" [label="{2}"];' -f $fromEsc, $toEsc, $label))
}

$dot.Add("}")
$dot | Set-Content -LiteralPath (Join-Path $RunOutput "builder_state_transitions.dot") -Encoding UTF8

# -----------------------------------------------------------------------------
# Line-numbered source evidence bundle
# -----------------------------------------------------------------------------

if ($EmitSourceBundle) {
    $bundlePath = Join-Path $RunOutput "builder_source_bundle.txt"
    $writer = New-Object System.IO.StreamWriter($bundlePath, $false, [System.Text.Encoding]::UTF8)

    try {
        $writer.WriteLine("ADVENTURER'S LAIR - BUILDER SOURCE EVIDENCE BUNDLE")
        $writer.WriteLine("Generated: " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
        $writer.WriteLine("Repository: " + $ResolvedRepoRoot)
        $writer.WriteLine("Source root: " + $SourceRoot)
        if ($GitMetadata.Available) {
            $writer.WriteLine("Git branch: " + $GitMetadata.Branch)
            $writer.WriteLine("Git commit: " + $GitMetadata.Commit)
        }
        $writer.WriteLine("")
        $writer.WriteLine("Every included line is numbered so findings can be cited back to source.")
        $writer.WriteLine("")

        foreach ($file in ($RelevantFiles | Sort-Object FullName)) {
            $relative = Convert-ToRelativePath $ResolvedRepoRoot $file.FullName
            $lines = Get-FileLines $file.FullName

            $writer.WriteLine("")
            $writer.WriteLine(("=" * 100))
            $writer.WriteLine("FILE: " + $relative)
            $writer.WriteLine("SHA256: " + (Get-Sha256 $file.FullName))
            $writer.WriteLine(("=" * 100))

            for ($i = 0; $i -lt $lines.Length; $i++) {
                $writer.WriteLine(("{0,6}: {1}" -f ($i + 1), $lines[$i]))
            }
        }
    }
    finally {
        $writer.Flush()
        $writer.Close()
    }
}

# -----------------------------------------------------------------------------
# Human-readable report
# -----------------------------------------------------------------------------

$txtPath = Join-Path $RunOutput "builder_command_contract.txt"
$w = New-Object System.IO.StreamWriter($txtPath, $false, [System.Text.Encoding]::UTF8)

try {
    $w.WriteLine("ADVENTURER'S LAIR - BUILDER SOURCE CONTRACT SCAN")
    $w.WriteLine(("=" * 78))
    $w.WriteLine("Generated:       " + (Get-Date).ToString("yyyy-MM-dd HH:mm:ss"))
    $w.WriteLine("Repository root: " + $ResolvedRepoRoot)
    $w.WriteLine("Source root:     " + $SourceRoot)

    if ($GitMetadata.Available) {
        $w.WriteLine("Git branch:      " + $GitMetadata.Branch)
        $w.WriteLine("Git commit:      " + $GitMetadata.Commit)
        $w.WriteLine("Working changes: " + $GitMetadata.Status.Count)

        foreach ($statusLine in $GitMetadata.Status) {
            $w.WriteLine("  " + $statusLine)
        }
    }
    else {
        $w.WriteLine("Git metadata:    unavailable")
    }

    $w.WriteLine("")
    $w.WriteLine("READ ONLY STATIC ANALYSIS")
    $w.WriteLine("This script does not drive OLC and does not modify the MUD or source tree.")
    $w.WriteLine("")

    $w.WriteLine("COUNTS")
    $w.WriteLine(("=" * 78))
    $w.WriteLine("Relevant source files:  " + $RelevantFiles.Count)
    $w.WriteLine("Prompts/output blocks:   " + $Prompts.Count)
    $w.WriteLine("Menu entries:            " + $MenuEntries.Count)
    $w.WriteLine("Mode definitions:        " + $ModeDefinitions.Count)
    $w.WriteLine("Mode transitions:        " + $ModeTransitions.Count)
    $w.WriteLine("Switch/case blocks:      " + $CaseBlocks.Count)
    $w.WriteLine("Multiline editor calls:  " + $MultilineEditors.Count)
    $w.WriteLine("Save paths/messages:     " + $SavePaths.Count)
    $w.WriteLine("Risk candidates:         " + $RiskCandidates.Count)
    $w.WriteLine("Command registrations:   " + $CommandRegistrations.Count)
    $w.WriteLine("Choice table entries:     " + $ChoiceTables.Count)
    $w.WriteLine("")

    $w.WriteLine("HIGH-VALUE FINDINGS")
    $w.WriteLine(("=" * 78))

    if ($HighValueFindings.Count -eq 0) {
        $w.WriteLine("No explicit high-value heuristic findings were generated.")
    }
    else {
        foreach ($finding in $HighValueFindings) {
            $w.WriteLine("")
            $w.WriteLine("[" + $finding.Severity + "] " + $finding.Title)
            $w.WriteLine("Reason: " + $finding.Reason)
            $w.WriteLine("Evidence:")
            $w.WriteLine($finding.Evidence)
        }
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("RELEVANT SOURCE MANIFEST")
    $w.WriteLine(("=" * 78))

    foreach ($row in $Manifest) {
        $w.WriteLine(("{0,5}  {1,7} lines  {2}" -f $row.EditorScore, $row.Lines, $row.File))
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("BUILDER COMMAND REGISTRATIONS")
    $w.WriteLine(("=" * 78))

    foreach ($row in $CommandRegistrations) {
        $w.WriteLine(("{0,-18} {1}:{2}  {3}" -f $row.Command, $row.File, $row.Line, $row.Evidence))
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("BUILDER CHOICE / INDEX TABLES")
    $w.WriteLine(("=" * 78))

    foreach ($group in ($ChoiceTables | Group-Object Table | Sort-Object Name)) {
        $w.WriteLine("")
        $w.WriteLine("TABLE: " + $group.Name)

        foreach ($row in ($group.Group | Sort-Object Index)) {
            $w.WriteLine(("  [{0,3}] {1}    ({2}:{3})" -f $row.Index, $row.Value, $row.File, $row.Line))
        }
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("SAVE CONFIRMATIONS AND DISK-SAVE PATHS")
    $w.WriteLine(("=" * 78))

    foreach ($row in $SavePaths) {
        $w.WriteLine("")
        $w.WriteLine(("[{0}] {1}:{2} {3}" -f $row.Kind, $row.File, $row.Line, $row.Function))
        if ($null -ne $row.EndsWithNewline) {
            $w.WriteLine("EndsWithNewline: " + $row.EndsWithNewline)
        }
        $w.WriteLine($row.Text)
        $w.WriteLine($row.Evidence)
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("NON-NEWLINE / TIMING-SENSITIVE PROMPTS")
    $w.WriteLine(("=" * 78))

    foreach ($row in ($Prompts | Where-Object { $_.LikelyPrompt -and -not $_.EndsWithNewline })) {
        $w.WriteLine("")
        $w.WriteLine(("{0}:{1} {2}" -f $row.File, $row.Line, $row.Function))
        $w.WriteLine($row.Text)
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("MULTILINE EDITOR ENTRY POINTS")
    $w.WriteLine(("=" * 78))

    foreach ($row in $MultilineEditors) {
        $w.WriteLine("")
        $w.WriteLine(("{0}:{1} {2}" -f $row.File, $row.Line, $row.Function))
        $w.WriteLine($row.Call)
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("MODE DEFINITIONS")
    $w.WriteLine(("=" * 78))

    foreach ($row in $ModeDefinitions) {
        $w.WriteLine(("{0,-40} {1}:{2}  {3}" -f $row.Symbol, $row.File, $row.Line, $row.Definition))
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("MODE TRANSITIONS")
    $w.WriteLine(("=" * 78))

    foreach ($row in $ModeTransitions) {
        $from = $row.SourceMode
        if ([string]::IsNullOrWhiteSpace($from)) { $from = $row.SourceCase }
        if ([string]::IsNullOrWhiteSpace($from)) { $from = $row.Function }

        $w.WriteLine(("{0,-35} -> {1,-35} {2}:{3}" -f $from, $row.TargetMode, $row.File, $row.Line))
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("MUTATION / DESTRUCTIVE RISK CANDIDATES")
    $w.WriteLine(("=" * 78))
    $w.WriteLine("These are heuristic review flags, not automatic proof of a bug.")
    $w.WriteLine("")

    foreach ($row in $RiskCandidates) {
        $w.WriteLine("")
        $w.WriteLine(("[{0}] {1}:{2} {3} ({4})" -f $row.Risk, $row.File, $row.Line, $row.Function, $row.Context))
        $w.WriteLine("Reason: " + $row.Reasons)
        $w.WriteLine($row.Evidence)
    }

    $w.WriteLine("")
    $w.WriteLine("")
    $w.WriteLine("FILES TO UPLOAD TO CHATGPT/CODEX")
    $w.WriteLine(("=" * 78))
    $w.WriteLine("1. builder_command_contract.txt")
    $w.WriteLine("2. builder_command_contract.json")
    $w.WriteLine("3. builder_source_bundle.txt")
    $w.WriteLine("")
    $w.WriteLine("The CSV files are useful for targeted debugging and machine processing.")
}
finally {
    $w.Flush()
    $w.Close()
}

# -----------------------------------------------------------------------------
# Small README
# -----------------------------------------------------------------------------

$readme = @"
ADVENTURER'S LAIR BUILDER SOURCE SCAN

Scan completed successfully.

Repository:
$ResolvedRepoRoot

Source:
$SourceRoot

Primary files to upload to ChatGPT:
  1. builder_command_contract.txt
  2. builder_command_contract.json
  3. builder_source_bundle.txt

Additional tables:
  builder_source_manifest.csv
  builder_prompts.csv
  builder_menu_entries.csv
  builder_mode_definitions.csv
  builder_mode_transitions.csv
  builder_case_blocks.csv
  builder_multiline_editors.csv
  builder_save_paths.csv
  builder_risk_candidates.csv
  builder_command_registrations.csv
  builder_choice_tables.csv

Optional graph:
  builder_state_transitions.dot

The scan is static/read-only. It does not connect to the MUD or modify source.
"@

$readme | Set-Content -LiteralPath (Join-Path $RunOutput "README_FIRST.txt") -Encoding UTF8

# -----------------------------------------------------------------------------
# Zip the complete scan package when Compress-Archive is available
# -----------------------------------------------------------------------------

$zipPath = "$RunOutput.zip"

try {
    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $RunOutput "*") -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Info "ZIP package created: $zipPath"
}
catch {
    Write-Warn "Could not create ZIP package: $($_.Exception.Message)"
}

Write-Host ""
Write-Host "======================================================================" -ForegroundColor Green
Write-Host " BUILDER SOURCE CONTRACT SCAN COMPLETE" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green
Write-Host ""
Write-Host "Output folder:" -ForegroundColor White
Write-Host "  $RunOutput" -ForegroundColor Yellow
Write-Host ""
Write-Host "Upload these three files first:" -ForegroundColor White
Write-Host "  builder_command_contract.txt" -ForegroundColor Yellow
Write-Host "  builder_command_contract.json" -ForegroundColor Yellow

if ($EmitSourceBundle) {
    Write-Host "  builder_source_bundle.txt" -ForegroundColor Yellow
}

if (Test-Path -LiteralPath $zipPath) {
    Write-Host ""
    Write-Host "Or upload the complete ZIP:" -ForegroundColor White
    Write-Host "  $zipPath" -ForegroundColor Yellow
}

Write-Host ""
Write-Host ("Relevant source files: {0}" -f $RelevantFiles.Count)
Write-Host ("Prompts:               {0}" -f $Prompts.Count)
Write-Host ("Mode transitions:      {0}" -f $ModeTransitions.Count)
Write-Host ("Risk candidates:       {0}" -f $RiskCandidates.Count)
Write-Host ("Choice table entries:   {0}" -f $ChoiceTables.Count)
Write-Host ""
