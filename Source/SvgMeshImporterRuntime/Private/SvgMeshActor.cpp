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

	bool IsStaticMeshRenderable(const UStaticMesh* StaticMesh)
	{
		if (!IsValid(StaticMesh))
		{
			return false;
		}

		const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
		return RenderData && RenderData->LODResources.Num() > 0;
	}

	void DestroyShapeComponent(UActorComponent* Component)
	{
		if (!Component)
		{
			return;
		}

		if (Component->IsRegistered())
		{
			Component->UnregisterComponent();
		}

		Component->Rename(
			nullptr,
			GetTransientPackage(),
			REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		Component->DestroyComponent();
	}

	void ReleaseComponentObjectSlot(AActor* Owner, FName SlotName)
	{
		if (!Owner || SlotName.IsNone())
		{
			return;
		}

		UObject* Existing = StaticFindObject(UObject::StaticClass(), Owner, *SlotName.ToString());
		if (!Existing)
		{
			return;
		}

		if (UActorComponent* ExistingComponent = Cast<UActorComponent>(Existing))
		{
			DestroyShapeComponent(ExistingComponent);
			return;
		}

		Existing->Rename(
			nullptr,
			GetTransientPackage(),
			REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_NonTransactional);
		Existing->MarkAsGarbage();
	}

	bool IsRuntimeOrPieWorld(const UWorld* World)
	{
		return World && World->IsGameWorld();
	}

	EObjectFlags GetShapeComponentObjectFlags(const UWorld* World)
	{
		return IsRuntimeOrPieWorld(World) ? RF_Transient : RF_Transactional;
	}

	void EnsureMovableShapeHierarchy(ASvgMeshActor* Actor)
	{
		if (!Actor || !Actor->ProceduralMesh)
		{
			return;
		}

		Actor->ProceduralMesh->SetMobility(EComponentMobility::Movable);

		TArray<UStaticMeshComponent*> StaticMeshComponents;
		Actor->GetComponents(StaticMeshComponents);
		for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
		{
			if (!IsValid(StaticMeshComponent) || !StaticMeshComponent->GetName().StartsWith(TEXT("ShapeMesh_")))
			{
				continue;
			}

			StaticMeshComponent->SetMobility(EComponentMobility::Movable);
		}

		TArray<USvgShapeMeshComponent*> ProceduralShapeComponents;
		Actor->GetComponents(ProceduralShapeComponents);
		for (USvgShapeMeshComponent* ShapeComponent : ProceduralShapeComponents)
		{
			if (!IsValid(ShapeComponent) || ShapeComponent == Actor->ProceduralMesh)
			{
				continue;
			}

			ShapeComponent->SetMobility(EComponentMobility::Movable);
		}
	}

	void DestroyAllShapeMeshSubobjects(AActor* Actor, UProceduralMeshComponent* RootProceduralMesh)
	{
		if (!Actor)
		{
			return;
		}

		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(Actor, Subobjects, false);
		for (UObject* Subobject : Subobjects)
		{
			UActorComponent* Component = Cast<UActorComponent>(Subobject);
			if (!Component || Component == RootProceduralMesh)
			{
				continue;
			}

			if (!Component->GetName().StartsWith(TEXT("ShapeMesh_")))
			{
				continue;
			}

			if (Component->IsA<USvgShapeMeshComponent>() || Component->IsA<UStaticMeshComponent>())
			{
				DestroyShapeComponent(Component);
			}
		}
	}
}

ASvgMeshActor::ASvgMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProceduralMesh->SetMobility(EComponentMobility::Movable);
	SetRootComponent(ProceduralMesh);
}

void ASvgMeshActor::PostLoad()
{
	Super::PostLoad();

	SvgMeshActorPrivate::EnsureMovableShapeHierarchy(this);

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
		if (IsValid(StaticMeshComponent)
			&& SvgMeshActorPrivate::IsStaticMeshRenderable(StaticMeshComponent->GetStaticMesh()))
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

		SvgMeshActorPrivate::DestroyShapeComponent(ShapeComponent);
	}

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents(StaticMeshComponents);
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (!IsValid(StaticMeshComponent) || !StaticMeshComponent->GetName().StartsWith(TEXT("ShapeMesh_")))
		{
			continue;
		}

		SvgMeshActorPrivate::DestroyShapeComponent(StaticMeshComponent);
	}

	SvgMeshActorPrivate::DestroyAllShapeMeshSubobjects(this, ProceduralMesh);
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

		SvgMeshActorPrivate::DestroyShapeComponent(ShapeComponent);
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

		SvgMeshActorPrivate::DestroyShapeComponent(StaticMeshComponent);
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

	SvgMeshActorPrivate::EnsureMovableShapeHierarchy(this);
	BakedAssetsRootPath = InBakedAssetsRootPath;

	for (const FSvgBakedShapeMeshEntry& Entry : Entries)
	{
		if (!SvgMeshActorPrivate::IsStaticMeshRenderable(Entry.StaticMesh))
		{
			UE_LOG(LogSvgMeshImporter, Warning,
				TEXT("[SvgMeshActor] '%s' skipping baked shape '%s' because the static mesh has no render data."),
				*GetName(),
				*Entry.ComponentName);
			continue;
		}

		const FName ComponentName(*Entry.ComponentName);
		SvgMeshActorPrivate::ReleaseComponentObjectSlot(this, ComponentName);
		UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(
			this,
			UStaticMeshComponent::StaticClass(),
			ComponentName,
			SvgMeshActorPrivate::GetShapeComponentObjectFlags(GetWorld()));
		StaticMeshComponent->SetMobility(EComponentMobility::Movable);
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

	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
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

	SvgMeshActorPrivate::EnsureMovableShapeHierarchy(this);
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
		SvgMeshActorPrivate::ReleaseComponentObjectSlot(this, ComponentName);
		USvgShapeMeshComponent* ShapeComponent = NewObject<USvgShapeMeshComponent>(
			this,
			USvgShapeMeshComponent::StaticClass(),
			ComponentName,
			SvgMeshActorPrivate::GetShapeComponentObjectFlags(GetWorld()));
		ShapeComponent->SetMobility(EComponentMobility::Movable);
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
		int32 RemovedInvalidComponents = 0;
		for (int32 ComponentIndex = ShapeStaticMeshComponents.Num() - 1; ComponentIndex >= 0; --ComponentIndex)
		{
			UStaticMeshComponent* StaticMeshComponent = ShapeStaticMeshComponents[ComponentIndex];
			if (!IsValid(StaticMeshComponent)
				|| !SvgMeshActorPrivate::IsStaticMeshRenderable(StaticMeshComponent->GetStaticMesh()))
			{
				SvgMeshActorPrivate::DestroyShapeComponent(StaticMeshComponent);
				ShapeStaticMeshComponents.RemoveAt(ComponentIndex);
				if (BakedStaticMeshes.IsValidIndex(ComponentIndex))
				{
					BakedStaticMeshes.RemoveAt(ComponentIndex);
				}
				++RemovedInvalidComponents;
			}
		}

		BuiltShapeMeshCount = ShapeStaticMeshComponents.Num();

		if (RemovedInvalidComponents > 0)
		{
			UE_LOG(LogSvgMeshImporter, Warning,
				TEXT("[SvgMeshActor] '%s' removed %d invalid baked static mesh component(s) before play. Use Generate Mesh in the editor to rebuild."),
				*GetName(),
				RemovedInvalidComponents);
		}

		UE_LOG(LogSvgMeshImporter, Log,
			TEXT("[SvgMeshActor] '%s' skipped mesh build because bBuildOnBeginPlay is false. staticMeshComponents=%d proceduralComponents=%d"),
			*GetName(),
			ShapeStaticMeshComponents.Num(),
			ShapeMeshComponents.Num());
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
		TEXT("[SvgMeshActor] '%s' RebuildMesh SvgFilePath='%s' UnionShapes=%s ExtrudeDepth=%.3f ExtrudeDir=%s FlipSides=%s Scale=%.3f FlipX=%s FlipY=%s Collision=%s"),
		*GetName(),
		*SvgFilePath,
		MeshSettings.bUnionShapes ? TEXT("true") : TEXT("false"),
		MeshSettings.ExtrudeDepth,
		MeshSettings.bExtrudeAlongPositiveZ ? TEXT("+Z") : TEXT("-Z"),
		MeshSettings.bFlipExtrusionSides ? TEXT("true") : TEXT("false"),
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
			if (bBakeToStaticMeshes)
			{
				const UWorld* World = GetWorld();
				if (World && World->IsGameWorld())
				{
					UE_LOG(LogSvgMeshImporter, Log,
						TEXT("[SvgMeshActor] '%s' building procedural shape meshes for runtime/PIE (static mesh baking is editor-only)."),
						*GetName());
				}
			}

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
	if (!SvgMeshActorPrivate::IsRuntimeOrPieWorld(GetWorld()))
	{
		Modify();
	}
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
