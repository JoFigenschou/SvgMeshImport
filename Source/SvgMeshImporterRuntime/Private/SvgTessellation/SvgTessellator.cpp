#include "SvgTessellation/SvgTessellator.h"

#include "SvgMeshImporterLog.h"
#include "SvgPolygon/SvgPolygonCleanup.h"
#include "SvgMeshSettings.h"

#include <array>
#include <vector>

THIRD_PARTY_INCLUDES_START
#include "earcut.hpp"
THIRD_PARTY_INCLUDES_END

namespace SvgTessellatorPrivate
{
	static bool IsClockwise(const TArray<FVector2D>& Poly)
	{
		double Sum = 0.0;
		for (int32 I = 0; I < Poly.Num(); ++I)
		{
			const FVector2D& A = Poly[I];
			const FVector2D& B = Poly[(I + 1) % Poly.Num()];
			Sum += (static_cast<double>(B.X) - A.X) * (static_cast<double>(B.Y) + A.Y);
		}
		return Sum > 0.0;
	}

	static TArray<FVector2D> EnsureWinding(const TArray<FVector2D>& Poly, bool bWantClockwise)
	{
		TArray<FVector2D> Out = Poly;
		const bool bCw = IsClockwise(Out);
		if (bCw != bWantClockwise)
		{
			Algo::Reverse(Out);
		}
		return Out;
	}
}

namespace SvgTessellatorPrivate
{
	using Point = std::array<double, 2>;
	using Polygon = std::vector<std::vector<Point>>;

	static bool TessellatePolygonWithHoles(
		const TArray<FVector2D>& Outer,
		const TArray<FSvgShapeHole>& Holes,
		const FSvgMeshSettings& Settings,
		FSvgTessellatedCap& OutCap,
		FString& OutError)
	{
		if (Outer.Num() < 3)
		{
			OutError = TEXT("Outer contour has fewer than 3 points.");
			return false;
		}

		Polygon PolygonData;
		PolygonData.reserve(static_cast<size_t>(1 + Holes.Num()));

		TArray<FVector2D> OuterRingPoints = Outer;
		FSvgPolygonCleanup::CleanRing(OuterRingPoints, Settings);
		OuterRingPoints = EnsureWinding(OuterRingPoints, false);
		std::vector<Point> OuterRing;
		OuterRing.reserve(static_cast<size_t>(OuterRingPoints.Num()));
		for (const FVector2D& P : OuterRingPoints)
		{
			OuterRing.push_back({ static_cast<double>(P.X), static_cast<double>(P.Y) });
		}
		PolygonData.push_back(std::move(OuterRing));

		TArray<TArray<FVector2D>> HoleRings;
		HoleRings.Reserve(Holes.Num());
		for (const FSvgShapeHole& Hole : Holes)
		{
			if (Hole.Points.Num() < 3)
			{
				continue;
			}

			TArray<FVector2D> HoleWound = Hole.Points;
			FSvgPolygonCleanup::CleanRing(HoleWound, Settings);
			HoleWound = EnsureWinding(HoleWound, true);
			std::vector<Point> HoleRing;
			HoleRing.reserve(static_cast<size_t>(HoleWound.Num()));
			for (const FVector2D& P : HoleWound)
			{
				HoleRing.push_back({ static_cast<double>(P.X), static_cast<double>(P.Y) });
			}
			PolygonData.push_back(std::move(HoleRing));
			HoleRings.Add(MoveTemp(HoleWound));
		}

		mapbox::detail::Earcut<uint32_t> Earcut;
		Earcut(PolygonData);
		const std::vector<uint32_t>& Indices = Earcut.indices;
		if (Indices.empty())
		{
			OutError = TEXT("Earcut failed to triangulate shape.");
			return false;
		}

		TArray<FVector2D> All2D;
		All2D.Reserve(OuterRingPoints.Num());
		for (const FVector2D& P : OuterRingPoints)
		{
			All2D.Add(P);
		}
		for (const TArray<FVector2D>& HoleRing : HoleRings)
		{
			for (const FVector2D& P : HoleRing)
			{
				All2D.Add(P);
			}
		}

		const int32 BaseVertex = OutCap.Vertices.Num();
		OutCap.Vertices.Reserve(BaseVertex + All2D.Num());
		for (const FVector2D& P : All2D)
		{
			OutCap.Vertices.Add(FVector(P.X, P.Y, 0.f));
		}

		OutCap.Triangles.Reserve(OutCap.Triangles.Num() + static_cast<int32>(Indices.size()));
		for (uint32_t Idx : Indices)
		{
			OutCap.Triangles.Add(BaseVertex + static_cast<int32>(Idx));
		}

		OutCap.Normals.AddDefaulted(All2D.Num());
		for (int32 I = BaseVertex; I < OutCap.Vertices.Num(); ++I)
		{
			OutCap.Normals[I] = FVector::UpVector;
		}

		OutCap.UV0.AddDefaulted(All2D.Num());

		for (int32 I = 0; I < OuterRingPoints.Num(); ++I)
		{
			const int32 Next = (I + 1) % OuterRingPoints.Num();
			OutCap.BoundaryEdges.Add(BaseVertex + I);
			OutCap.BoundaryEdges.Add(BaseVertex + Next);
		}

		int32 HoleVertexBase = BaseVertex + OuterRingPoints.Num();
		for (const TArray<FVector2D>& HoleRing : HoleRings)
		{
			for (int32 I = 0; I < HoleRing.Num(); ++I)
			{
				const int32 Next = (I + 1) % HoleRing.Num();
				OutCap.HoleBoundaryEdges.Add(HoleVertexBase + I);
				OutCap.HoleBoundaryEdges.Add(HoleVertexBase + Next);
			}
			HoleVertexBase += HoleRing.Num();
		}

		return true;
	}
}

bool FSvgTessellator::TessellateShape(const FSvgImportedShape& Shape, const FSvgMeshSettings& Settings, FSvgTessellatedCap& OutCap, FString& OutError)
{
	OutCap = FSvgTessellatedCap();
	OutError.Reset();

	if (!SvgTessellatorPrivate::TessellatePolygonWithHoles(Shape.Outer, Shape.Holes, Settings, OutCap, OutError))
	{
		return false;
	}

	for (const FSvgShapeOuterPart& AdditionalOuter : Shape.AdditionalOuters)
	{
		FString PartError;
		if (!SvgTessellatorPrivate::TessellatePolygonWithHoles(AdditionalOuter.Points, TArray<FSvgShapeHole>(), Settings, OutCap, PartError))
		{
			UE_LOG(LogSvgMeshImporter, Warning,
				TEXT("[SvgTessellator] Failed to tessellate additional outer for '%s': %s"),
				*Shape.ShapeName,
				*PartError);
		}
	}

	return !OutCap.Vertices.IsEmpty();
}

