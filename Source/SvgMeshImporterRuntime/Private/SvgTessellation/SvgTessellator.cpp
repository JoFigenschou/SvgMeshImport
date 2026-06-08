#include "SvgTessellation/SvgTessellator.h"

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

bool FSvgTessellator::TessellateShape(const FSvgImportedShape& Shape, const FSvgMeshSettings& Settings, FSvgTessellatedCap& OutCap, FString& OutError)
{
	OutCap = FSvgTessellatedCap();
	OutError.Reset();

	if (Shape.Outer.Num() < 3)
	{
		OutError = TEXT("Outer contour has fewer than 3 points.");
		return false;
	}

	using Point = std::array<double, 2>;
	using Polygon = std::vector<std::vector<Point>>;

	Polygon PolygonData;
	PolygonData.reserve(static_cast<size_t>(1 + Shape.Holes.Num()));

	TArray<FVector2D> Outer = Shape.Outer;
	FSvgPolygonCleanup::CleanRing(Outer, Settings);
	Outer = SvgTessellatorPrivate::EnsureWinding(Outer, false);
	std::vector<Point> OuterRing;
	OuterRing.reserve(static_cast<size_t>(Outer.Num()));
	for (const FVector2D& P : Outer)
	{
		OuterRing.push_back({ static_cast<double>(P.X), static_cast<double>(P.Y) });
	}
	PolygonData.push_back(std::move(OuterRing));

	for (const FSvgShapeHole& Hole : Shape.Holes)
	{
		if (Hole.Points.Num() < 3)
		{
			continue;
		}
		TArray<FVector2D> HoleWound = Hole.Points;
		FSvgPolygonCleanup::CleanRing(HoleWound, Settings);
		HoleWound = SvgTessellatorPrivate::EnsureWinding(HoleWound, true);
		std::vector<Point> HoleRing;
		HoleRing.reserve(static_cast<size_t>(HoleWound.Num()));
		for (const FVector2D& P : HoleWound)
		{
			HoleRing.push_back({ static_cast<double>(P.X), static_cast<double>(P.Y) });
		}
		PolygonData.push_back(std::move(HoleRing));
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
	All2D.Reserve(Outer.Num());
	for (const FVector2D& P : Outer)
	{
		All2D.Add(P);
	}
	TArray<int32> HoleOffsets;
	for (const FSvgShapeHole& Hole : Shape.Holes)
	{
		if (Hole.Points.Num() < 3)
		{
			continue;
		}
		HoleOffsets.Add(All2D.Num());
		TArray<FVector2D> HoleWound = Hole.Points;
		FSvgPolygonCleanup::CleanRing(HoleWound, Settings);
		HoleWound = SvgTessellatorPrivate::EnsureWinding(HoleWound, true);
		for (const FVector2D& P : HoleWound)
		{
			All2D.Add(P);
		}
	}

	OutCap.Vertices.Reserve(All2D.Num());
	for (const FVector2D& P : All2D)
	{
		OutCap.Vertices.Add(FVector(P.X, P.Y, 0.f));
	}

	OutCap.Triangles.Reserve(static_cast<int32>(Indices.size()));
	for (uint32_t Idx : Indices)
	{
		OutCap.Triangles.Add(static_cast<int32>(Idx));
	}

	OutCap.Normals.Init(FVector::UpVector, OutCap.Vertices.Num());
	OutCap.UV0.Init(FVector2D::ZeroVector, OutCap.Vertices.Num());

	for (int32 I = 0; I < Outer.Num(); ++I)
	{
		const int32 Next = (I + 1) % Outer.Num();
		OutCap.BoundaryEdges.Add(I);
		OutCap.BoundaryEdges.Add(Next);
	}

	return true;
}

