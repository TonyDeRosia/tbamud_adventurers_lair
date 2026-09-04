$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$files = @(
    "src\fight.c",
    "src\dg_misc.c",
    "src\act.informative.c",
    "lib\misc\messages"
)

foreach ($p in $files) {
    if (!(Test-Path $p)) {
        throw ("Missing {0}. Run this from C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED" -f $p)
    }
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$originals = @{}
foreach ($p in $files) {
    $resolved = (Resolve-Path $p).Path
    $originals[$resolved] = [System.IO.File]::ReadAllText($resolved)
}

function Read-Text([string]$p) {
    return [System.IO.File]::ReadAllText((Resolve-Path $p).Path)
}

function Write-Text([string]$p, [string]$text) {
    [System.IO.File]::WriteAllText((Resolve-Path $p).Path, $text, $utf8NoBom)
}

function Replace-Required([string]$text, [string]$old, [string]$new, [string]$label) {
    $count = ([regex]::Matches($text, [regex]::Escape($old))).Count
    if ($count -ne 1) {
        throw ("{0}: expected exactly one match, found {1}." -f $label, $count)
    }
    return $text.Replace($old, $new)
}

function Replace-Optional([string]$text, [string]$old, [string]$new, [string]$label) {
    $count = ([regex]::Matches($text, [regex]::Escape($old))).Count
    if ($count -gt 0) {
        $text = $text.Replace($old, $new)
        Write-Host ("Replaced {0} occurrence(s): {1}" -f $count, $label)
    } else {
        Write-Host ("Already clean / not present: {0}" -f $label)
    }
    return $text
}

try {
    # ==================================================================
    # fight.c
    # 1) Distinguish physical skills from spells in generic fallback
    #    damage messages.  Skill ids live above MAX_SPELLS.
    # 2) Clean death / XP / wound-state stock text.
    # ==================================================================
    $fight = Read-Text "src\fight.c"

    $anchor = 'int shown = skill_message(dam, ch, victim, attacktype);'
    $replacement = @'
int shown = skill_message(dam, ch, victim, attacktype);
    int physical_skill = (attacktype > MAX_SPELLS && attacktype <= MAX_SKILLS);
'@
    $fight = Replace-Required $fight $anchor $replacement "fight.c physical-skill classifier"

    $fight = Replace-Required $fight `
        'snprintf(to_char, sizeof(to_char), "Your magic %s%s%s%s\tn $N.", col, pre, v3[tier], post);' `
        'snprintf(to_char, sizeof(to_char), physical_skill ? "Your strike %s%s%s%s\tn $N." : "Your magic %s%s%s%s\tn $N.", col, pre, v3[tier], post);' `
        "fight.c attacker fallback"

    $fight = Replace-Required $fight `
        'snprintf(to_vict, sizeof(to_vict), "$n''s magic %s%s%s%s\tn you.", col, pre, v3[tier], post);' `
        'snprintf(to_vict, sizeof(to_vict), physical_skill ? "$n''s strike %s%s%s%s\tn you." : "$n''s magic %s%s%s%s\tn you.", col, pre, v3[tier], post);' `
        "fight.c victim fallback"

    $fight = Replace-Required $fight `
        'snprintf(to_room, sizeof(to_room), "$n''s magic %s%s%s%s\tn $N.", col, pre, v3[tier], post);' `
        'snprintf(to_room, sizeof(to_room), physical_skill ? "$n''s strike %s%s%s%s\tn $N." : "$n''s magic %s%s%s%s\tn $N.", col, pre, v3[tier], post);' `
        "fight.c room fallback"

    $oldMiss = @'
      if (ch) {
        act("Your magic misses $N.", FALSE, ch, NULL, victim, TO_CHAR);
        act("$n's magic misses you.", FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
        act("$n's magic misses $N.", FALSE, ch, NULL, victim, TO_NOTVICT);
      } else {
'@
    $newMiss = @'
      if (ch) {
        if (physical_skill) {
          act("Your strike misses $N.", FALSE, ch, NULL, victim, TO_CHAR);
          act("$n's strike misses you.", FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
          act("$n's strike misses $N.", FALSE, ch, NULL, victim, TO_NOTVICT);
        } else {
          act("Your magic misses $N.", FALSE, ch, NULL, victim, TO_CHAR);
          act("$n's magic misses you.", FALSE, ch, NULL, victim, TO_VICT | TO_SLEEP);
          act("$n's magic misses $N.", FALSE, ch, NULL, victim, TO_NOTVICT);
        }
      } else {
'@
    $fight = Replace-Required $fight $oldMiss $newMiss "fight.c zero-damage physical-skill fallback"

    $fight = Replace-Optional $fight `
        'act("$n is dead!  \tYR.I.P.\tn", FALSE, victim, 0, 0, TO_ROOM);' `
        'act("$n falls lifeless.", FALSE, victim, 0, 0, TO_ROOM);' `
        "fight.c R.I.P. death line"

    $fight = Replace-Optional $fight `
        'send_to_char(victim, "You are dead!  Sorry...\r\n");' `
        'send_to_char(victim, "You have been slain.\r\n");' `
        "fight.c player death line"

    $fight = Replace-Optional $fight `
        'send_to_char(ch, "You receive one lousy experience point.\r\n");' `
        'send_to_char(ch, "You receive \ty1\tn \tCexperience\tn point.\r\n");' `
        "fight.c one-XP line"

    $fight = Replace-Optional $fight 'return "has some big nasty wounds and scratches.";' 'return "is badly wounded.";' "fight.c wound description"
    $fight = Replace-Optional $fight 'return "needs a hospital.";' 'return "is barely clinging to life.";' "fight.c setting-breaking hospital text"

    Write-Text "src\fight.c" $fight

    # ==================================================================
    # dg_misc.c
    # Keep script-inflicted damage/death output consistent with fight.c.
    # ==================================================================
    $dg = Read-Text "src\dg_misc.c"
    $dg = Replace-Optional $dg `
        'act("$n is dead!  R.I.P.", FALSE, ch, 0, 0, TO_ROOM);' `
        'act("$n falls lifeless.", FALSE, ch, 0, 0, TO_ROOM);' `
        "dg_misc.c R.I.P. death line"
    $dg = Replace-Optional $dg `
        'send_to_char(ch, "You are dead!  Sorry...\r\n");' `
        'send_to_char(ch, "You have been slain.\r\n");' `
        "dg_misc.c player death line"
    Write-Text "src\dg_misc.c" $dg

    # ==================================================================
    # act.informative.c
    # Match LOOK condition wording to combat condition wording.
    # ==================================================================
    $info = Read-Text "src\act.informative.c"
    $info = Replace-Optional $info 'has some big nasty wounds and scratches.' 'is badly wounded.' "look wound wording"
    $info = Replace-Optional $info 'looks pretty hurt.' 'is gravely wounded.' "look grave-wound wording"
    $info = Replace-Optional $info 'is in awful condition.' 'is barely holding on.' "look near-death wording"
    $info = Replace-Optional $info 'is bleeding awfully from big wounds.' 'is bleeding heavily and near death.' "look critical wording"
    Write-Text "src\act.informative.c" $info

    # ==================================================================
    # lib/misc/messages
    # Clean stock joke/awkward lines observed live.  Optional replacements
    # preserve earlier local cleanup passes if some were already changed.
    # ==================================================================
    $msg = Read-Text "lib\misc\messages"

    $msg = Replace-Optional $msg `
        'You miss $N by an inch, curse that brat!' `
        'Your slash misses $N.' `
        "slash attacker miss"

    $msg = Replace-Optional $msg `
        'You manage to dodge $n''s slash and laugh, HA!' `
        'You evade $n''s slash at the last moment.' `
        "slash victim miss"

    $msg = Replace-Optional $msg `
        '$n''s slash misses $N who laughs in sheer delight, HA!' `
        '$N evades $n''s slash at the last moment.' `
        "slash room miss"

    $msg = Replace-Optional $msg `
        'What a stroke of luck, $n seems to have missed you with $s thrash!' `
        'You evade $n''s thrashing attack.' `
        "thrash victim miss"

    $msg = Replace-Optional $msg `
        'You successfully pierce $N and $S dead body falls to the ground in a lifeless heap.' `
        'Your piercing strike drops $N lifeless to the ground.' `
        "pierce attacker death"

    $msg = Replace-Optional $msg `
        '$n pierces you.  You are no longer a living member of this world -- R.I.P.!' `
        '$n pierces you with a fatal thrust.' `
        "pierce victim death"

    $msg = Replace-Optional $msg `
        '$n pierces $N whose suddenly lifeless body falls to the ground!' `
        '$n pierces $N with a fatal thrust, and $N falls lifeless.' `
        "pierce room death"

    Write-Text "lib\misc\messages" $msg

    # ==================================================================
    # Validation
    # ==================================================================
    $fightCheck = Read-Text "src\fight.c"

    if ($fightCheck -notmatch 'physical_skill\s*=\s*\(attacktype\s*>\s*MAX_SPELLS') {
        throw "Validation failed: physical skill classifier missing."
    }

    if ($fightCheck -match 'Your magic misses \$N\.".*?physical_skill' -and
        $fightCheck -notmatch 'Your strike misses \$N\.') {
        throw "Validation failed: physical-skill miss fallback missing."
    }

    $allText = ""
    foreach ($p in $files) {
        $allText += (Read-Text $p) + "`n"
    }

    foreach ($bad in @(
        'one lousy experience point',
        'needs a hospital',
        'laugh, HA!',
        'What a stroke of luck'
    )) {
        if ($allText -match [regex]::Escape($bad)) {
            throw ("Validation failed: stock combat phrase still present: {0}" -f $bad)
        }
    }

    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    $check = git diff --check -- src/fight.c src/dg_misc.c src/act.informative.c lib/misc/messages 2>&1
    $checkCode = $LASTEXITCODE
    $check | ForEach-Object { Write-Host $_ }

    if ($checkCode -eq 0) {
        wsl bash -lc "cd '/mnt/c/Users/antho/Desktop/TBAMUD/TBAMUD UPDATED' && make -C src -j2 CFLAGS='-g -O2 -Wall -Wno-char-subscripts -Wno-unused-but-set-variable -std=gnu17'"
        $buildCode = $LASTEXITCODE
    } else {
        $buildCode = 1
    }

    if ($buildCode -eq 0 -and (Test-Path "tests\phase5_shared_combat_helpers_test.py")) {
        py -3 tests\phase5_shared_combat_helpers_test.py
        $testCode = $LASTEXITCODE
    } else {
        $testCode = 0
    }

    $ErrorActionPreference = $oldPreference

    if ($checkCode -ne 0) { throw "git diff --check failed." }
    if ($buildCode -ne 0) { throw "WSL build failed." }
    if ($testCode -ne 0) { throw "phase5_shared_combat_helpers_test.py failed." }

    Write-Host ""
    Write-Host "COMBAT MESSAGE CLEANUP V1 APPLIED" -ForegroundColor Green
    Write-Host ""
    Write-Host "Major changes:" -ForegroundColor Cyan
    Write-Host "  physical skills no longer fall back to 'Your magic ...'"
    Write-Host "  stock R.I.P./Sorry death text cleaned"
    Write-Host "  'one lousy experience point' cleaned"
    Write-Host "  modern 'needs a hospital' wording removed"
    Write-Host "  live-tested goofy slash/thrash dodge lines cleaned"
    Write-Host "  piercing death text cleaned"
    Write-Host "  combat/look wound wording polished"
    Write-Host ""
    Write-Host "git diff --check passed." -ForegroundColor Green
    Write-Host "WSL build passed." -ForegroundColor Green

    if (Test-Path "tests\phase5_shared_combat_helpers_test.py") {
        Write-Host "phase5_shared_combat_helpers_test.py passed." -ForegroundColor Green
    }

    Write-Host ""
    Write-Host "Touched files:" -ForegroundColor Cyan
    git status --short -- src/fight.c src/dg_misc.c src/act.informative.c lib/misc/messages
}
catch {
    foreach ($entry in $originals.GetEnumerator()) {
        [System.IO.File]::WriteAllText($entry.Key, $entry.Value, $utf8NoBom)
    }
    Write-Host ""
    Write-Host "Patch failed. Restored all touched files." -ForegroundColor Red
    throw
}
