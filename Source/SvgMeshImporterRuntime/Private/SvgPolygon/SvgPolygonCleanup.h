#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"

struct FSvgPolygonCleanup
{
	static void CleanRing(TArray<FVector2D>& Poly, const FSvgMeshSettings& Settings);
	static double ComputeArea(const TArray<FVector2D>& Poly);
};
