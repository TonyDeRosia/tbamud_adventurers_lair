param(
    [string]$RepoRoot = "C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED",
    [ValidateSet("Preview","Apply")]
    [string]$Mode = "Preview"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2.0

$FightPath = Join-Path $RepoRoot "src\fight.c"
if (-not (Test-Path -LiteralPath $FightPath -PathType Leaf)) {
    throw "Missing required file: $FightPath"
}

$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$Stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$BackupRoot = Join-Path $RepoRoot ("source_fix_backups\severity_renderer_{0}" -f $Stamp)

function Read-All {
    param([string]$Path)
    return [System.IO.File]::ReadAllText($Path)
}

function Count-Literal {
    param([string]$Text, [string]$Needle)
    if ([string]::IsNullOrEmpty($Needle)) { return 0 }
    $count = 0
    $at = 0
    while (($at = $Text.IndexOf($Needle, $at, [System.StringComparison]::Ordinal)) -ge 0) {
        $count++
        $at += $Needle.Length
    }
    return $count
}

function Replace-OneRegex {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Replacement,
        [string]$Label
    )

    $rx = New-Object System.Text.RegularExpressions.Regex(
        $Pattern,
        [System.Text.RegularExpressions.RegexOptions]::Singleline
    )
    $matches = $rx.Matches($Text)

    if ($matches.Count -ne 1) {
        throw "$Label expected exactly one match, found $($matches.Count)."
    }

    return $rx.Replace($Text, $Replacement, 1)
}

$fightOld = Read-All $FightPath

# Confirm the architecture actually observed by the diagnostic.
foreach ($needle in @(
    'static void format_severity_verb(char *out, size_t outsz, int tier, int third_person)',
    'static void apply_severity_verb(char *out, size_t outsz, const char *in, int tier)',
    'const char *vb  = severity_verb_base(tier);',
    'const char *vt  = severity_verb_third(tier);',
    'const char *pre = severity_impact_wrap_open(tier);',
    'const char *post = severity_impact_wrap_close(tier);'
)) {
    if ((Count-Literal $fightOld $needle) -ne 1) {
        throw "Expected current combat architecture exactly once, but did not find: $needle"
    }
}

$applyReplacement = @'
static void apply_severity_verb(char *out, size_t outsz, const char *in, int tier)
{
  char verb_base[128];
  char verb_third[128];
  char *pos;

  if (!in || !*in) {
    if (outsz)
      out[0] = '\0';
    return;
  }

  /*
   * Reuse the same authoritative severity formatter used by the normal
   * damage paths.  This keeps the top-tier presentation in one place
   * instead of independently rebuilding another CENSORED wrapper here.
   */
  format_severity_verb(verb_base, sizeof(verb_base), tier, FALSE);
  format_severity_verb(verb_third, sizeof(verb_third), tier, TRUE);

  snprintf(out, outsz, "%s", in);

  /* Prefer replacing " hit" (base) first (skills often use "You hit $N"). */
  if ((pos = strstr(out, " hit "))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s %s", out, verb_base, pos + 5);
    snprintf(out, outsz, "%s", tmp);
    return;
  }

  if ((pos = strstr(out, " hit!"))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s!%s", out, verb_base, pos + 5);
    snprintf(out, outsz, "%s", tmp);
    return;
  }

  /* Then replace third person " hits" (many spell messages use "X hits $N"). */
  if ((pos = strstr(out, " hits "))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s %s", out, verb_third, pos + 6);
    snprintf(out, outsz, "%s", tmp);
    return;
  }

  if ((pos = strstr(out, " hits!"))) {
    char tmp[MAX_STRING_LENGTH];
    *pos = '\0';
    snprintf(tmp, sizeof(tmp), "%s %s!%s", out, verb_third, pos + 6);
    snprintf(out, outsz, "%s", tmp);
    return;
  }

  /* If we cannot find a generic verb, leave the authored message as-is. */
}
'@

$fightNew = Replace-OneRegex `
    $fightOld `
    'static void apply_severity_verb\(char \*out, size_t outsz, const char \*in, int tier\)\s*\{.*?\n\}\s*(?=static int compute_thaco)' `
    ($applyReplacement + "`r`n`r`n") `
    "apply_severity_verb"

if ($fightNew -eq $fightOld) {
    throw "Patch produced no source change."
}

# Re-read only the patched function and validate what matters.
$applyRx = New-Object System.Text.RegularExpressions.Regex(
    'static void apply_severity_verb\(char \*out, size_t outsz, const char \*in, int tier\)\s*\{.*?\n\}',
    [System.Text.RegularExpressions.RegexOptions]::Singleline
)
$applyMatch = $applyRx.Match($fightNew)
if (-not $applyMatch.Success) {
    throw "Could not parse patched apply_severity_verb()."
}
$applyBlock = $applyMatch.Value

foreach ($required in @(
    'format_severity_verb(verb_base, sizeof(verb_base), tier, FALSE);',
    'format_severity_verb(verb_third, sizeof(verb_third), tier, TRUE);'
)) {
    if (-not $applyBlock.Contains($required)) {
        throw "Patched function missing: $required"
    }
}

foreach ($forbidden in @(
    'severity_verb_base(tier)',
    'severity_verb_third(tier)',
    'severity_impact_wrap_open(tier)',
    'severity_impact_wrap_close(tier)'
)) {
    if ($applyBlock.Contains($forbidden)) {
        throw "Patched function still contains duplicate-renderer component: $forbidden"
    }
}

# Do NOT assert any literal ANSI/CENSORED spelling here.
# The diagnostic proved format_severity_verb() exists; its internal visual
# pattern is intentionally left untouched.

Write-Host ""
Write-Host "COMBAT SEVERITY RENDERER FIX V3" -ForegroundColor Cyan
Write-Host "--------------------------------"
Write-Host "This patch changes ONLY src\fight.c."
Write-Host ""
Write-Host "Confirmed problem:"
Write-Host "  apply_severity_verb() independently rebuilt severity verbs/wrappers."
Write-Host "  Normal damage paths already use format_severity_verb()."
Write-Host ""
Write-Host "Fix:"
Write-Host "  apply_severity_verb() will now call format_severity_verb() too."
Write-Host ""
Write-Host "Neutral severity ladder is NOT changed."
Write-Host "No ANSI pattern inside format_severity_verb() is changed."
Write-Host "DG scripted-damage table is NOT changed by this patch."
Write-Host ""

if ($Mode -eq "Preview") {
    Write-Host "PREVIEW PASSED. No files were modified." -ForegroundColor Green
    Write-Host ""
    Write-Host "Apply with:"
    Write-Host 'powershell -ExecutionPolicy Bypass -File ".\fix_duplicate_censored_renderer_v3.ps1" -Mode Apply'
    exit 0
}

New-Item -ItemType Directory -Force -Path (Join-Path $BackupRoot "src") | Out-Null
Copy-Item -LiteralPath $FightPath -Destination (Join-Path $BackupRoot "src\fight.c") -Force

try {
    [System.IO.File]::WriteAllText($FightPath, $fightNew, $Utf8NoBom)

    Push-Location $RepoRoot
    try {
        Write-Host ""
        Write-Host "git diff --check..." -ForegroundColor Cyan
        & git diff --check -- src/fight.c
        if ($LASTEXITCODE -ne 0) {
            throw "git diff --check failed."
        }

        Write-Host ""
        Write-Host "Relevant fight.c diff..." -ForegroundColor Cyan
        & git diff -- src/fight.c

        Write-Host ""
        Write-Host "Clean GNU17 build..." -ForegroundColor Cyan
        & wsl bash -lc "cd '/mnt/c/Users/antho/Desktop/TBAMUD/TBAMUD UPDATED' && make -C src clean && make -C src MYFLAGS='-std=gnu17 -Wall -Wno-char-subscripts -Wno-unused-but-set-variable' -j2"
        if ($LASTEXITCODE -ne 0) {
            throw "GNU17 build failed."
        }
    }
    finally {
        Pop-Location
    }
}
catch {
    Copy-Item -LiteralPath (Join-Path $BackupRoot "src\fight.c") -Destination $FightPath -Force

    Write-Host ""
    Write-Host "FIX FAILED. src\fight.c was restored from backup." -ForegroundColor Red
    Write-Host "Backup retained at:"
    Write-Host $BackupRoot
    throw
}

Write-Host ""
Write-Host "COMBAT SEVERITY RENDERER FIX V3 SUCCESSFUL." -ForegroundColor Green
Write-Host ""
Write-Host "Backup:"
Write-Host $BackupRoot
Write-Host ""
Write-Host "Restart the MUD, then reproduce the same high-damage mercenary hit."
Write-Host "The authored-message path and normal damage path now share one formatter."
