param(
	[string]$UnrealPluginDir = "S:\LocalDev\SVG_Import\Plugins\SvgMeshImporter",
	[string]$SourceRepo = "C:\Users\Jo\Documents\SvgMeshImporter"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $SourceRepo)) {
	Write-Error "Source repo not found: $SourceRepo"
}

if (-not (Test-Path $UnrealPluginDir)) {
	Write-Error "Unreal plugin folder not found: $UnrealPluginDir"
}

function Sync-RoboMirror {
	param(
		[string]$SourcePath,
		[string]$DestinationPath,
		[string]$Label
	)

	if (-not (Test-Path $SourcePath)) {
		return
	}

	if (-not (Test-Path $DestinationPath)) {
		New-Item -ItemType Directory -Path $DestinationPath -Force | Out-Null
	}

	& robocopy $SourcePath $DestinationPath /MIR /NFL /NDL /NJH /NJS | Out-Null
	if ($LASTEXITCODE -ge 8) {
		Write-Error "Robocopy failed for $Label with exit code $LASTEXITCODE"
	}

	Write-Host "  Synced $Label"
}

Write-Host "Syncing Documents (source of truth) -> Unreal plugin folder ..."
Write-Host "  From: $SourceRepo"
Write-Host "  To:   $UnrealPluginDir"

$SyncDirectories = @(
	"Source",
	"ThirdParty",
	"Resources",
	"Config"
)

foreach ($DirectoryName in $SyncDirectories) {
	$SourcePath = Join-Path $SourceRepo $DirectoryName
	$DestinationPath = Join-Path $UnrealPluginDir $DirectoryName
	Sync-RoboMirror -SourcePath $SourcePath -DestinationPath $DestinationPath -Label $DirectoryName
}

$PluginDescriptor = "SvgMeshImporter.uplugin"
$SourcePluginFile = Join-Path $SourceRepo $PluginDescriptor
if (Test-Path $SourcePluginFile) {
	Copy-Item $SourcePluginFile (Join-Path $UnrealPluginDir $PluginDescriptor) -Force
	Write-Host "  Copied $PluginDescriptor"
}

Write-Host "Unreal plugin folder updated from Documents."
