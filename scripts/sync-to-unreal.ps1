param(
	[string]$UnrealPluginDir = "S:\LocalDev\SVG_Import\Plugins\SvgMeshImporter",
	[string]$SourceRepo = "C:\Users\Jo\Documents\SvgMeshImporter",
	[string]$Branch = "main"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path "$UnrealPluginDir\.git")) {
	Write-Error "Unreal plugin git folder not found: $UnrealPluginDir"
}

if (-not (Test-Path "$SourceRepo\.git")) {
	Write-Error "Source repo not found: $SourceRepo"
}

$Remotes = git -C $UnrealPluginDir remote
if ($Remotes -notcontains "source") {
	git -C $UnrealPluginDir remote add source $SourceRepo
}

$Status = git -C $UnrealPluginDir status --porcelain
if ($Status) {
	Write-Host "Unreal plugin folder had local changes; resetting to source/$Branch ..."
}

Write-Host "Syncing Unreal plugin folder from source/$Branch ..."
git -C $UnrealPluginDir fetch source $Branch
git -C $UnrealPluginDir reset --hard "source/$Branch"
Write-Host "Unreal plugin folder is now at:" (git -C $UnrealPluginDir rev-parse --short HEAD)
