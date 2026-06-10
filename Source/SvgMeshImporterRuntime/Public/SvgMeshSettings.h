#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.generated.h"

UENUM(BlueprintType)
enum class ESvgWindingRule : uint8
{
	NonZero UMETA(DisplayName = "Non-Zero"),
	EvenOdd UMETA(DisplayName = "Even-Odd")
};

/** Debug/experimentation: cycle through tangent-space recipes until side-wall lighting looks correct. */
UENUM(BlueprintType)
enum class ESvgTangentSpaceMode : uint8
{
	Default UMETA(DisplayName = "0 - Up x Normal"),
	NormalCrossUp UMETA(DisplayName = "1 - Normal x Up"),
	UpCrossNormal_FlipY UMETA(DisplayName = "2 - Up x Normal, Flip Y"),
	NormalCrossUp_FlipY UMETA(DisplayName = "3 - Normal x Up, Flip Y"),
	FaceNormals UMETA(DisplayName = "4 - Smooth Face Normals + Up x Normal"),
	NegateSideNormals UMETA(DisplayName = "5 - Negate Side Normals + Up x Normal"),
	FromUVGradient UMETA(DisplayName = "6 - UV Gradient Tangent"),
	UVEdgeDirection UMETA(DisplayName = "7 - UV Edge Direction"),
	FlipSideWinding UMETA(DisplayName = "8 - Flip Side Winding + Face Normals"),
	WorldXProjected UMETA(DisplayName = "9 - World X Projected"),
	WorldYProjected UMETA(DisplayName = "10 - World Y Projected"),
	ForwardCrossNormal UMETA(DisplayName = "11 - Forward x Normal"),
	RightCrossNormal UMETA(DisplayName = "12 - Right x Normal"),
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgMeshSettings
{
	GENERATED_BODY()

	/** Depth in SVG units (multiplied by Svg Scale for the final mesh). Top cap stays at Z = 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (ClampMin = "0"))
	float ExtrudeDepth = 10.f;

	/** When true, extrude from the SVG plane along +Z. When false, extrude along -Z (top cap stays at Z = 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	bool bExtrudeAlongPositiveZ = false;

	/** Reverse extrusion side wall winding and normals. Enable if side faces appear inside-out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	bool bFlipExtrusionSides = true;

	/** Cycle through tangent-space recipes to find correct side-wall lighting. Regenerate mesh after changing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings|Debug")
	ESvgTangentSpaceMode TangentSpaceMode = ESvgTangentSpaceMode::NegateSideNormals;

	/** When true, smooth side-wall normals only. Top/bottom caps keep flat normals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	bool bGenerateSmoothNormals = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	float ChamferDistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (ClampMin = "1", ClampMax = "16"))
	int32 ChamferSegments = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (ClampMin = "0.01"))
	float CurveTolerance = 0.25f;

	/** Douglas-Peucker style simplification applied after union. Set to 0 to preserve full path detail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (ClampMin = "0"))
	float SimplifyTolerance = 0.f;

	/** Drop outline segments shorter than this when cleaning paths. Set to 0 to keep all segments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (ClampMin = "0"))
	float MinEdgeLength = 0.f;

	/** Ignore filled islands smaller than this area (square SVG units after scale). Set to 0 to keep all shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (ClampMin = "0"))
	float MinShapeArea = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	float SvgScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	bool bGenerateUVs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (DisplayName = "Flip Y"))
	bool bFlipY = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (DisplayName = "Flip X"))
	bool bFlipX = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	ESvgWindingRule WindingRule = ESvgWindingRule::NonZero;

	/** When false, each SVG path becomes its own shape/mesh. Auto-disabled when the file has multiple filled paths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	bool bUnionShapes = false;

	/** When true, inner contours (e.g. letter O, lakes) are cut out as holes instead of filled islands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	bool bCutHoles = true;

	/** Minimum filled SVG path elements before Union Shapes is auto-disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings", meta = (ClampMin = "2"))
	int32 AutoSeparatePathThreshold = 2;

	/** Move the built mesh so the SVG bounds center sits on the actor origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Mesh Settings")
	bool bCenterMesh = true;
};
