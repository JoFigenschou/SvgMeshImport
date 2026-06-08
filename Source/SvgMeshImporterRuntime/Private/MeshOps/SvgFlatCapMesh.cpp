#include "MeshOps/SvgFlatCapMesh.h"

#include "SvgMeshImporterLog.h"
#include "ProceduralMeshComponent.h"

namespace SvgFlatCapMeshPrivate
{
	static FVector ComputeFaceNormal(const FSvgMeshData& Mesh, int32 I0, int32 I1, int32 I2)
	{
		const FVector& P0 = Mesh.Vertices[I0];
		const FVector& P1 = Mesh.Vertices[I1];
		const FVector& P2 = Mesh.Vertices[I2];
		return FVector::CrossProduct(P1 - P2, P0 - P2);
	}
}

void FSvgFlatCapMesh::FixWindingForComponentLocalPositiveZ(FSvgMeshData& Mesh)
{
	int32 FlippedCount = 0;
	for (int32 T = 0; T < Mesh.Triangles.Num(); T += 3)
	{
		const int32 I0 = Mesh.Triangles[T];
		const int32 I1 = Mesh.Triangles[T + 1];
		const int32 I2 = Mesh.Triangles[T + 2];
		const FVector FaceNormal = SvgFlatCapMeshPrivate::ComputeFaceNormal(Mesh, I0, I1, I2);
		if (FaceNormal.Z < 0.f)
		{
			Swap(Mesh.Triangles[T + 1], Mesh.Triangles[T + 2]);
			++FlippedCount;
		}
	}

	UE_LOG(LogSvgMeshImporter, Verbose,
		TEXT("[SvgFlatCapMesh] Component-local +Z winding fix flipped %d triangle(s)."),
		FlippedCount);
}

void FSvgFlatCapMesh::ApplyComponentLocalFlatBasis(FSvgMeshData& Mesh)
{
	const FVector ComponentLocalUp = FVector::UpVector;
	const FProcMeshTangent ComponentLocalTangent(0.f, -1.f, 0.f);

	Mesh.Normals.Init(ComponentLocalUp, Mesh.Vertices.Num());
	Mesh.Tangents.Init(ComponentLocalTangent, Mesh.Vertices.Num());
}
