#include "MeshOps/SvgExtruder.h"

#include "SvgMeshImporterLog.h"

namespace SvgExtruderPrivate
{
	static TArray<FVector2D> BuildBoundaryPolygon(const FSvgTessellatedCap& TopCap)
	{
		TArray<FVector2D> Polygon;
		if (TopCap.BoundaryEdges.Num() < 6)
		{
			return Polygon;
		}

		TMap<int32, int32> NextVertex;
		NextVertex.Reserve(TopCap.BoundaryEdges.Num() / 2);
		for (int32 E = 0; E < TopCap.BoundaryEdges.Num(); E += 2)
		{
			NextVertex.Add(TopCap.BoundaryEdges[E], TopCap.BoundaryEdges[E + 1]);
		}

		const int32 Start = TopCap.BoundaryEdges[0];
		int32 Current = Start;
		do
		{
			if (!TopCap.Vertices.IsValidIndex(Current))
			{
				break;
			}

			Polygon.Add(FVector2D(TopCap.Vertices[Current].X, TopCap.Vertices[Current].Y));
			const int32* Next = NextVertex.Find(Current);
			if (!Next)
			{
				break;
			}

			Current = *Next;
		}
		while (Current != Start && Polygon.Num() <= NextVertex.Num());

		return Polygon;
	}

	static float ComputeSignedArea2D(const TArray<FVector2D>& Polygon)
	{
		if (Polygon.Num() < 3)
		{
			return 0.f;
		}

		double Area = 0.0;
		for (int32 I = 0; I < Polygon.Num(); ++I)
		{
			const FVector2D& A = Polygon[I];
			const FVector2D& B = Polygon[(I + 1) % Polygon.Num()];
			Area += static_cast<double>(A.X) * B.Y - static_cast<double>(B.X) * A.Y;
		}

		return static_cast<float>(Area * 0.5);
	}

	static FVector ComputeOutwardSideNormal(
		const FVector& Edge,
		bool bCounterClockwiseBoundary,
		bool bFlipExtrusionSides)
	{
		FVector SideNormal = bCounterClockwiseBoundary
			? FVector::CrossProduct(Edge, FVector::UpVector)
			: FVector::CrossProduct(FVector::UpVector, Edge);
		SideNormal.Z = 0.f;
		if (!SideNormal.Normalize())
		{
			SideNormal = FVector(1.f, 0.f, 0.f);
		}

		if (bFlipExtrusionSides)
		{
			SideNormal = -SideNormal;
		}

		return SideNormal;
	}

	static FVector ComputeHoleSideNormalForMode5(
		const FVector& Edge,
		bool bFlipExtrusionSides)
	{
		// Hole rings are clockwise, but mode 5 negates all side normals uniformly.
		// Use the CCW outward formula so the shared negate matches outer walls.
		return ComputeOutwardSideNormal(Edge, true, bFlipExtrusionSides);
	}

	static FVector ComputeFaceNormal(
		const TArray<FVector>& Vertices,
		int32 I0,
		int32 I1,
		int32 I2)
	{
		return FVector::CrossProduct(
			Vertices[I1] - Vertices[I0],
			Vertices[I2] - Vertices[I0]).GetSafeNormal();
	}

	static void AddTriangleWithNormal(
		TArray<int32>& Triangles,
		const TArray<FVector>& Vertices,
		int32 I0,
		int32 I1,
		int32 I2,
		const FVector& DesiredNormal)
	{
		if (FVector::DotProduct(ComputeFaceNormal(Vertices, I0, I1, I2), DesiredNormal) < 0.f)
		{
			Triangles.Append({ I0, I2, I1 });
		}
		else
		{
			Triangles.Append({ I0, I1, I2 });
		}
	}

	static void AddSideQuad(
		TArray<int32>& Triangles,
		const TArray<FVector>& Vertices,
		int32 V0,
		int32 V1,
		int32 V2,
		int32 V3,
		const FVector& SideNormal)
	{
		// V0=top A, V1=top B, V2=bottom B, V3=bottom A.
		AddTriangleWithNormal(Triangles, Vertices, V0, V1, V2, SideNormal);
		AddTriangleWithNormal(Triangles, Vertices, V0, V2, V3, SideNormal);
	}

	static void ExtrudeBoundaryEdgeList(
		FSvgMeshData& InOutMesh,
		const TArray<int32>& BoundaryEdges,
		int32 BottomOffset,
		bool bCounterClockwiseBoundary,
		bool bFlipExtrusionSides,
		float MinEdgeLengthSq,
		int32 SideBase,
		int32& SideVertexCount,
		int32& SkippedEdges,
		bool bMarkHoleSideVertices)
	{
		for (int32 E = 0; E < BoundaryEdges.Num(); E += 2)
		{
			const int32 ATop = BoundaryEdges[E];
			const int32 BTop = BoundaryEdges[E + 1];
			if (!InOutMesh.Vertices.IsValidIndex(ATop) || !InOutMesh.Vertices.IsValidIndex(BTop))
			{
				continue;
			}

			const FVector TA = InOutMesh.Vertices[ATop];
			const FVector TB = InOutMesh.Vertices[BTop];
			const FVector BA = InOutMesh.Vertices[ATop + BottomOffset];
			const FVector BB = InOutMesh.Vertices[BTop + BottomOffset];
			if (MinEdgeLengthSq > 0.f && FVector::DistSquared(TA, TB) < MinEdgeLengthSq)
			{
				++SkippedEdges;
				continue;
			}

			const FVector Edge = TB - TA;
			const FVector SideNormal = bMarkHoleSideVertices
				? ComputeHoleSideNormalForMode5(Edge, bFlipExtrusionSides)
				: ComputeOutwardSideNormal(Edge, bCounterClockwiseBoundary, bFlipExtrusionSides);

			const int32 V0 = SideBase + SideVertexCount;
			SideVertexCount += 4;
			InOutMesh.Vertices.Add(TA);
			InOutMesh.Vertices.Add(TB);
			InOutMesh.Vertices.Add(BB);
			InOutMesh.Vertices.Add(BA);
			for (int32 N = 0; N < 4; ++N)
			{
				InOutMesh.Normals.Add(SideNormal);
				InOutMesh.bIsHoleSideVertex.Add(bMarkHoleSideVertices);
			}

			const float EdgeLength = Edge.Size();
			InOutMesh.UV0.Add(FVector2D(0.f, 0.f));
			InOutMesh.UV0.Add(FVector2D(EdgeLength, 0.f));
			InOutMesh.UV0.Add(FVector2D(EdgeLength, 1.f));
			InOutMesh.UV0.Add(FVector2D(0.f, 1.f));

			AddSideQuad(
				InOutMesh.Triangles,
				InOutMesh.Vertices,
				V0,
				V0 + 1,
				V0 + 2,
				V0 + 3,
				SideNormal);
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
	const TArray<FVector2D> OuterPolygon = SvgExtruderPrivate::BuildBoundaryPolygon(TopCap);
	const bool bCounterClockwiseBoundary = OuterPolygon.Num() >= 3
		? SvgExtruderPrivate::ComputeSignedArea2D(OuterPolygon) > 0.f
		: true;
	const int32 BottomOffset = TopCount;
	InOutMesh.Vertices.Reserve(TopCount * 2);
	InOutMesh.Normals.Reserve(TopCount * 2);
	InOutMesh.UV0.Reserve(TopCount * 2);
	InOutMesh.bIsHoleSideVertex.Reserve(TopCount * 2);

	for (const FVector& V : TopCap.Vertices)
	{
		InOutMesh.Vertices.Add(V);
		InOutMesh.Normals.Add(FVector::UpVector);
		InOutMesh.bIsHoleSideVertex.Add(false);
	}

	const FVector BottomCapNormal = bExtrudeAlongPositiveZ ? FVector::UpVector : -FVector::UpVector;
	for (const FVector& V : TopCap.Vertices)
	{
		InOutMesh.Vertices.Add(FVector(V.X, V.Y, V.Z + BottomZOffset));
		InOutMesh.Normals.Add(BottomCapNormal);
		InOutMesh.bIsHoleSideVertex.Add(false);
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
	int32 SideVertexCount = 0;
	int32 SkippedEdges = 0;

	const int32 HoleEdgeCount = TopCap.HoleBoundaryEdges.Num() / 2;

	SvgExtruderPrivate::ExtrudeBoundaryEdgeList(
		InOutMesh,
		TopCap.BoundaryEdges,
		BottomOffset,
		bCounterClockwiseBoundary,
		bFlipExtrusionSides,
		MinEdgeLengthSq,
		SideBase,
		SideVertexCount,
		SkippedEdges,
		false);

	SvgExtruderPrivate::ExtrudeBoundaryEdgeList(
		InOutMesh,
		TopCap.HoleBoundaryEdges,
		BottomOffset,
		bCounterClockwiseBoundary,
		bFlipExtrusionSides,
		MinEdgeLengthSq,
		SideBase,
		SideVertexCount,
		SkippedEdges,
		true);

	UE_LOG(LogSvgMeshImporter, Verbose,
		TEXT("[SvgExtruder] Extruded %s Z by %.3f flipSides=%s outerBoundaryVerts=%d holeBoundaryEdges=%d (skipped %d short edges)"),
		bExtrudeAlongPositiveZ ? TEXT("+") : TEXT("-"),
		Depth,
		bFlipExtrusionSides ? TEXT("true") : TEXT("false"),
		OuterPolygon.Num(),
		HoleEdgeCount,
		SkippedEdges);
}
