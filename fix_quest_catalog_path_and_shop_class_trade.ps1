$ErrorActionPreference = "Stop"

$Root = "C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED"
Set-Location $Root

$Required = @(
    "src\shop.c",
    "src\quest_rewards.c",
    "lib\misc\quest_rewards.cfg"
)

foreach ($f in $Required) {
    if (!(Test-Path $f)) {
        throw "Missing required file: $f"
    }
}

Write-Host "Branch: " -NoNewline
git branch --show-current
Write-Host "HEAD:   " -NoNewline
git rev-parse --short HEAD
Write-Host ""
Write-Host "Worktree before hotfix:"
git status --short
Write-Host ""

$BackupDir = Join-Path $env:TEMP ("quest_shop_hotfix_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $BackupDir | Out-Null

foreach ($f in @("src\shop.c", "src\quest_rewards.c")) {
    $dest = Join-Path $BackupDir $f
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item $f $dest -Force
}

function Restore-Files {
    Write-Host ""
    Write-Host "Restoring touched files..."
    foreach ($f in @("src\shop.c", "src\quest_rewards.c")) {
        Copy-Item (Join-Path $BackupDir $f) $f -Force
    }
}

try {
    $shop = (Get-Content "src\shop.c" -Raw) -replace "`r`n", "`n"
    $qr   = (Get-Content "src\quest_rewards.c" -Raw) -replace "`r`n", "`n"

    $OldPath = '#define QUEST_REWARD_FILE "lib/misc/quest_rewards.cfg"'
    $NewPath = '#define QUEST_REWARD_FILE LIB_MISC"quest_rewards.cfg"'

    $PathCount = ([regex]::Matches($qr, [regex]::Escape($OldPath))).Count
    if ($PathCount -ne 1) {
        throw "Quest reward path preflight expected exactly 1 match, found $PathCount"
    }
    $qr = $qr.Replace($OldPath, $NewPath)

    $OldClassBlock = @'
  if ((IS_MAGIC_USER(ch) && NOTRADE_MAGIC_USER(shop_nr)) ||
      (IS_CLERIC(ch) && NOTRADE_CLERIC(shop_nr)) ||
      (IS_THIEF(ch) && NOTRADE_THIEF(shop_nr)) ||
      (IS_WARRIOR(ch) && NOTRADE_WARRIOR(shop_nr))) {
    snprintf(buf, sizeof(buf), "%s %s", GET_NAME(ch), MSG_NO_SELL_CLASS);
    do_tell(keeper, buf, cmd_tell, 0);
    return (FALSE);
  }
'@ -replace "`r`n", "`n"

    $NewClassBlock = @'
  /* Adventurer's Lair: shops never refuse service based solely on class.
   * Legacy class no-trade bits remain loadable for old shop-file
   * compatibility, but they are intentionally ignored at runtime. */
'@ -replace "`r`n", "`n"

    $ClassCount = ([regex]::Matches($shop, [regex]::Escape($OldClassBlock))).Count
    if ($ClassCount -ne 1) {
        throw "Shop class-refusal preflight expected exactly 1 match, found $ClassCount"
    }
    $shop = $shop.Replace($OldClassBlock, $NewClassBlock)

    if ($shop.Contains("NOTRADE_THIEF(shop_nr)") -or
        $shop.Contains("NOTRADE_CLERIC(shop_nr)") -or
        $shop.Contains("NOTRADE_MAGIC_USER(shop_nr)") -or
        $shop.Contains("NOTRADE_WARRIOR(shop_nr)")) {
        throw "Class-based shop refusal logic still remains after patch."
    }

    if (!$qr.Contains('#define QUEST_REWARD_FILE LIB_MISC"quest_rewards.cfg"')) {
        throw "Quest reward runtime path fix was not applied."
    }

    $Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText((Join-Path $Root "src\shop.c"), $shop, $Utf8NoBom)
    [IO.File]::WriteAllText((Join-Path $Root "src\quest_rewards.c"), $qr, $Utf8NoBom)

    Write-Host "Applied source changes."

    Write-Host ""
    Write-Host "Verifying catalog at the MUD's runtime data-directory path..."
    wsl bash -lc "cd '/mnt/c/Users/antho/Desktop/TBAMUD/TBAMUD UPDATED/lib' && test -f misc/quest_rewards.cfg && echo 'Found: misc/quest_rewards.cfg'"
    if ($LASTEXITCODE -ne 0) {
        throw "Runtime catalog path verification failed."
    }

    Write-Host ""
    Write-Host "git diff --check..."
    git diff --check -- src/shop.c src/quest_rewards.c
    if ($LASTEXITCODE -ne 0) {
        throw "git diff --check failed."
    }

    Write-Host ""
    Write-Host "Building..."
    wsl bash -lc "cd '/mnt/c/Users/antho/Desktop/TBAMUD/TBAMUD UPDATED' && make -C src -j2 CFLAGS='-g -O2 -Wall -Wno-char-subscripts -Wno-unused-but-set-variable -std=gnu17'"
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed."
    }

    if (Test-Path "tests\phase5_shared_combat_helpers_test.py") {
        Write-Host ""
        Write-Host "Running existing regression test..."
        wsl bash -lc "cd '/mnt/c/Users/antho/Desktop/TBAMUD/TBAMUD UPDATED' && python3 tests/phase5_shared_combat_helpers_test.py"
        if ($LASTEXITCODE -ne 0) {
            throw "Regression test failed."
        }
    }

    Write-Host ""
    Write-Host "==============================================================="
    Write-Host "QUEST CATALOG + SHOP CLASS HOTFIX APPLIED SUCCESSFULLY"
    Write-Host "==============================================================="
    Write-Host ""
    Write-Host "Fixed:"
    Write-Host "  - quest list now reads misc\quest_rewards.cfg from the lib runtime directory"
    Write-Host "  - shops no longer reject Magic Users, Clerics, Thieves, or Warriors by class"
    Write-Host "  - alignment restrictions remain unchanged"
    Write-Host ""
    Write-Host "Full worktree:"
    git status --short
}
catch {
    Restore-Files
    throw
}
finally {
    if (Test-Path $BackupDir) {
        Remove-Item $BackupDir -Recurse -Force
    }
}
