$RepoRoot = Split-Path -Parent $PSScriptRoot
$HookSource = Join-Path $PSScriptRoot "post-push"
$HookTarget = Join-Path $RepoRoot ".git\hooks\post-push"

$Content = Get-Content -Path $HookSource -Raw
$Content = $Content -replace "`r`n", "`n"
[System.IO.File]::WriteAllText($HookTarget, $Content)
Write-Host "Installed post-push hook -> $HookTarget"
