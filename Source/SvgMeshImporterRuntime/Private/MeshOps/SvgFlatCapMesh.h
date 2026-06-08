#pragma once

#include "CoreMinimal.h"
#include "SvgMeshTypes.h"

/** Utilities for flat XY caps in procedural mesh component local space (+Z normal). */
struct FSvgFlatCapMesh
{
	static void FixWindingForComponentLocalPositiveZ(FSvgMeshData& Mesh);
	static void ApplyComponentLocalFlatBasis(FSvgMeshData& Mesh);
};
