#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"
#include "SvgImportedShape.h"

class FSvgParser
{
public:
	static bool Parse(const FString& SvgContent, FSvgMeshSettings& Settings, TArray<FSvgImportedShape>& OutShapes, FString& OutError);
};
