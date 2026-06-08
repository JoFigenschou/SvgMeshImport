#include "MeshOps/SvgChamfer.h"

namespace SvgChamferPrivate
{
	static void AppendTriangle(FSvgMeshData& Mesh, int32 A, int32 B, int32 C, const FVector& Normal)
	{
		const int32 Base = Mesh.Vertices.Num();
		Mesh.Vertices.Add(Mesh.Vertices[A]);
		Mesh.Vertices.Add(Mesh.Vertices[B]);
		Mesh.Vertices.Add(Mesh.Vertices[C]);
		Mesh.Normals.Add(Normal);
		Mesh.Normals.Add(Normal);
		Mesh.Normals.Add(Normal);
		Mesh.Triangles.Add(Base);
		Mesh.Triangles.Add(Base + 1);
		Mesh.Triangles.Add(Base + 2);
	}
}

void FSvgChamfer::ApplyChamfer(FSvgMeshData& InOutMesh, const FSvgMeshSettings& Settings)
{
	const float Dist = Settings.ChamferDistance;
	const int32 Segments = FMath::Clamp(Settings.ChamferSegments, 1, 16);
	if (Dist <= KINDA_SMALL_NUMBER || InOutMesh.Vertices.IsEmpty())
	{
		return;
	}

	TArray<FVector> NewVerts;
	TArray<FVector> NewNormals;
	TArray<int32> NewTris;

	for (int32 T = 0; T < InOutMesh.Triangles.Num(); T += 3)
	{
		const int32 I0 = InOutMesh.Triangles[T];
		const int32 I1 = InOutMesh.Triangles[T + 1];
		const int32 I2 = InOutMesh.Triangles[T + 2];
		const FVector& V0 = InOutMesh.Vertices[I0];
		const FVector& V1 = InOutMesh.Vertices[I1];
		const FVector& V2 = InOutMesh.Vertices[I2];

		const bool TopCap = FMath::IsNearlyZero(V0.Z) && FMath::IsNearlyZero(V1.Z) && FMath::IsNearlyZero(V2.Z);
		if (!TopCap)
		{
			const int32 Base = NewVerts.Num();
			NewVerts.Add(V0);
			NewVerts.Add(V1);
			NewVerts.Add(V2);
			NewNormals.Add(InOutMesh.Normals.IsValidIndex(I0) ? InOutMesh.Normals[I0] : FVector::UpVector);
			NewNormals.Add(InOutMesh.Normals.IsValidIndex(I1) ? InOutMesh.Normals[I1] : FVector::UpVector);
			NewNormals.Add(InOutMesh.Normals.IsValidIndex(I2) ? InOutMesh.Normals[I2] : FVector::UpVector);
			NewTris.Add(Base);
			NewTris.Add(Base + 1);
			NewTris.Add(Base + 2);
			continue;
		}

		auto ChamferCorner = [&](const FVector& Corner, const FVector& Prev, const FVector& Next)
		{
			FVector DirA = (Prev - Corner).GetSafeNormal2D();
			FVector DirB = (Next - Corner).GetSafeNormal2D();
			if (DirA.IsNearlyZero() || DirB.IsNearlyZero())
			{
				return Corner;
			}
			const float UseDist = FMath::Min(Dist, FVector2D::Distance(FVector2D(Corner), FVector2D(Prev)) * 0.45f);
			return Corner + (DirA + DirB).GetSafeNormal2D() * UseDist;
		};

		const FVector C0 = ChamferCorner(V0, V2, V1);
		const FVector C1 = ChamferCorner(V1, V0, V2);
		const FVector C2 = ChamferCorner(V2, V1, V0);

		const int32 Base = NewVerts.Num();
		NewVerts.Add(C0);
		NewVerts.Add(C1);
		NewVerts.Add(C2);
		NewNormals.Add(FVector::UpVector);
		NewNormals.Add(FVector::UpVector);
		NewNormals.Add(FVector::UpVector);
		NewTris.Add(Base);
		NewTris.Add(Base + 1);
		NewTris.Add(Base + 2);
	}

	InOutMesh.Vertices = MoveTemp(NewVerts);
	InOutMesh.Normals = MoveTemp(NewNormals);
	InOutMesh.Triangles = MoveTemp(NewTris);
}
