#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.generated.h"

UENUM(BlueprintType)
enum class ESvgWindingRule : uint8
{
	NonZero UMETA(DisplayName = "Non-Zero"),
	EvenOdd UMETA(DisplayName = "Even-Odd")
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgMeshSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ExtrudeDepth = 0.f;

	/** When true, extrude from the SVG plane along +Z. When false, extrude along -Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bExtrudeAlongPositiveZ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ChamferDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "16"))
	int32 ChamferSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float CurveTolerance = 0.25f;

	/** Douglas-Peucker style simplification applied after union. Set to 0 to preserve full path detail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float SimplifyTolerance = 0.f;

	/** Drop outline segments shorter than this when cleaning paths. Set to 0 to keep all segments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MinEdgeLength = 0.f;

	/** Ignore filled islands smaller than this area (square SVG units after scale). Set to 0 to keep all shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MinShapeArea = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float SvgScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bGenerateUVs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Flip Y"))
	bool bFlipY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Flip X"))
	bool bFlipX = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESvgWindingRule WindingRule = ESvgWindingRule::NonZero;

	/** When false, each SVG path becomes its own shape/mesh. Auto-disabled when the file has multiple filled paths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUnionShapes = false;

	/** Minimum filled SVG path elements before Union Shapes is auto-disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "2"))
	int32 AutoSeparatePathThreshold = 2;

	/** Move the built mesh so the SVG bounds center sits on the actor origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCenterMesh = true;
};
