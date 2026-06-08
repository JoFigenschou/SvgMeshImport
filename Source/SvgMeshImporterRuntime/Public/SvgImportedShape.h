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
struct SVGMESHIMPORTERRUNTIME_API FSvgShapeOuterPart
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FVector2D> Points;
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgImportedShape
{
	GENERATED_BODY()

	/** SVG element id when available (e.g. state code on map SVGs). */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FString ShapeName;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FVector2D> Outer;

	/** Disconnected outer rings that share the same SVG id (e.g. islands) merged into one mesh. */
	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FSvgShapeOuterPart> AdditionalOuters;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	TArray<FSvgShapeHole> Holes;

	UPROPERTY(BlueprintReadOnly, Category = "Shape")
	FSvgMeshBuildDiagnostics Diagnostics;
};
