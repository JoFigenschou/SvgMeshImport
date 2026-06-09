#include "MeshOps/SvgUvGenerator.h"

void FSvgUvGenerator::GenerateUVs(FSvgMeshData& InOutMesh, const FSvgMeshSettings& Settings, const FBox2D& Bounds2D)
{
	if (!Settings.bGenerateUVs)
	{
		InOutMesh.UV0.Init(FVector2D::ZeroVector, InOutMesh.Vertices.Num());
		return;
	}

	const FVector2D Min = Bounds2D.Min;
	const FVector2D Size = Bounds2D.GetSize();
	const float UScale = Size.X > KINDA_SMALL_NUMBER ? 1.f / Size.X : 1.f;
	const float VScale = Size.Y > KINDA_SMALL_NUMBER ? 1.f / Size.Y : 1.f;

	if (InOutMesh.UV0.Num() != InOutMesh.Vertices.Num())
	{
		InOutMesh.UV0.SetNum(InOutMesh.Vertices.Num());
	}

	for (int32 I = 0; I < InOutMesh.Vertices.Num(); ++I)
	{
		const FVector Normal = InOutMesh.Normals.IsValidIndex(I)
			? InOutMesh.Normals[I].GetSafeNormal()
			: FVector::UpVector;

		// Side walls already carry edge/height UVs from the extruder.
		if (FMath::Abs(Normal.Z) < 0.5f)
		{
			continue;
		}

		const FVector& V = InOutMesh.Vertices[I];
		const float U = (V.X - Min.X) * UScale;
		const float VCoord = (V.Y - Min.Y) * VScale;
		InOutMesh.UV0[I] = FVector2D(U, VCoord);
	}
}
