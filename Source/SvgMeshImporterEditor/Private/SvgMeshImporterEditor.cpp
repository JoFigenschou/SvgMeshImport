#include "SvgMeshImporterEditor.h"

#include "SSvgPreviewPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

static const FName SvgMeshPreviewTabName("SvgMeshPreview");

static TSharedRef<SDockTab> SpawnSvgPreviewTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SSvgPreviewPanel)
		];
}

void FSvgMeshImporterEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SvgMeshPreviewTabName, FOnSpawnTab::CreateStatic(&SpawnSvgPreviewTab))
		.SetDisplayName(NSLOCTEXT("SvgMeshImporter", "SvgMeshPreviewTab", "SVG Mesh Preview"))
		.SetTooltipText(NSLOCTEXT("SvgMeshImporter", "SvgMeshPreviewTabTip", "Preview SVG to procedural mesh conversion"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSvgMeshImporterEditorModule::RegisterMenus));
}

void FSvgMeshImporterEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SvgMeshPreviewTabName);
}

void FSvgMeshImporterEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools"))
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("SvgMeshImporter");
		Section.AddMenuEntry(
			"OpenSvgMeshPreview",
			NSLOCTEXT("SvgMeshImporter", "OpenSvgMeshPreview", "SVG Mesh Preview"),
			NSLOCTEXT("SvgMeshImporter", "OpenSvgMeshPreviewTip", "Open the SVG mesh preview panel"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([]()
			{
				FGlobalTabmanager::Get()->TryInvokeTab(SvgMeshPreviewTabName);
			}))
		);
	}
}

IMPLEMENT_MODULE(FSvgMeshImporterEditorModule, SvgMeshImporterEditor)
