param(
	[string]$UnrealPluginDir = "S:\LocalDev\SVG_Import\Plugins\SvgMeshImporter",
	[string]$Branch = "main"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path "$UnrealPluginDir\.git")) {
	Write-Error "Unreal plugin git folder not found: $UnrealPluginDir"
}

$Status = git -C $UnrealPluginDir status --porcelain
if ($Status) {
	Write-Host "Unreal plugin folder had local changes; resetting to origin/$Branch ..."
}

Write-Host "Syncing Unreal plugin folder from origin/$Branch ..."
git -C $UnrealPluginDir fetch origin $Branch
git -C $UnrealPluginDir reset --hard "origin/$Branch"
Write-Host "Unreal plugin folder is now at:" (git -C $UnrealPluginDir rev-parse --short HEAD)
