#include "Rendering/SvgProceduralMeshBuilder.h"

#include "MeshOps/SvgFlatCapMesh.h"
#include "SvgMeshImporterLog.h"
#include "ProceduralMeshComponent.h"

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
	FSvgFlatCapMesh::FixWindingForComponentLocalPositiveZ(LocalMesh);
	FSvgFlatCapMesh::ApplyComponentLocalFlatBasis(LocalMesh);

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
		bCreateCollision);

	FBox MeshBounds(ForceInit);
	for (const FVector& V : LocalMesh.Vertices)
	{
		MeshBounds += V;
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[ProceduralMeshBuilder] '%s' section=%d verts=%d tris=%d collision=%s boundsMin=%s boundsMax=%s material=%s (component-local normals)"),
		*Target->GetName(),
		SectionIndex,
		LocalMesh.Vertices.Num(),
		LocalMesh.Triangles.Num() / 3,
		bCreateCollision ? TEXT("true") : TEXT("false"),
		*MeshBounds.Min.ToString(),
		*MeshBounds.Max.ToString(),
		Target->GetMaterial(SectionIndex) ? *Target->GetMaterial(SectionIndex)->GetName() : TEXT("NONE"));
}
