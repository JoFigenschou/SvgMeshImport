#include "Rendering/SvgProceduralMeshBuilder.h"

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

	TArray<FLinearColor> VertexColors;
	VertexColors.Init(FLinearColor::White, MeshData.Vertices.Num());

	TArray<FProcMeshTangent> Tangents = MeshData.Tangents;
	if (Tangents.Num() != MeshData.Vertices.Num())
	{
		Tangents.SetNum(MeshData.Vertices.Num());
		for (int32 I = 0; I < Tangents.Num(); ++I)
		{
			const FVector Normal = MeshData.Normals.IsValidIndex(I) ? MeshData.Normals[I] : FVector::UpVector;
			const FVector TangentX = FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal();
			Tangents[I] = FProcMeshTangent(TangentX, false);
		}
	}

	TArray<FVector2D> UV0 = MeshData.UV0;
	if (UV0.Num() != MeshData.Vertices.Num())
	{
		UV0.Init(FVector2D::ZeroVector, MeshData.Vertices.Num());
	}

	TArray<FVector> Normals = MeshData.Normals;
	if (Normals.Num() != MeshData.Vertices.Num())
	{
		Normals.Init(FVector::UpVector, MeshData.Vertices.Num());
	}

	Target->CreateMeshSection_LinearColor(
		SectionIndex,
		MeshData.Vertices,
		MeshData.Triangles,
		Normals,
		UV0,
		VertexColors,
		Tangents,
		bCreateCollision);

	FBox MeshBounds(ForceInit);
	for (const FVector& V : MeshData.Vertices)
	{
		MeshBounds += V;
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[ProceduralMeshBuilder] '%s' section=%d verts=%d tris=%d collision=%s boundsMin=%s boundsMax=%s material=%s"),
		*Target->GetName(),
		SectionIndex,
		MeshData.Vertices.Num(),
		MeshData.Triangles.Num() / 3,
		bCreateCollision ? TEXT("true") : TEXT("false"),
		*MeshBounds.Min.ToString(),
		*MeshBounds.Max.ToString(),
		Target->GetMaterial(SectionIndex) ? *Target->GetMaterial(SectionIndex)->GetName() : TEXT("NONE"));
}
