#include "MeshOps/SvgExtruder.h"

void FSvgExtruder::Extrude(const FSvgTessellatedCap& TopCap, float Depth, FSvgMeshData& InOutMesh)
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
		InOutMesh.Vertices.Add(FVector(V.X, V.Y, V.Z - Depth));
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
	for (int32 E = 0; E < TopCap.BoundaryEdges.Num(); E += 2)
	{
		const int32 ATop = TopCap.BoundaryEdges[E];
		const int32 BTop = TopCap.BoundaryEdges[E + 1];
		const int32 ABot = ATop + BottomOffset;
		const int32 BBot = BTop + BottomOffset;

		const FVector& TA = InOutMesh.Vertices[ATop];
		const FVector& TB = InOutMesh.Vertices[BTop];
		const FVector Edge = TB - TA;
		FVector SideNormal = FVector::CrossProduct(Edge, FVector::UpVector);
		SideNormal.Z = 0.f;
		if (!SideNormal.Normalize())
		{
			SideNormal = FVector(1.f, 0.f, 0.f);
		}

		const int32 V0 = SideBase + (E / 2) * 4;
		InOutMesh.Vertices.Add(TA);
		InOutMesh.Vertices.Add(TB);
		InOutMesh.Vertices.Add(InOutMesh.Vertices[BBot]);
		InOutMesh.Vertices.Add(InOutMesh.Vertices[ABot]);
		for (int32 N = 0; N < 4; ++N)
		{
			InOutMesh.Normals.Add(SideNormal);
		}

		InOutMesh.Triangles.Add(V0);
		InOutMesh.Triangles.Add(V0 + 1);
		InOutMesh.Triangles.Add(V0 + 2);
		InOutMesh.Triangles.Add(V0);
		InOutMesh.Triangles.Add(V0 + 2);
		InOutMesh.Triangles.Add(V0 + 3);
	}
}
