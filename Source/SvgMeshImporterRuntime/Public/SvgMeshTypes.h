#pragma once

#include "CoreMinimal.h"
#include "SvgImportedShape.h"
#include "ProceduralMeshComponent.h"
#include "SvgMeshTypes.generated.h"

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgTessellatedCap
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector> Vertices;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<int32> Triangles;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector> Normals;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector2D> UV0;

	/** Pairs of vertex indices describing the outer cap boundary segments. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<int32> BoundaryEdges;

	/** Pairs of vertex indices describing hole perimeter segments (clockwise wound). */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<int32> HoleBoundaryEdges;
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgMeshData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector> Vertices;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<int32> Triangles;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector> Normals;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector2D> UV0;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FProcMeshTangent> Tangents;

	/** Per-vertex flag for hole perimeter extrusion walls. */
	TArray<bool> bIsHoleSideVertex;
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgShapeMesh
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shape Mesh")
	int32 ShapeIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Shape Mesh")
	FSvgMeshData MeshData;

	UPROPERTY(BlueprintReadOnly, Category = "Shape Mesh")
	FSvgImportedShape Shape;
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgMeshBuildResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	/** Union Shapes value used for this build (may differ after auto-detection). */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bUnionShapesUsed = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString ErrorMessage;

	/** One entry per parsed shape; primary access for multi-shape SVGs. */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FSvgShapeMesh> ShapeMeshes;

	/** All shape meshes combined into a single buffer (legacy / convenience). */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FSvgMeshData MeshData;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FSvgImportedShape> Shapes;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FSvgMeshBuildDiagnostics Diagnostics;
};
