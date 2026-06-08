$RepoRoot = Split-Path -Parent $PSScriptRoot

foreach ($HookName in @("post-commit", "post-push")) {
	$HookSource = Join-Path $PSScriptRoot $HookName
	$HookTarget = Join-Path $RepoRoot ".git\hooks\$HookName"
	$Content = Get-Content -Path $HookSource -Raw
	$Content = $Content -replace "`r`n", "`n"
	[System.IO.File]::WriteAllText($HookTarget, $Content)
	Write-Host "Installed $HookName hook -> $HookTarget"
}
