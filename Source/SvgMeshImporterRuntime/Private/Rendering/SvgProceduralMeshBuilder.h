#pragma once

#include "CoreMinimal.h"
#include "SvgMeshTypes.h"

class UProceduralMeshComponent;

class FSvgProceduralMeshBuilder
{
public:
	static void ApplyMeshData(UProceduralMeshComponent* Target, const FSvgMeshData& MeshData, bool bCreateCollision, int32 SectionIndex = 0);
	static void ClearMesh(UProceduralMeshComponent* Target);
};
