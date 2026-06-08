#pragma once

#include "CoreMinimal.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"
#include "SvgMeshGenerator.generated.h"

class UProceduralMeshComponent;

UCLASS(BlueprintType)
class SVGMESHIMPORTERRUNTIME_API USvgMeshGenerator : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SVG Mesh")
	FSvgMeshBuildResult BuildFromSvgFile(const FString& FilePath, const FSvgMeshSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "SVG Mesh")
	FSvgMeshBuildResult BuildFromSvgString(const FString& SvgContent, const FSvgMeshSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "SVG Mesh")
	bool BuildFromSvgFileToMesh(const FString& FilePath, const FSvgMeshSettings& Settings, UProceduralMeshComponent* TargetMesh, bool bCreateCollision = true);

	UFUNCTION(BlueprintCallable, Category = "SVG Mesh")
	bool BuildFromSvgStringToMesh(const FString& SvgContent, const FSvgMeshSettings& Settings, UProceduralMeshComponent* TargetMesh, bool bCreateCollision = true);

	FSvgMeshBuildResult BuildFromSvgStringInternal(const FString& SvgContent, const FSvgMeshSettings& Settings, const FString& SourceLabel);
};
