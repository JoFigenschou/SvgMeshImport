#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SvgMeshSettings.h"
#include "SvgMeshTypes.h"
#include "SvgMeshActor.generated.h"

class UProceduralMeshComponent;
class USvgShapeMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
struct FSvgBakedShapeMeshEntry;
struct FSvgMeshBuildResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSvgMeshRebuildCompleted, bool, bSuccess);

UCLASS(Blueprintable)
class SVGMESHIMPORTERRUNTIME_API ASvgMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ASvgMeshActor();

	virtual void PostLoad() override;

	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Attachment root — move the actor (not individual ShapeMesh children) to reposition the map. */
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

	/** When false (default), meshes are not built automatically in the editor — call EditorRebuildMesh instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bAutoRebuildInEditor = false;

	/** When false (default), changing SVG settings in the editor does not trigger an automatic rebuild. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bAutoRebuildOnPropertyChange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG")
	bool bCreateCollision = true;

	/** Bake each shape to a Static Mesh asset in the editor (Option 6). Old assets are deleted on every rebuild. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Static Meshes")
	bool bBakeToStaticMeshes = true;

	/** Content folder where baked static meshes for this actor are stored (e.g. /Game/SvgMeshImporter/Generated). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVG|Static Meshes", meta = (EditCondition = "bBakeToStaticMeshes"))
	FString StaticMeshOutputPath = TEXT("/Game/SvgMeshImporter/Generated");

	UFUNCTION(BlueprintCallable, Category = "SVG")
	bool RebuildMesh();

	/** Build or rebuild meshes from the editor Details panel button, Blueprints, or editor utility widgets. */
	UFUNCTION(BlueprintCallable, Category = "SVG", meta = (CallInEditor = "true"))
	void EditorRebuildMesh();

	/** Remove all generated shape components without rebuilding (fixes stale/duplicate meshes). */
	UFUNCTION(BlueprintCallable, Category = "SVG", meta = (CallInEditor = "true"))
	void EditorClearGeneratedMeshes();

	/** Fired after EditorRebuildMesh completes (not fired by RebuildMesh alone). */
	UPROPERTY(BlueprintAssignable, Category = "SVG")
	FSvgMeshRebuildCompleted OnMeshRebuildCompleted;

	/** Updated after RebuildMesh; use this to confirm how many shapes were built. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG|Shape Meshes")
	int32 BuiltShapeMeshCount = 0;

	/** One mesh per parsed SVG shape (e.g. per US state). Populated after RebuildMesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG|Shape Meshes")
	TArray<FSvgShapeMesh> ShapeMeshes;

	/** One procedural mesh component per shape; used when bBakeToStaticMeshes is false. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG|Shape Meshes")
	TArray<TObjectPtr<USvgShapeMeshComponent>> ShapeMeshComponents;

	/** Baked static mesh assets (one per shape). Regenerated on every RebuildMesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG|Static Meshes")
	TArray<TObjectPtr<UStaticMesh>> BakedStaticMeshes;

	/** One static mesh component per shape; movable in the editor when bBakeToStaticMeshes is true. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG|Static Meshes")
	TArray<TObjectPtr<UStaticMeshComponent>> ShapeStaticMeshComponents;

	/** Last content path where baked assets were written; cleared and replaced on reimport. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SVG|Static Meshes")
	FString BakedAssetsRootPath;

	UFUNCTION(BlueprintPure, Category = "SVG|Shape Meshes")
	int32 GetShapeMeshCount() const { return ShapeMeshes.Num(); }

	UFUNCTION(BlueprintPure, Category = "SVG|Shape Meshes")
	bool GetShapeMesh(int32 ShapeIndex, FSvgShapeMesh& OutShapeMesh) const;

	UFUNCTION(BlueprintPure, Category = "SVG|Shape Meshes")
	USvgShapeMeshComponent* GetShapeMeshComponent(int32 ShapeIndex) const;

	UFUNCTION(BlueprintPure, Category = "SVG|Static Meshes")
	UStaticMeshComponent* GetShapeStaticMeshComponent(int32 ShapeIndex) const;

	UPROPERTY(BlueprintReadOnly, Category = "SVG")
	FString LastBuildError;

	void ClearShapeMeshComponents();
	void ClearBakedStaticMeshComponents();
	void DestroyAllGeneratedShapeComponents();
	void ResetGeneratedMeshState();
	void ApplyBakedStaticMeshShapes(const TArray<FSvgBakedShapeMeshEntry>& Entries, const FString& InBakedAssetsRootPath);

private:
	void ApplyMeshMaterial();
	void ApplyShapeMeshesAsComponents(const TArray<FSvgShapeMesh>& InShapeMeshes);
	bool TryApplyBakedStaticMeshes(const FSvgMeshBuildResult& Result);
};
