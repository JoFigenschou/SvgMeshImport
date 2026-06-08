#pragma once

#include "CoreMinimal.h"
#include "SvgImportedShape.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"

class FSvgTessellator
{
public:
	static bool TessellateShape(const FSvgImportedShape& Shape, const FSvgMeshSettings& Settings, FSvgTessellatedCap& OutCap, FString& OutError);
};
