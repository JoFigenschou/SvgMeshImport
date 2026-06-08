#pragma once

#include "CoreMinimal.h"
#include "SvgImportedShape.generated.h"

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgMeshBuildDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Diagnostics")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Diagnostics")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Diagnostics")
	int32 OuterPointCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Diagnostics")
	int32 HoleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Diagnostics")
	int32 TriangleCount = 0;
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgShapeHole
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FVector2D> Points;
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgImportedShape
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FVector2D> Outer;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FSvgShapeHole> Holes;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FSvgMeshBuildDiagnostics Diagnostics;
};
