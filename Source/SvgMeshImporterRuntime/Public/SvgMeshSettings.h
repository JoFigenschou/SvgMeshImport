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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	float ChamferDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ChamferSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG", meta = (ClampMin = "0.01"))
	float CurveTolerance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	float SvgScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bGenerateUVs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bFlipY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	ESvgWindingRule WindingRule = ESvgWindingRule::NonZero;
};
