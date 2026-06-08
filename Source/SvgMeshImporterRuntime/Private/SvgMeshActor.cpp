#include "SvgMeshActor.h"

#include "Rendering/SvgProceduralMeshBuilder.h"
#include "SvgMeshGenerator.h"
#include "SvgMeshImporterLog.h"
#include "ProceduralMeshComponent.h"

#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

ASvgMeshActor::ASvgMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(ProceduralMesh);
}

#if WITH_EDITOR
void ASvgMeshActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ASvgMeshActor, MeshMaterial))
	{
		ApplyMeshMaterial();
	}
}
#endif

void ASvgMeshActor::ApplyMeshMaterial()
{
	if (!ProceduralMesh || !MeshMaterial || ProceduralMesh->GetNumSections() <= 0)
	{
		return;
	}

	ProceduralMesh->SetMaterial(0, MeshMaterial);
	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' applied material '%s' to section 0."),
		*GetName(),
		*MeshMaterial->GetName());
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

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' RebuildMesh SvgFilePath='%s' ExtrudeDepth=%.3f ExtrudeDir=%s Scale=%.3f FlipY=%s Collision=%s"),
		*GetName(),
		*SvgFilePath,
		MeshSettings.ExtrudeDepth,
		MeshSettings.bExtrudeAlongPositiveZ ? TEXT("+Z") : TEXT("-Z"),
		MeshSettings.SvgScale,
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
		FSvgProceduralMeshBuilder::ClearMesh(ProceduralMesh);
		UE_LOG(LogSvgMeshImporter, Error,
			TEXT("[SvgMeshActor] '%s' build failed: %s"),
			*GetName(),
			*LastBuildError);
		return false;
	}

	FSvgProceduralMeshBuilder::ApplyMeshData(ProceduralMesh, Result.MeshData, bCreateCollision);
	ApplyMeshMaterial();

	const FBox SphereBounds = ProceduralMesh->Bounds.GetBox();
	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshActor] '%s' mesh applied verts=%d tris=%d boundsMin=%s boundsMax=%s actorLoc=%s"),
		*GetName(),
		Result.MeshData.Vertices.Num(),
		Result.MeshData.Triangles.Num() / 3,
		*SphereBounds.Min.ToString(),
		*SphereBounds.Max.ToString(),
		*GetActorLocation().ToString());

	if (!ProceduralMesh->GetMaterial(0))
	{
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgMeshActor] '%s' ProceduralMesh section 0 has no material assigned. Assign Mesh Material under SVG or the mesh may be invisible in game."),
			*GetName());
	}

	return true;
}
