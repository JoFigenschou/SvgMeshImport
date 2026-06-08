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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	float ExtrudeDepth = 10.f;

	/** When true, extrude from the SVG plane along +Z. When false, extrude along -Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bExtrudeAlongPositiveZ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	float ChamferDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ChamferSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG", meta = (ClampMin = "0.01"))
	float CurveTolerance = 0.5f;

	/** Douglas-Peucker style simplification applied after union. Increase for noisy or over-dense SVG paths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG", meta = (ClampMin = "0"))
	float SimplifyTolerance = 2.f;

	/** Drop outline segments shorter than this when cleaning paths and building extrusion walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG", meta = (ClampMin = "0"))
	float MinEdgeLength = 1.f;

	/** Ignore filled islands smaller than this area (square SVG units after scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG", meta = (ClampMin = "0"))
	float MinShapeArea = 16.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	float SvgScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bGenerateUVs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bFlipY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	ESvgWindingRule WindingRule = ESvgWindingRule::NonZero;
};
