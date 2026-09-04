$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$path = "src\fight.c"

if (!(Test-Path $path)) {
    throw ("Missing {0}. Run this from C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED" -f $path)
}

$resolved = (Resolve-Path $path).Path
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$text = [System.IO.File]::ReadAllText($resolved)
$original = $text

function Get-NL([string]$value) {
    if ($value.Contains("`r`n")) { return "`r`n" }
    return "`n"
}

try {
    $nl = Get-NL $text

    $funcStart = $text.IndexOf("void set_fighting(struct char_data *ch, struct char_data *vict)")
    if ($funcStart -lt 0) {
        throw "Could not find set_fighting(). Nothing changed."
    }

    $funcTail = $text.Substring($funcStart)
    $nextFunc = [regex]::Match(
        $funcTail.Substring(1),
        "(?m)^(?:void|int|static\s+void|static\s+int)\s+[A-Za-z_][A-Za-z0-9_]*\s*\("
    )

    if ($nextFunc.Success) {
        $funcEnd = $funcStart + 1 + $nextFunc.Index
    } else {
        $funcEnd = $text.Length
    }

    $block = $text.Substring($funcStart, $funcEnd - $funcStart)

    if ($block -match 'affect_from_char\(ch,\s*SKILL_SNEAK\)') {
        throw "set_fighting() already appears to remove Sneak. Nothing changed."
    }

    $anchorPattern = '(?ms)(if\s*\(\s*ch\s*==\s*vict\s*\)\s*\r?\n\s*return\s*;)'
    $matches = [regex]::Matches($block, $anchorPattern)

    if ($matches.Count -ne 1) {
        throw ("set_fighting() self-check anchor: expected exactly one match, found {0}. Nothing changed." -f $matches.Count)
    }

    $insert = @'

  /* Entering real combat breaks mobile concealment.  This is centralized here
   * so ordinary attacks, theft retaliation, assists, and scripted combat all
   * obey the same Sneak/Skulk rule.  Vanish still works because it stops combat
   * first and applies Sneak afterward. */
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    affect_from_char(ch, SKILL_SNEAK);
  if (AFF_FLAGGED(ch, AFF_SKULK))
    affect_from_char(ch, SKILL_SKULK);
'@

    if ($nl -eq "`r`n") {
        $insert = $insert -replace "`n", "`r`n"
    }

    $block = [regex]::Replace(
        $block,
        $anchorPattern,
        ('$1' + $insert),
        1
    )

    $text = $text.Substring(0, $funcStart) + $block + $text.Substring($funcEnd)

    if ($text -notmatch 'Entering real combat breaks mobile concealment') {
        throw "Validation failed: combat concealment hook not inserted."
    }

    if ($text -notmatch 'affect_from_char\(ch,\s*SKILL_SNEAK\)') {
        throw "Validation failed: Sneak removal missing."
    }

    if ($text -notmatch 'affect_from_char\(ch,\s*SKILL_SKULK\)') {
        throw "Validation failed: Skulk removal missing."
    }

    [System.IO.File]::WriteAllText($resolved, $text, $utf8NoBom)

    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    $check = git diff --check -- $path 2>&1
    $checkCode = $LASTEXITCODE
    $check | ForEach-Object { Write-Host $_ }

    if ($checkCode -eq 0) {
        wsl bash -lc "cd '/mnt/c/Users/antho/Desktop/TBAMUD/TBAMUD UPDATED' && make -C src -j2 CFLAGS='-g -O2 -Wall -Wno-char-subscripts -Wno-unused-but-set-variable -std=gnu17'"
        $buildCode = $LASTEXITCODE
    }
    else {
        $buildCode = 1
    }

    if ($buildCode -eq 0 -and (Test-Path "tests\phase5_shared_combat_helpers_test.py")) {
        py -3 tests\phase5_shared_combat_helpers_test.py
        $testCode = $LASTEXITCODE
    }
    else {
        $testCode = 0
    }

    $ErrorActionPreference = $oldPreference

    if ($checkCode -ne 0) { throw "git diff --check failed." }
    if ($buildCode -ne 0) { throw "WSL build failed." }
    if ($testCode -ne 0) { throw "phase5_shared_combat_helpers_test.py failed." }

    Write-Host ""
    Write-Host "COMBAT STEALTH BREAK FIX APPLIED" -ForegroundColor Green
    Write-Host ""
    Write-Host "Entering combat now removes:" -ForegroundColor Cyan
    Write-Host "  Sneak"
    Write-Host "  Skulk"
    Write-Host ""
    Write-Host "This is centralized in set_fighting(), so normal attacks, retaliation,"
    Write-Host "guards assisting, theft combat, and scripted combat use the same rule."
    Write-Host ""
    Write-Host "Hide/Invisibility are not changed by this patch." -ForegroundColor Cyan
    Write-Host "Vanish remains valid because it leaves combat before reapplying Sneak."
    Write-Host ""
    Write-Host "git diff --check passed." -ForegroundColor Green
    Write-Host "WSL build passed." -ForegroundColor Green

    if (Test-Path "tests\phase5_shared_combat_helpers_test.py") {
        Write-Host "phase5_shared_combat_helpers_test.py passed." -ForegroundColor Green
    }

    Write-Host ""
    Write-Host "Current status:" -ForegroundColor Cyan
    git status --short -- $path
}
catch {
    [System.IO.File]::WriteAllText($resolved, $original, $utf8NoBom)
    Write-Host ""
    Write-Host "Patch failed. Restored src\fight.c." -ForegroundColor Red
    throw
}
