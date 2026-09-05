$ErrorActionPreference = 'Stop'
$RepoRoot = 'C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED'
$BackupRoot = 'C:\Users\antho\Desktop\TBAMUD\TBAMUD UPDATED\codex_audit_exports\body_profile_backup_20260905_035328'
$files = Get-ChildItem -LiteralPath (Join-Path $BackupRoot 'lib\world\mob') -Filter '*.mob' -File
foreach ($f in $files) {
    $dest = Join-Path $RepoRoot ('lib\world\mob\' + $f.Name)
    Copy-Item -LiteralPath $f.FullName -Destination $dest -Force
}
Write-Host ('Restored ' + $files.Count + ' mob files from the body-profile backup.') -ForegroundColor Green