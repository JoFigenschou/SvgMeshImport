$RepoRoot = Split-Path -Parent $PSScriptRoot
$HookSource = Join-Path $PSScriptRoot "post-push"
$HookTarget = Join-Path $RepoRoot ".git\hooks\post-push"

Copy-Item -Path $HookSource -Destination $HookTarget -Force
Write-Host "Installed post-push hook -> $HookTarget"
