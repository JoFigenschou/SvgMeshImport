#include "Rendering/SvgProceduralMeshBuilder.h"

#include "MeshOps/SvgFlatCapMesh.h"
#include "SvgMeshImporterLog.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"

namespace SvgProceduralMeshBuilderPrivate
{
	static FBox ComputeVertexBounds(const TArray<FVector>& Vertices)
	{
		FBox Bounds(ForceInit);
		for (const FVector& Vertex : Vertices)
		{
			Bounds += Vertex;
		}
		return Bounds;
	}

	static void ApplySimpleBoxCollision(UProceduralMeshComponent* Target, const FBox& LocalBounds, bool bCreateCollision)
	{
		if (!Target)
		{
			return;
		}

		Target->bUseComplexAsSimpleCollision = false;

		if (!bCreateCollision || !LocalBounds.IsValid)
		{
			Target->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Target->ClearCollisionConvexMeshes();
			return;
		}

		UBodySetup* BodySetup = Target->GetBodySetup();
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

		Target->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Target->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Target->RecreatePhysicsState();
	}

	static bool UsesFlatCapBasis(const FSvgMeshData& Mesh)
	{
		if (Mesh.Normals.Num() != Mesh.Vertices.Num())
		{
			return true;
		}

		for (const FVector& Normal : Mesh.Normals)
		{
			if (!Normal.Equals(FVector::UpVector, KINDA_SMALL_NUMBER))
			{
				return false;
			}
		}

		return true;
	}
}

void FSvgProceduralMeshBuilder::ClearMesh(UProceduralMeshComponent* Target)
{
	if (!Target)
	{
		UE_LOG(LogSvgMeshImporter, Warning, TEXT("[ProceduralMeshBuilder] ClearMesh called with null target."));
		return;
	}

	const int32 PreviousSections = Target->GetNumSections();
	Target->ClearAllMeshSections();
	UE_LOG(LogSvgMeshImporter, Verbose,
		TEXT("[ProceduralMeshBuilder] Cleared mesh on '%s' (removed %d section(s))."),
		*Target->GetName(),
		PreviousSections);
}

void FSvgProceduralMeshBuilder::ApplyMeshData(UProceduralMeshComponent* Target, const FSvgMeshData& MeshData, bool bCreateCollision, int32 SectionIndex)
{
	if (!Target)
	{
		UE_LOG(LogSvgMeshImporter, Warning, TEXT("[ProceduralMeshBuilder] ApplyMeshData called with null target."));
		return;
	}

	if (MeshData.Vertices.IsEmpty() || MeshData.Triangles.Num() < 3)
	{
		UE_LOG(LogSvgMeshImporter, Error,
			TEXT("[ProceduralMeshBuilder] '%s' invalid mesh data verts=%d tris=%d"),
			*Target->GetName(),
			MeshData.Vertices.Num(),
			MeshData.Triangles.Num() / 3);
		return;
	}

	FSvgMeshData LocalMesh = MeshData;
	if (SvgProceduralMeshBuilderPrivate::UsesFlatCapBasis(LocalMesh))
	{
		FSvgFlatCapMesh::FixWindingForComponentLocalPositiveZ(LocalMesh);
		FSvgFlatCapMesh::ApplyComponentLocalFlatBasis(LocalMesh);
	}

	TArray<FLinearColor> VertexColors;
	VertexColors.Init(FLinearColor::White, LocalMesh.Vertices.Num());

	TArray<FVector2D> UV0 = LocalMesh.UV0;
	if (UV0.Num() != LocalMesh.Vertices.Num())
	{
		UV0.Init(FVector2D::ZeroVector, LocalMesh.Vertices.Num());
	}

	Target->CreateMeshSection_LinearColor(
		SectionIndex,
		LocalMesh.Vertices,
		LocalMesh.Triangles,
		LocalMesh.Normals,
		UV0,
		VertexColors,
		LocalMesh.Tangents,
		false);

	const FBox MeshBounds = SvgProceduralMeshBuilderPrivate::ComputeVertexBounds(LocalMesh.Vertices);
	SvgProceduralMeshBuilderPrivate::ApplySimpleBoxCollision(Target, MeshBounds, bCreateCollision);

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[ProceduralMeshBuilder] '%s' section=%d verts=%d tris=%d collision=%s boundsMin=%s boundsMax=%s material=%s (component-local normals)"),
		*Target->GetName(),
		SectionIndex,
		LocalMesh.Vertices.Num(),
		LocalMesh.Triangles.Num() / 3,
		bCreateCollision ? TEXT("box") : TEXT("false"),
		*MeshBounds.Min.ToString(),
		*MeshBounds.Max.ToString(),
		Target->GetMaterial(SectionIndex) ? *Target->GetMaterial(SectionIndex)->GetName() : TEXT("NONE"));
}

void FSvgProceduralMeshBuilder::ApplyShapeMeshes(UProceduralMeshComponent* Target, const TArray<FSvgShapeMesh>& ShapeMeshes, bool bCreateCollision)
{
	if (!Target)
	{
		return;
	}

	ClearMesh(Target);

	FBox CombinedBounds(ForceInit);
	int32 AppliedSections = 0;
	for (int32 ShapeIdx = 0; ShapeIdx < ShapeMeshes.Num(); ++ShapeIdx)
	{
		const FSvgShapeMesh& ShapeMesh = ShapeMeshes[ShapeIdx];
		if (ShapeMesh.MeshData.Vertices.IsEmpty() || ShapeMesh.MeshData.Triangles.Num() < 3)
		{
			continue;
		}

		ApplyMeshData(Target, ShapeMesh.MeshData, false, AppliedSections);
		CombinedBounds += SvgProceduralMeshBuilderPrivate::ComputeVertexBounds(ShapeMesh.MeshData.Vertices);
		++AppliedSections;
	}

	if (bCreateCollision)
	{
		SvgProceduralMeshBuilderPrivate::ApplySimpleBoxCollision(Target, CombinedBounds, true);
	}
	else
	{
		SvgProceduralMeshBuilderPrivate::ApplySimpleBoxCollision(Target, FBox(ForceInit), false);
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[ProceduralMeshBuilder] '%s' applied %d shape mesh section(s)."),
		*Target->GetName(),
		AppliedSections);
}
