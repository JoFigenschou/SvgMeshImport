#pragma once

#include "CoreMinimal.h"
#include "SvgMeshTypes.h"
#include "SvgMeshBakeBridge.generated.h"

class ASvgMeshActor;
class UStaticMesh;

USTRUCT(BlueprintType)
struct SVGMESHIMPORTERRUNTIME_API FSvgBakedShapeMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Baked Shape")
	TObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Baked Shape")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Baked Shape")
	FString ComponentName;

	UPROPERTY(BlueprintReadOnly, Category = "Baked Shape")
	int32 ShapeIndex = INDEX_NONE;
};

/** Editor module binds this to bake per-shape static mesh assets. */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FSvgBakeStaticMeshesDelegate, ASvgMeshActor*, const FSvgMeshBuildResult&);

extern SVGMESHIMPORTERRUNTIME_API FSvgBakeStaticMeshesDelegate GSvgBakeStaticMeshesDelegate;
