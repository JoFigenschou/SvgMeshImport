#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"

class FSvgExtruder
{
public:
	static void Extrude(const FSvgTessellatedCap& TopCap, float Depth, FSvgMeshData& InOutMesh, float MinEdgeLength = 1.f);
};
