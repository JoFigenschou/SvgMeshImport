#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SvgMeshSettings.h"
#include "SvgMeshActor.generated.h"

class UProceduralMeshComponent;

UCLASS(Blueprintable)
class SVGMESHIMPORTERRUNTIME_API ASvgMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ASvgMeshActor();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG")
	TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	FString SvgFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	FSvgMeshSettings MeshSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bBuildOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bCreateCollision = true;

	UFUNCTION(BlueprintCallable, Category = "SVG")
	bool RebuildMesh();

	UPROPERTY(BlueprintReadOnly, Category = "SVG")
	FString LastBuildError;
};
