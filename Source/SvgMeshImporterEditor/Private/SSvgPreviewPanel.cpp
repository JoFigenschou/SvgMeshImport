#include "SSvgPreviewPanel.h"

#include "Framework/Application/SlateApplication.h"

#include "SvgMeshGenerator.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

void SSvgPreviewPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock).Text(NSLOCTEXT("SvgMeshImporter", "PreviewTitle", "SVG Mesh Importer Preview"))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(SvgFilePath); })
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					SvgFilePath = Text.ToString();
				})
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
			[
				SNew(SButton)
				.Text(NSLOCTEXT("SvgMeshImporter", "Browse", "Browse..."))
				.OnClicked(this, &SSvgPreviewPanel::OnBrowseClicked)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SButton)
			.Text(NSLOCTEXT("SvgMeshImporter", "BuildPreview", "Build Preview"))
			.OnClicked(this, &SSvgPreviewPanel::OnBuildPreviewClicked)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(this, &SSvgPreviewPanel::GetDiagnosticsText)
			]
		]
	];
}

FReply SSvgPreviewPanel::OnBrowseClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	TArray<FString> OutFiles;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Choose SVG File"),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("SVG files|*.svg"),
		EFileDialogFlags::None,
		OutFiles);

	if (bOpened && OutFiles.Num() > 0)
	{
		SvgFilePath = OutFiles[0];
	}
	return FReply::Handled();
}

FReply SSvgPreviewPanel::OnBuildPreviewClicked()
{
	DiagnosticsText.Reset();
	if (SvgFilePath.IsEmpty())
	{
		DiagnosticsText = TEXT("Select an SVG file first.");
		return FReply::Handled();
	}

	USvgMeshGenerator* Generator = NewObject<USvgMeshGenerator>(GetTransientPackage());
	const FSvgMeshBuildResult Result = Generator->BuildFromSvgFile(SvgFilePath, Settings);
	if (!Result.bSuccess)
	{
		DiagnosticsText = FString::Printf(TEXT("Build failed: %s"), *Result.ErrorMessage);
		return FReply::Handled();
	}

	DiagnosticsText = FString::Printf(
		TEXT("Success\nTriangles: %d\nShapes: %d\n%s"),
		Result.Diagnostics.TriangleCount,
		Result.Shapes.Num(),
		*Result.Diagnostics.Message);

	for (const FSvgImportedShape& Shape : Result.Shapes)
	{
		DiagnosticsText += FString::Printf(
			TEXT("\n- Outer points: %d, holes: %d, tris: %d (%s)"),
			Shape.Diagnostics.OuterPointCount,
			Shape.Diagnostics.HoleCount,
			Shape.Diagnostics.TriangleCount,
			*Shape.Diagnostics.Message);
	}

	return FReply::Handled();
}

FText SSvgPreviewPanel::GetDiagnosticsText() const
{
	return FText::FromString(DiagnosticsText);
}


