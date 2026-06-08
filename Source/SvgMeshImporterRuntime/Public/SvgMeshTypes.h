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

	/** Pairs of vertex indices describing cap boundary segments. */
	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<int32> BoundaryEdges;
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
};

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgMeshBuildResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FSvgMeshData MeshData;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	TArray<FSvgImportedShape> Shapes;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	FSvgMeshBuildDiagnostics Diagnostics;
};
