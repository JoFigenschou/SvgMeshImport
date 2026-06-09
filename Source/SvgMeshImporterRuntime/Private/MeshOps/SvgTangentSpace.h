#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"

struct FSvgTangentSpaceBuilder
{
	static void ApplySmoothNormals(FSvgMeshData& Mesh);
	static void Apply(FSvgMeshData& Mesh, ESvgTangentSpaceMode Mode);
};
