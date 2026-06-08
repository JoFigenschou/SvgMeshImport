#include "Rendering/SvgProceduralMeshBuilder.h"

#include "ProceduralMeshComponent.h"

void FSvgProceduralMeshBuilder::ClearMesh(UProceduralMeshComponent* Target)
{
	if (!Target)
	{
		return;
	}
	Target->ClearAllMeshSections();
}

void FSvgProceduralMeshBuilder::ApplyMeshData(UProceduralMeshComponent* Target, const FSvgMeshData& MeshData, bool bCreateCollision, int32 SectionIndex)
{
	if (!Target)
	{
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
}
