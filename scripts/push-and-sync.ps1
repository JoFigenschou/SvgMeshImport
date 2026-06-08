param(
	[string]$Branch = "main"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

Push-Location $RepoRoot
try {
	git push origin $Branch
	if ($LASTEXITCODE -ne 0) {
		exit $LASTEXITCODE
	}
}
finally {
	Pop-Location
}

& (Join-Path $PSScriptRoot "sync-to-unreal.ps1")
