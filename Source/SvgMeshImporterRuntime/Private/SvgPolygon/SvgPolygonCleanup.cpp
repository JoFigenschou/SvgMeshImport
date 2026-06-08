#include "SvgPolygon/SvgPolygonCleanup.h"

THIRD_PARTY_INCLUDES_START
#include "clipper2/clipper.h"
THIRD_PARTY_INCLUDES_END

#ifndef SVG_MESH_IMPORTER_CLIPPER_SCALE
#define SVG_MESH_IMPORTER_CLIPPER_SCALE 1000.0
#endif

namespace SvgPolygonCleanupPrivate
{
	static double PolygonArea(const TArray<FVector2D>& Poly)
	{
		if (Poly.Num() < 3)
		{
			return 0.0;
		}

		double Area = 0.0;
		for (int32 I = 0; I < Poly.Num(); ++I)
		{
			const FVector2D& P0 = Poly[I];
			const FVector2D& P1 = Poly[(I + 1) % Poly.Num()];
			Area += static_cast<double>(P0.X) * P1.Y - static_cast<double>(P1.X) * P0.Y;
		}
		return 0.5 * Area;
	}

	static void RemoveDuplicatePoints(TArray<FVector2D>& Poly, float Epsilon)
	{
		if (Poly.Num() < 2)
		{
			return;
		}

		TArray<FVector2D> Clean;
		Clean.Reserve(Poly.Num());
		for (const FVector2D& P : Poly)
		{
			if (Clean.Num() == 0 || FVector2D::Distance(Clean.Last(), P) > Epsilon)
			{
				Clean.Add(P);
			}
		}

		if (Clean.Num() > 1 && FVector2D::Distance(Clean[0], Clean.Last()) <= Epsilon)
		{
			Clean.Pop();
		}

		Poly = MoveTemp(Clean);
	}

	static void RemoveCollinearPoints(TArray<FVector2D>& Poly)
	{
		if (Poly.Num() < 3)
		{
			return;
		}

		TArray<FVector2D> Clean;
		Clean.Reserve(Poly.Num());
		const int32 Count = Poly.Num();
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector2D& Prev = Poly[(I + Count - 1) % Count];
			const FVector2D& Curr = Poly[I];
			const FVector2D& Next = Poly[(I + 1) % Count];
			const FVector2D AB = Curr - Prev;
			const FVector2D BC = Next - Curr;
			const float Cross = FMath::Abs(AB.X * BC.Y - AB.Y * BC.X);
			const float Scale = FMath::Max(AB.Size() * BC.Size(), KINDA_SMALL_NUMBER);
			if ((Cross / Scale) > 1e-4f)
			{
				Clean.Add(Curr);
			}
		}

		if (Clean.Num() >= 3)
		{
			Poly = MoveTemp(Clean);
		}
	}

	static void RemoveMicroEdges(TArray<FVector2D>& Poly, float MinEdgeLength)
	{
		if (Poly.Num() < 3 || MinEdgeLength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float MinEdgeLengthSq = MinEdgeLength * MinEdgeLength;
		bool bChanged = true;
		while (bChanged && Poly.Num() >= 3)
		{
			bChanged = false;
			for (int32 I = 0; I < Poly.Num(); ++I)
			{
				const int32 Next = (I + 1) % Poly.Num();
				if (FVector2D::DistSquared(Poly[I], Poly[Next]) >= MinEdgeLengthSq)
				{
					continue;
				}

				const int32 RemoveIndex = (FVector2D::DistSquared(Poly[I], Poly[(I + Poly.Num() - 1) % Poly.Num()])
					<= FVector2D::DistSquared(Poly[Next], Poly[(Next + 1) % Poly.Num()])) ? I : Next;
				Poly.RemoveAt(RemoveIndex);
				bChanged = true;
				break;
			}
		}
	}

	static Clipper2Lib::Path64 ToClipperPath(const TArray<FVector2D>& Poly, float ClipperScale)
	{
		Clipper2Lib::Path64 Path;
		Path.reserve(static_cast<size_t>(Poly.Num()));
		for (const FVector2D& P : Poly)
		{
			Path.emplace_back(
				static_cast<int64>(FMath::RoundToDouble(static_cast<double>(P.X) * static_cast<double>(ClipperScale))),
				static_cast<int64>(FMath::RoundToDouble(static_cast<double>(P.Y) * static_cast<double>(ClipperScale))));
		}
		return Path;
	}

	static TArray<FVector2D> FromClipperPath(const Clipper2Lib::Path64& Path, float ClipperScale)
	{
		TArray<FVector2D> Out;
		Out.Reserve(static_cast<int32>(Path.size()));
		const double Inv = 1.0 / static_cast<double>(ClipperScale);
		for (const auto& Pt : Path)
		{
			Out.Add(FVector2D(
				static_cast<float>(static_cast<double>(Pt.x) * Inv),
				static_cast<float>(static_cast<double>(Pt.y) * Inv)));
		}
		return Out;
	}

	static void SimplifyRing(TArray<FVector2D>& Poly, const FSvgMeshSettings& Settings)
	{
		if (Poly.Num() < 3 || Settings.SimplifyTolerance <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float ClipperScale = static_cast<float>(SVG_MESH_IMPORTER_CLIPPER_SCALE);
		const double Epsilon = FMath::Max(1.0, static_cast<double>(Settings.SimplifyTolerance) * static_cast<double>(ClipperScale));
		Clipper2Lib::Paths64 Paths{ ToClipperPath(Poly, ClipperScale) };
		Paths = Clipper2Lib::SimplifyPaths(Paths, Epsilon, true);
		if (!Paths.empty() && Paths[0].size() >= 3)
		{
			Poly = FromClipperPath(Paths[0], ClipperScale);
		}
	}
}

void FSvgPolygonCleanup::CleanRing(TArray<FVector2D>& Poly, const FSvgMeshSettings& Settings)
{
	const float DuplicateEpsilon = FMath::Max(0.01f, Settings.MinEdgeLength * 0.25f);
	SvgPolygonCleanupPrivate::RemoveDuplicatePoints(Poly, DuplicateEpsilon);
	SvgPolygonCleanupPrivate::RemoveCollinearPoints(Poly);
	SvgPolygonCleanupPrivate::RemoveMicroEdges(Poly, Settings.MinEdgeLength);
	SvgPolygonCleanupPrivate::SimplifyRing(Poly, Settings);
	SvgPolygonCleanupPrivate::RemoveDuplicatePoints(Poly, DuplicateEpsilon);
}

double FSvgPolygonCleanup::ComputeArea(const TArray<FVector2D>& Poly)
{
	return SvgPolygonCleanupPrivate::PolygonArea(Poly);
}
