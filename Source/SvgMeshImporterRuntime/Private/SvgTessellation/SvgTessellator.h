#pragma once

#include "CoreMinimal.h"
#include "SvgImportedShape.h"
#include "SvgMeshTypes.h"

class FSvgTessellator
{
public:
	static bool TessellateShape(const FSvgImportedShape& Shape, bool bFlipY, float Scale, FSvgTessellatedCap& OutCap, FString& OutError);
};
