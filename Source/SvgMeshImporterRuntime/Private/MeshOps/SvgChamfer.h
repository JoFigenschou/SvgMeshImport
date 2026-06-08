#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"

class FSvgChamfer
{
public:
	static void ApplyChamfer(FSvgMeshData& InOutMesh, const FSvgMeshSettings& Settings);
};
