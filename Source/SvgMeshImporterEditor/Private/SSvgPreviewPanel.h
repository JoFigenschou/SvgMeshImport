#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"
#include "Widgets/SCompoundWidget.h"

class SSvgPreviewPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSvgPreviewPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnBrowseClicked();
	FReply OnBuildPreviewClicked();
	FText GetDiagnosticsText() const;

	FString SvgFilePath;
	FSvgMeshSettings Settings;
	FString DiagnosticsText;
};
