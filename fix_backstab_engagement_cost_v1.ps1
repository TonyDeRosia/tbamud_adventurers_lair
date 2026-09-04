$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$path = "src\act.offensive.c"

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

function Normalize-NL([string]$value, [string]$nl) {
    return [regex]::Replace($value, "`r`n|`r|`n", $nl)
}

function Replace-Once([string]$value, [string]$old, [string]$new, [string]$label) {
    $count = ([regex]::Matches($value, [regex]::Escape($old))).Count
    if ($count -ne 1) {
        throw ("{0}: expected exactly one match, found {1}. Nothing changed." -f $label, $count)
    }
    return $value.Replace($old, $new)
}

try {
    $nl = Get-NL $text

    $oldDecl = @'
{
  int percent, success;

  if (!physical_skill_target_ok(ch, vict) || GET_POS(ch) < POS_STANDING ||
      !GET_EQ(ch, WEAR_WIELD) ||
      GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT ||
      FIGHTING(vict) || is_owned_follower_target(ch, vict))
    return COMBAT_SKILL_NOT_ATTEMPTED;
'@
    $oldDecl = Normalize-NL $oldDecl $nl

    $newDecl = @'
{
  int percent, success, move_cost;

  if (!physical_skill_target_ok(ch, vict) || GET_POS(ch) < POS_STANDING ||
      !GET_EQ(ch, WEAR_WIELD) ||
      GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD), 3) != TYPE_PIERCE - TYPE_HIT ||
      FIGHTING(vict) || is_owned_follower_target(ch, vict))
    return COMBAT_SKILL_NOT_ATTEMPTED;

  /* Player Backstab attempts pay the configured physical-skill cost from MOVE.
   * DG-scripted NPC backstabs call this helper with improve == FALSE and keep
   * their existing resource behavior. */
  move_cost = MAX(0, spell_info[SKILL_BACKSTAB].mana_min);
  if (improve && GET_LEVEL(ch) < LVL_IMMORT && GET_MOVE(ch) < move_cost) {
    send_to_char(ch, "You are too exhausted to attempt a backstab.\r\n");
    return COMBAT_SKILL_NOT_ATTEMPTED;
  }
  if (improve && GET_LEVEL(ch) < LVL_IMMORT && move_cost > 0)
    GET_MOVE(ch) = MAX(0, GET_MOVE(ch) - move_cost);

  /* Backstab is an aggressive action and breaks voluntary concealment. */
  if (AFF_FLAGGED(ch, AFF_INVISIBLE) || AFF_FLAGGED(ch, AFF_HIDE))
    appear(ch);
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    affect_from_char(ch, SKILL_SNEAK);
  if (AFF_FLAGGED(ch, AFF_SKULK))
    affect_from_char(ch, SKILL_SKULK);
'@
    $newDecl = Normalize-NL $newDecl $nl

    $text = Replace-Once $text $oldDecl $newDecl "Backstab cost/concealment setup"

    $oldAware = @'
  if (MOB_FLAGGED(vict, MOB_AWARE) && AWAKE(vict)) {
    act("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);
    hit(vict, ch, TYPE_UNDEFINED);
    return COMBAT_SKILL_ATTEMPTED;
  }
'@
    $oldAware = Normalize-NL $oldAware $nl

    $newAware = @'
  if (MOB_FLAGGED(vict, MOB_AWARE) && AWAKE(vict)) {
    act("You notice $N lunging at you!", FALSE, vict, 0, ch, TO_CHAR);
    act("$e notices you lunging at $m!", FALSE, vict, 0, ch, TO_VICT);
    act("$n notices $N lunging at $m!", FALSE, vict, 0, ch, TO_NOTVICT);

    /* Spotting the ambush turns it into a real fight, not a one-off swing. */
    if (!FIGHTING(vict) && GET_POS(vict) > POS_STUNNED)
      set_fighting(vict, ch);
    if (!FIGHTING(ch) && GET_POS(ch) > POS_STUNNED)
      set_fighting(ch, vict);

    hit(vict, ch, TYPE_UNDEFINED);

    if (improve)
      improve_ability_from_use(ch, SKILL_BACKSTAB, FALSE);
    WAIT_STATE(ch, 2 * PULSE_VIOLENCE);
    return COMBAT_SKILL_ATTEMPTED;
  }
'@
    $newAware = Normalize-NL $newAware $nl

    $text = Replace-Once $text $oldAware $newAware "MOB_AWARE Backstab response"

    # Static validation before write.
    if ($text -notmatch 'spell_info\[SKILL_BACKSTAB\]\.mana_min') {
        throw "Validation failed: Backstab MOVE cost hook is missing."
    }
    if ($text -notmatch 'set_fighting\(ch,\s*vict\)') {
        throw "Validation failed: player combat engagement is missing."
    }
    if ($text -notmatch 'improve_ability_from_use\(ch,\s*SKILL_BACKSTAB,\s*FALSE\)') {
        throw "Validation failed: noticed Backstab improvement/failure hook is missing."
    }
    if ($text -notmatch 'affect_from_char\(ch,\s*SKILL_SNEAK\)') {
        throw "Validation failed: aggressive Backstab Sneak break is missing."
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
    Write-Host "BACKSTAB LIVE-BEHAVIOR FIX APPLIED" -ForegroundColor Green
    Write-Host ""
    Write-Host "Backstab now:" -ForegroundColor Cyan
    Write-Host "  costs its configured 20 MOVE on a real player attempt"
    Write-Host "  refuses the attempt if the player lacks the MOVE"
    Write-Host "  breaks Hide/Invisibility/Sneak/Skulk as an aggressive action"
    Write-Host "  starts full combat when an AWARE victim spots the lunge"
    Write-Host "  applies normal Backstab lag when spotted"
    Write-Host "  records the spotted attempt as a failed skill-use attempt"
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
    Write-Host "Patch failed. Restored src\act.offensive.c." -ForegroundColor Red
    throw
}
