#include "MeshOps/SvgExtruder.h"

#include "SvgMeshImporterLog.h"

namespace SvgExtruderPrivate
{
	static FVector ComputeFaceNormal(const TArray<FVector>& Vertices, int32 I0, int32 I1, int32 I2)
	{
		return FVector::CrossProduct(
			Vertices[I1] - Vertices[I0],
			Vertices[I2] - Vertices[I0]).GetSafeNormal();
	}

	static void AddSideTriangles(
		TArray<int32>& Triangles,
		int32 V0,
		int32 V1,
		int32 V2,
		int32 V3,
		bool bUsePositiveZExtrudeWinding)
	{
		// V0=top A, V1=top B, V2=bottom B, V3=bottom A.
		if (bUsePositiveZExtrudeWinding)
		{
			Triangles.Append({ V0, V1, V2, V0, V2, V3 });
		}
		else
		{
			Triangles.Append({ V0, V2, V1, V0, V3, V2 });
		}
	}

	static void AssignSideQuadNormals(FSvgMeshData& Mesh, int32 V0)
	{
		const FVector FaceNormalA = ComputeFaceNormal(Mesh.Vertices, V0, V0 + 1, V0 + 2);
		const FVector FaceNormalB = ComputeFaceNormal(Mesh.Vertices, V0, V0 + 2, V0 + 3);
		FVector SideNormal = (FaceNormalA + FaceNormalB).GetSafeNormal();
		if (SideNormal.IsNearlyZero())
		{
			SideNormal = FaceNormalA;
		}

		for (int32 VertexIndex = V0; VertexIndex < V0 + 4; ++VertexIndex)
		{
			Mesh.Normals[VertexIndex] = SideNormal;
		}
	}
}

void FSvgExtruder::Extrude(
	const FSvgTessellatedCap& TopCap,
	float Depth,
	FSvgMeshData& InOutMesh,
	float MinEdgeLength,
	bool bExtrudeAlongPositiveZ,
	bool bFlipExtrusionSides)
{
	InOutMesh = FSvgMeshData();

	const int32 TopCount = TopCap.Vertices.Num();
	if (TopCount < 3 || Depth <= KINDA_SMALL_NUMBER)
	{
		InOutMesh.Vertices = TopCap.Vertices;
		InOutMesh.Triangles = TopCap.Triangles;
		InOutMesh.Normals = TopCap.Normals;
		InOutMesh.UV0 = TopCap.UV0;
		return;
	}

	const float BottomZOffset = bExtrudeAlongPositiveZ ? Depth : -Depth;
	const bool bUsePositiveZExtrudeWinding = bExtrudeAlongPositiveZ != bFlipExtrusionSides;
	const int32 BottomOffset = TopCount;
	InOutMesh.Vertices.Reserve(TopCount * 2);
	InOutMesh.Normals.Reserve(TopCount * 2);
	InOutMesh.UV0.Reserve(TopCount * 2);

	for (const FVector& V : TopCap.Vertices)
	{
		InOutMesh.Vertices.Add(V);
		InOutMesh.Normals.Add(FVector::UpVector);
	}

	const FVector BottomCapNormal = bExtrudeAlongPositiveZ ? FVector::UpVector : -FVector::UpVector;
	for (const FVector& V : TopCap.Vertices)
	{
		InOutMesh.Vertices.Add(FVector(V.X, V.Y, V.Z + BottomZOffset));
		InOutMesh.Normals.Add(BottomCapNormal);
	}

	InOutMesh.Triangles = TopCap.Triangles;
	TArray<int32> BottomTris;
	BottomTris.Reserve(TopCap.Triangles.Num());
	for (int32 I = 0; I < TopCap.Triangles.Num(); I += 3)
	{
		const int32 I0 = TopCap.Triangles[I] + BottomOffset;
		const int32 I1 = TopCap.Triangles[I + 2] + BottomOffset;
		const int32 I2 = TopCap.Triangles[I + 1] + BottomOffset;
		BottomTris.Add(I0);
		BottomTris.Add(I1);
		BottomTris.Add(I2);
	}
	InOutMesh.Triangles.Append(BottomTris);

	const int32 SideBase = InOutMesh.Vertices.Num();
	const float MinEdgeLengthSq = FMath::Square(FMath::Max(0.f, MinEdgeLength));
	int32 SkippedEdges = 0;
	for (int32 E = 0; E < TopCap.BoundaryEdges.Num(); E += 2)
	{
		const int32 ATop = TopCap.BoundaryEdges[E];
		const int32 BTop = TopCap.BoundaryEdges[E + 1];
		const int32 ABot = ATop + BottomOffset;
		const int32 BBot = BTop + BottomOffset;

		const FVector TA = InOutMesh.Vertices[ATop];
		const FVector TB = InOutMesh.Vertices[BTop];
		const FVector BA = InOutMesh.Vertices[ABot];
		const FVector BB = InOutMesh.Vertices[BBot];
		if (MinEdgeLengthSq > 0.f && FVector::DistSquared(TA, TB) < MinEdgeLengthSq)
		{
			++SkippedEdges;
			continue;
		}

		const int32 V0 = SideBase + (E / 2) * 4;
		InOutMesh.Vertices.Add(TA);
		InOutMesh.Vertices.Add(TB);
		InOutMesh.Vertices.Add(BB);
		InOutMesh.Vertices.Add(BA);
		InOutMesh.Normals.AddDefaulted(4);

		SvgExtruderPrivate::AddSideTriangles(
			InOutMesh.Triangles,
			V0,
			V0 + 1,
			V0 + 2,
			V0 + 3,
			bUsePositiveZExtrudeWinding);

		SvgExtruderPrivate::AssignSideQuadNormals(InOutMesh, V0);
	}

	UE_LOG(LogSvgMeshImporter, Verbose,
		TEXT("[SvgExtruder] Extruded %s Z by %.3f flipSides=%s (skipped %d short edges)"),
		bExtrudeAlongPositiveZ ? TEXT("+") : TEXT("-"),
		Depth,
		bFlipExtrusionSides ? TEXT("true") : TEXT("false"),
		SkippedEdges);
}
