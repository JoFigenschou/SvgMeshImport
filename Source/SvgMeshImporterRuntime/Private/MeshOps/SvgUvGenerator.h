#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"

class FSvgUvGenerator
{
public:
	static void GenerateUVs(FSvgMeshData& InOutMesh, const FSvgMeshSettings& Settings, const FBox2D& Bounds2D);
};
