param(
	[string]$UnrealPluginDir = "S:\LocalDev\SVG_Import\Plugins\SvgMeshImporter",
	[string]$ProjectFile = "S:\LocalDev\SVG_Import\SVG_Import.uproject",
	[string]$EngineRoot = "D:\Epic_Games\UE_5.7",
	[switch]$Sync
)

$ErrorActionPreference = "Stop"

$SourceRepo = Split-Path $PSScriptRoot -Parent
$Ubt = Join-Path $EngineRoot "Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll"
$PluginFile = Join-Path $UnrealPluginDir "SvgMeshImporter.uplugin"

if (-not (Test-Path $Ubt)) {
	Write-Error "UnrealBuildTool not found: $Ubt"
}

if (-not (Test-Path $PluginFile)) {
	Write-Error "Plugin descriptor not found: $PluginFile"
}

if (-not (Test-Path $ProjectFile)) {
	Write-Error "Project file not found: $ProjectFile"
}

if ($Sync) {
	& (Join-Path $PSScriptRoot "sync-to-unreal.ps1") -UnrealPluginDir $UnrealPluginDir -SourceRepo $SourceRepo
}

Write-Host "Building SvgMeshImporter plugin only (UnrealEditor + -plugin) ..."
Write-Host "  Project: $ProjectFile"
Write-Host "  Plugin:  $PluginFile"

& dotnet $Ubt UnrealEditor Win64 Development `
	"-Project=$ProjectFile" `
	"-plugin=$PluginFile" `
	-WaitMutex

if ($LASTEXITCODE -ne 0) {
	Write-Error "Plugin build failed with exit code $LASTEXITCODE"
}

Write-Host "Plugin build succeeded."
Write-Host "Binaries: $(Join-Path $UnrealPluginDir 'Binaries\Win64')"
