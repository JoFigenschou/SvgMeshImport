#include "SvgMeshActor.h"

#include "Components/SvgShapeMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Rendering/SvgProceduralMeshBuilder.h"
#include "SvgMeshBakeBridge.h"
#include "SvgMeshGenerator.h"
#include "SvgMeshImporterLog.h"
#include "ProceduralMeshComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Editor.h"
#include "UObject/UnrealType.h"
#endif

namespace SvgMeshActorPrivate
{
	constexpr int32 StaleComponentThreshold = 120;
}

ASvgMeshActor::ASvgMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(ProceduralMesh);
}

void ASvgMeshActor::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!GIsEditor || IsRunningGame())
	{
		return;
	}

	int32 GeneratedComponentCount = 0;
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents(StaticMeshComponents);
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (IsValid(StaticMeshComponent)
			&& StaticMeshComponent->GetName().StartsWith(TEXT("ShapeMesh_")))
		{
			++GeneratedComponentCount;
		}
	}

	TArray<USvgShapeMeshComponent*> ProceduralShapeComponents;
	GetComponents(ProceduralShapeComponents);
	for (USvgShapeMeshComponent* ShapeComponent : ProceduralShapeComponents)
	{
		if (IsValid(ShapeComponent) && ShapeComponent != ProceduralMesh)
		{
			++GeneratedComponentCount;
		}
	}

	const bool bHasStaleGeneratedMeshes = GeneratedComponentCount > SvgMeshActorPrivate::StaleComponentThreshold
		|| BuiltShapeMeshCount > SvgMeshActorPrivate::StaleComponentThreshold
		|| ShapeStaticMeshComponents.Num() > SvgMeshActorPrivate::StaleComponentThreshold
		|| ShapeMeshComponents.Num() > SvgMeshActorPrivate::StaleComponentThreshold
		|| ShapeMeshes.Num() > SvgMeshActorPrivate::StaleComponentThreshold;

	if (bHasStaleGeneratedMeshes)
	{
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgMeshActor] '%s' purging %d stale generated shape component(s) (saved count=%d). Use Generate Mesh to regenerate."),
			*GetName(),
			GeneratedComponentCount,
			BuiltShapeMeshCount);
		ResetGeneratedMeshState();
	}
#endif
}

#if WITH_EDITOR
void ASvgMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ASvgMeshActor, MeshMaterial))
	{
		ApplyMeshMaterial();
	}
}
#endif

void ASvgMeshActor::EditorRebuildMesh()
{
	const bool bSuccess = RebuildMesh();
	OnMeshRebuildCompleted.Broadcast(bSuccess);
}

void ASvgMeshActor::EditorClearGeneratedMeshes()
{
	ResetGeneratedMeshState();

	if (ProceduralMesh)
	{
		FSvgProceduralMeshBuilder::ClearMesh(ProceduralMesh);
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' cleared generated shape meshes. BuiltShapeMeshCount reset to 0."),
		*GetName());

#if WITH_EDITOR
	Modify();
#endif
}

void ASvgMeshActor::ResetGeneratedMeshState()
{
	DestroyAllGeneratedShapeComponents();
	ShapeMeshComponents.Reset();
	ShapeStaticMeshComponents.Reset();
	BakedStaticMeshes.Reset();
	ShapeMeshes.Reset();
	BuiltShapeMeshCount = 0;
}

void ASvgMeshActor::ApplyMeshMaterial()
{
	if (!MeshMaterial)
	{
		return;
	}

	int32 AppliedCount = 0;
	for (UStaticMeshComponent* StaticMeshComponent : ShapeStaticMeshComponents)
	{
		if (IsValid(StaticMeshComponent) && StaticMeshComponent->GetStaticMesh())
		{
			StaticMeshComponent->SetMaterial(0, MeshMaterial);
			++AppliedCount;
		}
	}

	for (USvgShapeMeshComponent* ShapeComponent : ShapeMeshComponents)
	{
		if (IsValid(ShapeComponent) && ShapeComponent->GetNumSections() > 0)
		{
			ShapeComponent->SetMaterial(0, MeshMaterial);
			++AppliedCount;
		}
	}

	if (ProceduralMesh && ProceduralMesh->GetNumSections() > 0)
	{
		ProceduralMesh->SetMaterial(0, MeshMaterial);
		++AppliedCount;
	}

	if (AppliedCount == 0)
	{
		return;
	}

	if (MeshMaterial->IsTwoSided())
	{
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgMeshActor] '%s' material '%s' is two-sided. Disable Two Sided on the material for correct single-sided lighting with +Z component-local normals."),
			*GetName(),
			*MeshMaterial->GetName());
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' applied material '%s' to %d shape mesh component(s)."),
		*GetName(),
		*MeshMaterial->GetName(),
		AppliedCount);
}

void ASvgMeshActor::DestroyAllGeneratedShapeComponents()
{
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
	}
#endif

	TArray<USvgShapeMeshComponent*> ProceduralShapeComponents;
	GetComponents(ProceduralShapeComponents);
	for (USvgShapeMeshComponent* ShapeComponent : ProceduralShapeComponents)
	{
		if (!IsValid(ShapeComponent) || ShapeComponent == ProceduralMesh)
		{
			continue;
		}

		ShapeComponent->UnregisterComponent();
		ShapeComponent->DestroyComponent();
	}

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents(StaticMeshComponents);
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (!IsValid(StaticMeshComponent) || !StaticMeshComponent->GetName().StartsWith(TEXT("ShapeMesh_")))
		{
			continue;
		}

		StaticMeshComponent->UnregisterComponent();
		StaticMeshComponent->DestroyComponent();
	}
}

void ASvgMeshActor::ClearShapeMeshComponents()
{
	DestroyAllGeneratedShapeComponents();

	for (TObjectPtr<USvgShapeMeshComponent>& ShapeComponent : ShapeMeshComponents)
	{
		if (!IsValid(ShapeComponent))
		{
			continue;
		}

		ShapeComponent->UnregisterComponent();
		ShapeComponent->DestroyComponent();
	}

	ShapeMeshComponents.Reset();
}

void ASvgMeshActor::ClearBakedStaticMeshComponents()
{
	DestroyAllGeneratedShapeComponents();

	for (TObjectPtr<UStaticMeshComponent>& StaticMeshComponent : ShapeStaticMeshComponents)
	{
		if (!IsValid(StaticMeshComponent))
		{
			continue;
		}

		StaticMeshComponent->UnregisterComponent();
		StaticMeshComponent->DestroyComponent();
	}

	ShapeStaticMeshComponents.Reset();
	BakedStaticMeshes.Reset();
}

void ASvgMeshActor::ApplyBakedStaticMeshShapes(
	const TArray<FSvgBakedShapeMeshEntry>& Entries,
	const FString& InBakedAssetsRootPath)
{
	ClearBakedStaticMeshComponents();

	if (!ProceduralMesh)
	{
		return;
	}

	BakedAssetsRootPath = InBakedAssetsRootPath;

	for (const FSvgBakedShapeMeshEntry& Entry : Entries)
	{
		if (!IsValid(Entry.StaticMesh))
		{
			continue;
		}

		const FName ComponentName(*Entry.ComponentName);
		UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(
			this,
			UStaticMeshComponent::StaticClass(),
			ComponentName,
			RF_Transactional);
		StaticMeshComponent->SetupAttachment(ProceduralMesh);
		StaticMeshComponent->SetStaticMesh(Entry.StaticMesh);
		StaticMeshComponent->SetRelativeLocation(Entry.RelativeLocation);
		StaticMeshComponent->SetVisibility(true);
		StaticMeshComponent->SetHiddenInGame(false);
		StaticMeshComponent->CreationMethod = EComponentCreationMethod::Instance;
		StaticMeshComponent->RegisterComponent();

		ShapeStaticMeshComponents.Add(StaticMeshComponent);
		BakedStaticMeshes.Add(Entry.StaticMesh);
	}
}

bool ASvgMeshActor::TryApplyBakedStaticMeshes(const FSvgMeshBuildResult& Result)
{
#if WITH_EDITOR
	if (!bBakeToStaticMeshes || !GSvgBakeStaticMeshesDelegate.IsBound())
	{
		return false;
	}

	return GSvgBakeStaticMeshesDelegate.Execute(this, Result);
#else
	return false;
#endif
}

void ASvgMeshActor::ApplyShapeMeshesAsComponents(const TArray<FSvgShapeMesh>& InShapeMeshes)
{
	ClearBakedStaticMeshComponents();
	ClearShapeMeshComponents();

	if (!ProceduralMesh)
	{
		return;
	}

	FSvgProceduralMeshBuilder::ClearMesh(ProceduralMesh);

	int32 CreatedCount = 0;
	for (const FSvgShapeMesh& ShapeMesh : InShapeMeshes)
	{
		if (ShapeMesh.MeshData.Vertices.IsEmpty() || ShapeMesh.MeshData.Triangles.Num() < 3)
		{
			continue;
		}

		FString SafeShapeName = ShapeMesh.Shape.ShapeName;
		SafeShapeName.ReplaceInline(TEXT(" "), TEXT("_"));
		if (SafeShapeName.IsEmpty())
		{
			SafeShapeName = FString::Printf(TEXT("%d"), CreatedCount);
		}

		const FName ComponentName(*FString::Printf(TEXT("ShapeMesh_%d_%s"), CreatedCount, *SafeShapeName));
		USvgShapeMeshComponent* ShapeComponent = NewObject<USvgShapeMeshComponent>(
			this,
			USvgShapeMeshComponent::StaticClass(),
			ComponentName,
			RF_Transactional);
		ShapeComponent->SetupAttachment(ProceduralMesh);
		ShapeComponent->SetVisibility(true);
		ShapeComponent->SetHiddenInGame(false);
		ShapeComponent->CreationMethod = EComponentCreationMethod::Instance;
		ShapeComponent->RegisterComponent();

		FSvgProceduralMeshBuilder::ApplyMeshData(ShapeComponent, ShapeMesh.MeshData, bCreateCollision, 0);
		ShapeMeshComponents.Add(ShapeComponent);
		++CreatedCount;
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' created %d shape mesh component(s) under '%s'. Move the SvgMeshActor to reposition the map (individual shape components are not movable)."),
		*GetName(),
		CreatedCount,
		*ProceduralMesh->GetName());
}

bool ASvgMeshActor::GetShapeMesh(int32 ShapeIndex, FSvgShapeMesh& OutShapeMesh) const
{
	if (!ShapeMeshes.IsValidIndex(ShapeIndex))
	{
		return false;
	}

	OutShapeMesh = ShapeMeshes[ShapeIndex];
	return true;
}

USvgShapeMeshComponent* ASvgMeshActor::GetShapeMeshComponent(int32 ShapeIndex) const
{
	return ShapeMeshComponents.IsValidIndex(ShapeIndex) ? ShapeMeshComponents[ShapeIndex] : nullptr;
}

UStaticMeshComponent* ASvgMeshActor::GetShapeStaticMeshComponent(int32 ShapeIndex) const
{
	return ShapeStaticMeshComponents.IsValidIndex(ShapeIndex) ? ShapeStaticMeshComponents[ShapeIndex] : nullptr;
}

void ASvgMeshActor::BeginPlay()
{
	Super::BeginPlay();

	const UWorld* World = GetWorld();
	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] BeginPlay '%s' World=%s NetMode=%d bBuildOnBeginPlay=%s SvgFilePath='%s'"),
		*GetName(),
		World ? *World->GetName() : TEXT("null"),
		World ? static_cast<int32>(World->GetNetMode()) : -1,
		bBuildOnBeginPlay ? TEXT("true") : TEXT("false"),
		*SvgFilePath);

	if (!bBuildOnBeginPlay)
	{
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgMeshActor] '%s' skipped mesh build because bBuildOnBeginPlay is false."),
			*GetName());
		return;
	}

	const bool bBuilt = RebuildMesh();
	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' BeginPlay RebuildMesh -> %s"),
		*GetName(),
		bBuilt ? TEXT("SUCCESS") : TEXT("FAILED"));
}

bool ASvgMeshActor::RebuildMesh()
{
	LastBuildError.Reset();
	ResetGeneratedMeshState();

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' RebuildMesh SvgFilePath='%s' UnionShapes=%s ExtrudeDepth=%.3f ExtrudeDir=%s Scale=%.3f FlipX=%s FlipY=%s Collision=%s"),
		*GetName(),
		*SvgFilePath,
		MeshSettings.bUnionShapes ? TEXT("true") : TEXT("false"),
		MeshSettings.ExtrudeDepth,
		MeshSettings.bExtrudeAlongPositiveZ ? TEXT("+Z") : TEXT("-Z"),
		MeshSettings.SvgScale,
		MeshSettings.bFlipX ? TEXT("true") : TEXT("false"),
		MeshSettings.bFlipY ? TEXT("true") : TEXT("false"),
		bCreateCollision ? TEXT("true") : TEXT("false"));

	if (SvgFilePath.IsEmpty())
	{
		LastBuildError = TEXT("SvgFilePath is empty.");
		UE_LOG(LogSvgMeshImporter, Error, TEXT("[SvgMeshActor] '%s' %s"), *GetName(), *LastBuildError);
		return false;
	}

	if (!ProceduralMesh)
	{
		LastBuildError = TEXT("ProceduralMesh component is missing.");
		UE_LOG(LogSvgMeshImporter, Error, TEXT("[SvgMeshActor] '%s' %s"), *GetName(), *LastBuildError);
		return false;
	}

	UE_LOG(LogSvgMeshImporter, Verbose,
		TEXT("[SvgMeshActor] '%s' ProceduralMesh visible=%s hiddenInGame=%s sections=%d material[0]=%s"),
		*GetName(),
		ProceduralMesh->IsVisible() ? TEXT("true") : TEXT("false"),
		ProceduralMesh->bHiddenInGame ? TEXT("true") : TEXT("false"),
		ProceduralMesh->GetNumSections(),
		ProceduralMesh->GetMaterial(0) ? *ProceduralMesh->GetMaterial(0)->GetName() : TEXT("NONE"));

	USvgMeshGenerator* Generator = NewObject<USvgMeshGenerator>(this);
	const FSvgMeshBuildResult Result = Generator->BuildFromSvgFile(SvgFilePath, MeshSettings);
	if (!Result.bSuccess)
	{
		LastBuildError = Result.ErrorMessage;
		ShapeMeshes.Reset();
		ClearBakedStaticMeshComponents();
		ClearShapeMeshComponents();
		FSvgProceduralMeshBuilder::ClearMesh(ProceduralMesh);
		UE_LOG(LogSvgMeshImporter, Error,
			TEXT("[SvgMeshActor] '%s' build failed: %s"),
			*GetName(),
			*LastBuildError);
		return false;
	}

	MeshSettings.bUnionShapes = Result.bUnionShapesUsed;

	ShapeMeshes = Result.ShapeMeshes;

	if (!Result.ShapeMeshes.IsEmpty())
	{
		const bool bUsedStaticMeshes = TryApplyBakedStaticMeshes(Result);
		if (bUsedStaticMeshes)
		{
			BuiltShapeMeshCount = ShapeStaticMeshComponents.Num();
		}
		else
		{
			ApplyShapeMeshesAsComponents(Result.ShapeMeshes);
			BuiltShapeMeshCount = ShapeMeshComponents.Num();
		}

		if (BuiltShapeMeshCount == 0)
		{
			LastBuildError = TEXT("Parsed shapes but no valid mesh components were created.");
			UE_LOG(LogSvgMeshImporter, Error,
				TEXT("[SvgMeshActor] '%s' %s"),
				*GetName(),
				*LastBuildError);
			return false;
		}

		if (BuiltShapeMeshCount == 1 && Result.ShapeMeshes.Num() > 1)
		{
			UE_LOG(LogSvgMeshImporter, Warning,
				TEXT("[SvgMeshActor] '%s' only 1 of %d parsed shape(s) produced renderable geometry."),
				*GetName(),
				Result.ShapeMeshes.Num());
		}
	}
	else
	{
		ClearBakedStaticMeshComponents();
		ClearShapeMeshComponents();
		FSvgProceduralMeshBuilder::ClearMesh(ProceduralMesh);
		FSvgProceduralMeshBuilder::ApplyMeshData(ProceduralMesh, Result.MeshData, bCreateCollision);
		BuiltShapeMeshCount = ProceduralMesh->GetNumSections();
	}

	ApplyMeshMaterial();

#if WITH_EDITOR
	Modify();
#endif

	FBox CombinedBounds(ForceInit);
	for (UStaticMeshComponent* StaticMeshComponent : ShapeStaticMeshComponents)
	{
		if (IsValid(StaticMeshComponent))
		{
			CombinedBounds += StaticMeshComponent->Bounds.GetBox();
		}
	}
	for (USvgShapeMeshComponent* ShapeComponent : ShapeMeshComponents)
	{
		if (IsValid(ShapeComponent))
		{
			CombinedBounds += ShapeComponent->Bounds.GetBox();
		}
	}
	if (ProceduralMesh && ProceduralMesh->GetNumSections() > 0)
	{
		CombinedBounds += ProceduralMesh->Bounds.GetBox();
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' mesh applied builtShapeMeshes=%d shapeData=%d proceduralComponents=%d staticMeshComponents=%d bakeToStaticMeshes=%s unionShapes=%s verts=%d tris=%d boundsMin=%s boundsMax=%s actorLoc=%s"),
		*GetName(),
		BuiltShapeMeshCount,
		ShapeMeshes.Num(),
		ShapeMeshComponents.Num(),
		ShapeStaticMeshComponents.Num(),
		bBakeToStaticMeshes ? TEXT("true") : TEXT("false"),
		MeshSettings.bUnionShapes ? TEXT("true") : TEXT("false"),
		Result.MeshData.Vertices.Num(),
		Result.MeshData.Triangles.Num() / 3,
		*CombinedBounds.Min.ToString(),
		*CombinedBounds.Max.ToString(),
		*GetActorLocation().ToString());

	if (!MeshMaterial)
	{
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgMeshActor] '%s' Mesh Material is not set. Assign Mesh Material on the actor (SVG category) or meshes will be invisible."),
			*GetName());
	}

	return true;
}
