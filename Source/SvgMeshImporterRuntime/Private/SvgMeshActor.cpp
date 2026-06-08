#include "SvgMeshActor.h"

#include "Rendering/SvgProceduralMeshBuilder.h"
#include "SvgMeshGenerator.h"
#include "ProceduralMeshComponent.h"

ASvgMeshActor::ASvgMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	SetRootComponent(ProceduralMesh);
}

void ASvgMeshActor::BeginPlay()
{
	Super::BeginPlay();
	if (bBuildOnBeginPlay)
	{
		RebuildMesh();
	}
}

bool ASvgMeshActor::RebuildMesh()
{
	LastBuildError.Reset();
	if (SvgFilePath.IsEmpty())
	{
		LastBuildError = TEXT("SvgFilePath is empty.");
		return false;
	}
	if (!ProceduralMesh)
	{
		LastBuildError = TEXT("ProceduralMesh component is missing.");
		return false;
	}

	USvgMeshGenerator* Generator = NewObject<USvgMeshGenerator>(this);
	const FSvgMeshBuildResult Result = Generator->BuildFromSvgFile(SvgFilePath, MeshSettings);
	if (!Result.bSuccess)
	{
		LastBuildError = Result.ErrorMessage;
		FSvgProceduralMeshBuilder::ClearMesh(ProceduralMesh);
		return false;
	}

	FSvgProceduralMeshBuilder::ApplyMeshData(ProceduralMesh, Result.MeshData, bCreateCollision);
	return true;
}
