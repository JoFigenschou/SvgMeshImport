#include "SvgStaticMeshBaker.h"

#include "SvgMeshActor.h"
#include "SvgMeshBakeBridge.h"
#include "SvgMeshImporterLog.h"
#include "ProceduralMeshComponent.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"

namespace SvgStaticMeshBakerPrivate
{
	void ClearProceduralMesh(UProceduralMeshComponent* Target)
	{
		if (Target)
		{
			Target->ClearAllMeshSections();
		}
	}

	FBox ComputeVertexBounds(const TArray<FVector>& Vertices)
	{
		FBox Bounds(ForceInit);
		for (const FVector& Vertex : Vertices)
		{
			Bounds += Vertex;
		}
		return Bounds;
	}

	void ApplySimpleBoxCollision(UStaticMesh* StaticMesh, const FBox& LocalBounds, bool bBuildCollision)
	{
		if (!StaticMesh || !bBuildCollision || !LocalBounds.IsValid)
		{
			return;
		}

		UBodySetup* BodySetup = StaticMesh->GetBodySetup();
		if (!BodySetup)
		{
			StaticMesh->CreateBodySetup();
			BodySetup = StaticMesh->GetBodySetup();
		}

		if (!BodySetup)
		{
			return;
		}

		BodySetup->AggGeom.BoxElems.Reset();
		BodySetup->AggGeom.ConvexElems.Reset();

		FKBoxElem BoxElem;
		BoxElem.Center = LocalBounds.GetCenter();
		const FVector Size = LocalBounds.GetSize();
		BoxElem.X = FMath::Max(Size.X, KINDA_SMALL_NUMBER);
		BoxElem.Y = FMath::Max(Size.Y, KINDA_SMALL_NUMBER);
		BoxElem.Z = FMath::Max(Size.Z, KINDA_SMALL_NUMBER);
		BodySetup->AggGeom.BoxElems.Add(BoxElem);
		BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseDefault;
		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
	}

	FString SanitizePathSegment(const FString& Name)
	{
		FString Result;
		Result.Reserve(Name.Len());

		for (const TCHAR Character : Name)
		{
			if (FChar::IsAlnum(Character) || Character == TEXT('_'))
			{
				Result.AppendChar(Character);
			}
			else if (Character == TEXT(' ') || Character == TEXT('-'))
			{
				Result.AppendChar(TEXT('_'));
			}
		}

		if (Result.IsEmpty())
		{
			Result = TEXT("SvgMesh");
		}

		return Result;
	}
}

FString FSvgStaticMeshBaker::SanitizeAssetName(const FString& Name)
{
	FString Result = SvgStaticMeshBakerPrivate::SanitizePathSegment(Name);
	if (!Result.IsEmpty() && FChar::IsDigit(Result[0]))
	{
		Result = FString::Printf(TEXT("SM_%s"), *Result);
	}
	return Result;
}

FString FSvgStaticMeshBaker::GetActorBakeRoot(ASvgMeshActor* Actor)
{
	const FString RootPath = Actor->StaticMeshOutputPath.IsEmpty()
		? TEXT("/Game/SvgMeshImporter/Generated")
		: Actor->StaticMeshOutputPath;

	const FString ActorFolder = SvgStaticMeshBakerPrivate::SanitizePathSegment(Actor->GetName());
	return FString::Printf(TEXT("%s/%s"), *RootPath, *ActorFolder);
}

void FSvgStaticMeshBaker::DeleteActorBakedAssets(const FString& BakeRoot)
{
	if (BakeRoot.IsEmpty())
	{
		return;
	}

	if (UEditorAssetLibrary::DoesDirectoryExist(BakeRoot))
	{
		const bool bDeleted = UEditorAssetLibrary::DeleteDirectory(BakeRoot);
		UE_LOG(LogSvgMeshImporter, Log,
			TEXT("[SvgStaticMeshBaker] DeleteDirectory '%s' -> %s"),
			*BakeRoot,
			bDeleted ? TEXT("success") : TEXT("failed"));
	}
}

FSvgMeshData FSvgStaticMeshBaker::ToShapeLocalMesh(const FSvgMeshData& WorldMesh, FVector& OutCenter)
{
	OutCenter = FVector::ZeroVector;

	if (WorldMesh.Vertices.IsEmpty())
	{
		return WorldMesh;
	}

	FBox Bounds(ForceInit);
	for (const FVector& Vertex : WorldMesh.Vertices)
	{
		Bounds += Vertex;
	}

	OutCenter = Bounds.GetCenter();

	FSvgMeshData LocalMesh = WorldMesh;
	for (FVector& Vertex : LocalMesh.Vertices)
	{
		Vertex -= OutCenter;
	}

	return LocalMesh;
}

UStaticMesh* FSvgStaticMeshBaker::CreateStaticMeshAsset(
	const FString& PackageName,
	const FString& AssetName,
	const FSvgMeshData& MeshData,
	bool bBuildCollision)
{
	if (MeshData.Vertices.IsEmpty() || MeshData.Triangles.Num() < 3)
	{
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return nullptr;
	}

	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
		Package,
		*AssetName,
		RF_Public | RF_Standalone | RF_Transactional);
	if (!StaticMesh)
	{
		return nullptr;
	}

	FMeshDescription MeshDescription;
	FStaticMeshAttributes Attributes(MeshDescription);
	Attributes.Register();

	FMeshDescriptionBuilder MeshBuilder;
	MeshBuilder.SetMeshDescription(&MeshDescription);
	MeshBuilder.EnablePolyGroups();
	MeshBuilder.SetNumUVLayers(1);

	const bool bHasNormals = MeshData.Normals.Num() == MeshData.Vertices.Num();
	const bool bHasUVs = MeshData.UV0.Num() == MeshData.Vertices.Num();

	TArray<FVertexInstanceID> VertexInstances;
	VertexInstances.SetNum(MeshData.Vertices.Num());

	for (int32 VertexIndex = 0; VertexIndex < MeshData.Vertices.Num(); ++VertexIndex)
	{
		const FVertexID VertexId = MeshBuilder.AppendVertex(MeshData.Vertices[VertexIndex]);
		const FVertexInstanceID InstanceId = MeshBuilder.AppendInstance(VertexId);
		VertexInstances[VertexIndex] = InstanceId;

		if (bHasNormals)
		{
			const FVector Normal = MeshData.Normals[VertexIndex].GetSafeNormal();
			FVector Tangent(0.f, -1.f, 0.f);
			float BinormalSign = 1.f;

			if (MeshData.Tangents.IsValidIndex(VertexIndex))
			{
				Tangent = MeshData.Tangents[VertexIndex].TangentX.GetSafeNormal();
				BinormalSign = MeshData.Tangents[VertexIndex].bFlipTangentY ? -1.f : 1.f;
			}
			else if (FMath::Abs(Normal.Z) < 0.9f)
			{
				Tangent = FVector::CrossProduct(FVector::UpVector, Normal);
				Tangent.Z = 0.f;
				if (!Tangent.Normalize())
				{
					Tangent = FVector(1.f, 0.f, 0.f);
				}
			}

			MeshBuilder.SetInstanceTangentSpace(InstanceId, Normal, Tangent, BinormalSign);
		}

		if (bHasUVs)
		{
			MeshBuilder.SetInstanceUV(InstanceId, MeshData.UV0[VertexIndex], 0);
		}
	}

	const FPolygonGroupID PolygonGroup = MeshBuilder.AppendPolygonGroup();
	int32 AddedTriangleCount = 0;
	for (int32 TriangleIndex = 0; TriangleIndex < MeshData.Triangles.Num(); TriangleIndex += 3)
	{
		const int32 Index0 = MeshData.Triangles[TriangleIndex];
		const int32 Index1 = MeshData.Triangles[TriangleIndex + 1];
		const int32 Index2 = MeshData.Triangles[TriangleIndex + 2];

		if (!VertexInstances.IsValidIndex(Index0)
			|| !VertexInstances.IsValidIndex(Index1)
			|| !VertexInstances.IsValidIndex(Index2))
		{
			continue;
		}

		MeshBuilder.AppendTriangle(
			VertexInstances[Index0],
			VertexInstances[Index1],
			VertexInstances[Index2],
			PolygonGroup);
		++AddedTriangleCount;
	}

	if (AddedTriangleCount == 0)
	{
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgStaticMeshBaker] Skipping static mesh '%s' because no valid triangles were produced."),
			*AssetName);
		return nullptr;
	}

	StaticMesh->GetStaticMaterials().Add(FStaticMaterial());

	UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
	BuildParams.bBuildSimpleCollision = false;
	BuildParams.bFastBuild = true;

	TArray<const FMeshDescription*> MeshDescriptions;
	MeshDescriptions.Add(&MeshDescription);
	StaticMesh->BuildFromMeshDescriptions(MeshDescriptions, BuildParams);
	StaticMesh->CalculateExtendedBounds();
	SvgStaticMeshBakerPrivate::ApplySimpleBoxCollision(StaticMesh, SvgStaticMeshBakerPrivate::ComputeVertexBounds(MeshData.Vertices), bBuildCollision);

	const FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
	if (!RenderData || RenderData->LODResources.Num() == 0)
	{
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgStaticMeshBaker] Static mesh '%s' has no render LODs after build."),
			*AssetName);
		return nullptr;
	}

	return StaticMesh;
}

bool FSvgStaticMeshBaker::SaveStaticMeshAsset(UStaticMesh* StaticMesh, const FString& PackageName)
{
	if (!StaticMesh)
	{
		return false;
	}

	UPackage* Package = StaticMesh->GetOutermost();
	if (!Package)
	{
		return false;
	}

	Package->MarkPackageDirty();

	FAssetRegistryModule::AssetCreated(StaticMesh);

	const FString Filename = FPackageName::LongPackageNameToFilename(
		PackageName,
		FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	return UPackage::SavePackage(Package, StaticMesh, *Filename, SaveArgs);
}

bool FSvgStaticMeshBaker::BakeAndApply(ASvgMeshActor* Actor, const FSvgMeshBuildResult& Result)
{
	if (!Actor || !Actor->ProceduralMesh)
	{
		return false;
	}

	const FString BakeRoot = GetActorBakeRoot(Actor);
	DeleteActorBakedAssets(BakeRoot);
	DeleteActorBakedAssets(Actor->BakedAssetsRootPath);

	Actor->ClearShapeMeshComponents();
	Actor->ClearBakedStaticMeshComponents();
	SvgStaticMeshBakerPrivate::ClearProceduralMesh(Actor->ProceduralMesh);

	if (Result.ShapeMeshes.IsEmpty())
	{
		Actor->BakedAssetsRootPath = BakeRoot;
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgStaticMeshBaker] '%s' no shape meshes to bake."),
			*Actor->GetName());
		return false;
	}

	TArray<FSvgBakedShapeMeshEntry> BakedEntries;
	BakedEntries.Reserve(Result.ShapeMeshes.Num());

	TSet<FString> UsedAssetNames;
	int32 CreatedCount = 0;

	for (const FSvgShapeMesh& ShapeMesh : Result.ShapeMeshes)
	{
		if (ShapeMesh.MeshData.Vertices.IsEmpty() || ShapeMesh.MeshData.Triangles.Num() < 3)
		{
			continue;
		}

		FVector ShapeCenter = FVector::ZeroVector;
		const FSvgMeshData LocalMeshData = ToShapeLocalMesh(ShapeMesh.MeshData, ShapeCenter);

		FString ShapeLabel = ShapeMesh.Shape.ShapeName;
		ShapeLabel.ReplaceInline(TEXT(" "), TEXT("_"));
		if (ShapeLabel.IsEmpty())
		{
			ShapeLabel = FString::Printf(TEXT("%d"), CreatedCount);
		}

		const FString BaseAssetName = SanitizeAssetName(ShapeLabel);
		FString AssetName = FString::Printf(TEXT("SM_%s"), *BaseAssetName);
		int32 DuplicateSuffix = 0;
		while (UsedAssetNames.Contains(AssetName))
		{
			++DuplicateSuffix;
			AssetName = FString::Printf(TEXT("SM_%s_%d"), *BaseAssetName, DuplicateSuffix);
		}
		UsedAssetNames.Add(AssetName);

		const FString PackageName = FString::Printf(TEXT("%s/%s"), *BakeRoot, *AssetName);
		UStaticMesh* StaticMesh = CreateStaticMeshAsset(
			PackageName,
			AssetName,
			LocalMeshData,
			Actor->bCreateCollision);
		if (!StaticMesh)
		{
			UE_LOG(LogSvgMeshImporter, Warning,
				TEXT("[SvgStaticMeshBaker] '%s' failed to create static mesh for shape '%s'."),
				*Actor->GetName(),
				*ShapeLabel);
			continue;
		}

		if (!SaveStaticMeshAsset(StaticMesh, PackageName))
		{
			UE_LOG(LogSvgMeshImporter, Warning,
				TEXT("[SvgStaticMeshBaker] '%s' failed to save static mesh '%s'."),
				*Actor->GetName(),
				*PackageName);
			continue;
		}

		FSvgBakedShapeMeshEntry Entry;
		Entry.StaticMesh = StaticMesh;
		Entry.RelativeLocation = ShapeCenter;
		Entry.ComponentName = FString::Printf(TEXT("ShapeMesh_%d_%s"), CreatedCount, *SanitizeAssetName(ShapeLabel));
		Entry.ShapeIndex = ShapeMesh.ShapeIndex;
		BakedEntries.Add(Entry);
		++CreatedCount;
	}

	Actor->ApplyBakedStaticMeshShapes(BakedEntries, BakeRoot);

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgStaticMeshBaker] '%s' baked %d static mesh asset(s) under '%s'. Individual ShapeMesh components are movable in the editor."),
		*Actor->GetName(),
		Actor->ShapeStaticMeshComponents.Num(),
		*BakeRoot);

	return Actor->ShapeStaticMeshComponents.Num() > 0;
}
