#include "MeshOps/SvgExtruder.h"

#include "SvgMeshImporterLog.h"

namespace SvgExtruderPrivate
{
	static FVector ComputeOutwardSideNormal(const FVector& Edge)
	{
		FVector SideNormal = FVector::CrossProduct(Edge, FVector::UpVector);
		SideNormal.Z = 0.f;
		if (!SideNormal.Normalize())
		{
			SideNormal = FVector(1.f, 0.f, 0.f);
		}
		return SideNormal;
	}

	static void AddSideTriangles(TArray<int32>& Triangles, int32 V0, int32 V1, int32 V2, int32 V3, const FVector& SideNormal, const TArray<FVector>& Vertices)
	{
		const auto FaceNormal = [&Vertices](int32 A, int32 B, int32 C)
		{
			return FVector::CrossProduct(Vertices[B] - Vertices[A], Vertices[C] - Vertices[A]).GetSafeNormal();
		};

		const int32 TriA[] = { V0, V1, V2 };
		const int32 TriB[] = { V0, V2, V3 };
		if (FVector::DotProduct(FaceNormal(TriA[0], TriA[1], TriA[2]), SideNormal) < 0.f)
		{
			Triangles.Add(TriA[0]);
			Triangles.Add(TriA[2]);
			Triangles.Add(TriA[1]);
		}
		else
		{
			Triangles.Add(TriA[0]);
			Triangles.Add(TriA[1]);
			Triangles.Add(TriA[2]);
		}

		if (FVector::DotProduct(FaceNormal(TriB[0], TriB[1], TriB[2]), SideNormal) < 0.f)
		{
			Triangles.Add(TriB[0]);
			Triangles.Add(TriB[2]);
			Triangles.Add(TriB[1]);
		}
		else
		{
			Triangles.Add(TriB[0]);
			Triangles.Add(TriB[1]);
			Triangles.Add(TriB[2]);
		}
	}
}

void FSvgExtruder::Extrude(
	const FSvgTessellatedCap& TopCap,
	float Depth,
	FSvgMeshData& InOutMesh,
	float MinEdgeLength,
	bool bExtrudeAlongPositiveZ)
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
	const int32 BottomOffset = TopCount;
	InOutMesh.Vertices.Reserve(TopCount * 2);
	InOutMesh.Normals.Reserve(TopCount * 2);
	InOutMesh.UV0.Reserve(TopCount * 2);

	for (const FVector& V : TopCap.Vertices)
	{
		InOutMesh.Vertices.Add(V);
		InOutMesh.Normals.Add(FVector::UpVector);
	}
	for (const FVector& V : TopCap.Vertices)
	{
		InOutMesh.Vertices.Add(FVector(V.X, V.Y, V.Z + BottomZOffset));
		InOutMesh.Normals.Add(-FVector::UpVector);
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
		if (MinEdgeLengthSq > 0.f && FVector::DistSquared(TA, TB) < MinEdgeLengthSq)
		{
			++SkippedEdges;
			continue;
		}

		const FVector BA = InOutMesh.Vertices[ABot];
		const FVector BB = InOutMesh.Vertices[BBot];
		const FVector Edge = TB - TA;
		const FVector SideNormal = SvgExtruderPrivate::ComputeOutwardSideNormal(Edge);

		const int32 V0 = SideBase + (E / 2) * 4;
		InOutMesh.Vertices.Add(TA);
		InOutMesh.Vertices.Add(TB);
		InOutMesh.Vertices.Add(BB);
		InOutMesh.Vertices.Add(BA);
		for (int32 N = 0; N < 4; ++N)
		{
			InOutMesh.Normals.Add(SideNormal);
		}

		SvgExtruderPrivate::AddSideTriangles(InOutMesh.Triangles, V0, V0 + 1, V0 + 2, V0 + 3, SideNormal, InOutMesh.Vertices);
	}

	UE_LOG(LogSvgMeshImporter, Verbose,
		TEXT("[SvgExtruder] Extruded %s Z by %.3f (skipped %d short edges)"),
		bExtrudeAlongPositiveZ ? TEXT("+") : TEXT("-"),
		Depth,
		SkippedEdges);
}
