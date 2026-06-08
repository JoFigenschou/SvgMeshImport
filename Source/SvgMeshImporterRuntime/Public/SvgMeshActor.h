#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"
#include "SvgMeshActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UCLASS(Blueprintable)
class SVGMESHIMPORTERRUNTIME_API ASvgMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ASvgMeshActor();

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG")
	TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	FString SvgFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	FSvgMeshSettings MeshSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	TObjectPtr<UMaterialInterface> MeshMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bBuildOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bCreateCollision = true;

	UFUNCTION(BlueprintCallable, Category = "SVG")
	bool RebuildMesh();

	/** One mesh per parsed SVG shape (e.g. per US state). */
	UPROPERTY(BlueprintReadOnly, Category = "SVG")
	TArray<FSvgShapeMesh> ShapeMeshes;

	UFUNCTION(BlueprintPure, Category = "SVG")
	int32 GetShapeMeshCount() const { return ShapeMeshes.Num(); }

	UFUNCTION(BlueprintPure, Category = "SVG")
	bool GetShapeMesh(int32 ShapeIndex, FSvgShapeMesh& OutShapeMesh) const;

	UPROPERTY(BlueprintReadOnly, Category = "SVG")
	FString LastBuildError;

private:
	void ApplyMeshMaterial();
};
