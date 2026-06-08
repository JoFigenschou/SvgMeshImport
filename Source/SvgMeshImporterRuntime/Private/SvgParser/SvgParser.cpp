#include "SvgParser/SvgParser.h"

#include "SvgPolygon/SvgPolygonCleanup.h"
#include "SvgMeshImporterLog.h"
#include "nanosvg.h"

THIRD_PARTY_INCLUDES_START
#include "clipper2/clipper.h"
THIRD_PARTY_INCLUDES_END

#ifndef SVG_MESH_IMPORTER_CLIPPER_SCALE
#define SVG_MESH_IMPORTER_CLIPPER_SCALE 1000.0
#endif

namespace SvgParserPrivate
{
	static FVector2D ToVec2(float X, float Y, bool bFlipY, float SvgHeight)
	{
		const float YVal = bFlipY ? (SvgHeight - Y) : Y;
		return FVector2D(X, YVal);
	}

	static double PolygonArea(const TArray<FVector2D>& Poly)
	{
		if (Poly.Num() < 3)
		{
			return 0.0;
		}
		double A = 0.0;
		for (int32 I = 0; I < Poly.Num(); ++I)
		{
			const FVector2D& P0 = Poly[I];
			const FVector2D& P1 = Poly[(I + 1) % Poly.Num()];
			A += static_cast<double>(P0.X) * P1.Y - static_cast<double>(P1.X) * P0.Y;
		}
		return 0.5 * A;
	}

	static void FlattenCubic(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3,
		float Tolerance, TArray<FVector2D>& Out)
	{
		struct FFlatSeg
		{
			FVector2D A, B, C, D;
		};

		TArray<FFlatSeg> Stack;
		FFlatSeg Root;
		Root.A = P0; Root.B = P1; Root.C = P2; Root.D = P3;
		Stack.Add(Root);

		const float TolSq = FMath::Max(0.0001f, Tolerance * Tolerance);

		while (Stack.Num() > 0)
		{
			const FFlatSeg Seg = Stack.Pop();
			const FVector2D Mid = (Seg.A + Seg.D) * 0.5f;
			const FVector2D Est = (Seg.A + Seg.B + Seg.C + Seg.D) * 0.25f;
			if (FVector2D::DistSquared(Mid, Est) <= TolSq)
			{
				if (Out.Num() == 0 || FVector2D::Distance(Out.Last(), Seg.D) > KINDA_SMALL_NUMBER)
				{
					Out.Add(Seg.D);
				}
			}
			else
			{
				const FVector2D AB = (Seg.A + Seg.B) * 0.5f;
				const FVector2D BC = (Seg.B + Seg.C) * 0.5f;
				const FVector2D CD = (Seg.C + Seg.D) * 0.5f;
				const FVector2D ABC = (AB + BC) * 0.5f;
				const FVector2D BCD = (BC + CD) * 0.5f;
				const FVector2D ABCD = (ABC + BCD) * 0.5f;

				FFlatSeg L; L.A = Seg.A; L.B = AB; L.C = ABC; L.D = ABCD;
				FFlatSeg R; R.A = ABCD; R.B = BCD; R.C = CD; R.D = Seg.D;
				Stack.Add(R);
				Stack.Add(L);
			}
		}
	}

	static void RemoveDuplicatePoints(TArray<FVector2D>& Poly, float Epsilon = 0.01f)
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

	static Clipper2Lib::Path64 ToClipperPath(const TArray<FVector2D>& Poly, float Scale)
	{
		Clipper2Lib::Path64 Path;
		Path.reserve(static_cast<size_t>(Poly.Num()));
		for (const FVector2D& P : Poly)
		{
			Path.emplace_back(
				static_cast<int64>(FMath::RoundToDouble(static_cast<double>(P.X) * static_cast<double>(Scale))),
				static_cast<int64>(FMath::RoundToDouble(static_cast<double>(P.Y) * static_cast<double>(Scale))));
		}
		return Path;
	}

	static TArray<FVector2D> FromClipperPath(const Clipper2Lib::Path64& Path, float Scale)
	{
		TArray<FVector2D> Out;
		Out.Reserve(static_cast<int32>(Path.size()));
		const double Inv = 1.0 / static_cast<double>(Scale);
		for (const auto& Pt : Path)
		{
			Out.Add(FVector2D(static_cast<float>(static_cast<double>(Pt.x) * Inv), static_cast<float>(static_cast<double>(Pt.y) * Inv)));
		}
		return Out;
	}
}

bool FSvgParser::Parse(const FString& SvgContent, const FSvgMeshSettings& Settings, TArray<FSvgImportedShape>& OutShapes, FString& OutError)
{
	OutShapes.Reset();
	OutError.Reset();

	FTCHARToUTF8 Utf8(*SvgContent);
	NSVGimage* Image = nsvgParse(const_cast<char*>(Utf8.Get()), "px", 96.0f);
	if (!Image)
	{
		OutError = TEXT("Failed to parse SVG content.");
		return false;
	}

	const float SvgHeight = Image->height;
	const float Scale = FMath::Max(KINDA_SMALL_NUMBER, Settings.SvgScale);
	const float ClipperScale = static_cast<float>(SVG_MESH_IMPORTER_CLIPPER_SCALE);

	TArray<TArray<FVector2D>> RawLoops;

	for (NSVGshape* Shape = Image->shapes; Shape != nullptr; Shape = Shape->next)
	{
		if (!(Shape->flags & NSVG_FLAGS_VISIBLE))
		{
			continue;
		}

		for (NSVGpath* Path = Shape->paths; Path != nullptr; Path = Path->next)
		{
			if (Path->npts < 4)
			{
				continue;
			}

			TArray<FVector2D> Loop;
			FVector2D Start = SvgParserPrivate::ToVec2(Path->pts[0], Path->pts[1], Settings.bFlipY, SvgHeight);
			Loop.Add(Start * Scale);

			for (int32 I = 0; I < Path->npts - 1; I += 3)
			{
				const float* P = &Path->pts[I * 2];
				const FVector2D P0 = SvgParserPrivate::ToVec2(P[0], P[1], Settings.bFlipY, SvgHeight) * Scale;
				const FVector2D P1 = SvgParserPrivate::ToVec2(P[2], P[3], Settings.bFlipY, SvgHeight) * Scale;
				const FVector2D P2 = SvgParserPrivate::ToVec2(P[4], P[5], Settings.bFlipY, SvgHeight) * Scale;
				const FVector2D P3 = SvgParserPrivate::ToVec2(P[6], P[7], Settings.bFlipY, SvgHeight) * Scale;
				SvgParserPrivate::FlattenCubic(P0, P1, P2, P3, Settings.CurveTolerance, Loop);
			}

			SvgParserPrivate::RemoveDuplicatePoints(Loop);
			FSvgPolygonCleanup::CleanRing(Loop, Settings);
			if (Loop.Num() >= 3)
			{
				RawLoops.Add(MoveTemp(Loop));
			}
		}
	}

	nsvgDelete(Image);

	if (RawLoops.IsEmpty())
	{
		OutError = TEXT("No filled paths found in SVG.");
		return false;
	}

	Clipper2Lib::FillRule FillRule = (Settings.WindingRule == ESvgWindingRule::EvenOdd)
		? Clipper2Lib::FillRule::EvenOdd
		: Clipper2Lib::FillRule::NonZero;

	Clipper2Lib::Paths64 Subject;
	Subject.reserve(static_cast<size_t>(RawLoops.Num()));
	for (const TArray<FVector2D>& Loop : RawLoops)
	{
		Subject.push_back(SvgParserPrivate::ToClipperPath(Loop, ClipperScale));
	}

	Clipper2Lib::Paths64 Solution = Clipper2Lib::Union(Subject, FillRule);

	int32 DroppedSmallShapes = 0;
	for (const Clipper2Lib::Path64& OuterPath : Solution)
	{
		if (OuterPath.size() < 3)
		{
			continue;
		}

		const double AreaOuterClipper = Clipper2Lib::Area(OuterPath);
		const double MinAreaClipper = static_cast<double>(Settings.MinShapeArea) * static_cast<double>(ClipperScale) * static_cast<double>(ClipperScale);
		if (Settings.MinShapeArea > KINDA_SMALL_NUMBER && FMath::Abs(AreaOuterClipper) < MinAreaClipper)
		{
			++DroppedSmallShapes;
			continue;
		}

		FSvgImportedShape Imported;
		Imported.Outer = SvgParserPrivate::FromClipperPath(OuterPath, ClipperScale);
		FSvgPolygonCleanup::CleanRing(Imported.Outer, Settings);
		if (Imported.Outer.Num() < 3)
		{
			++DroppedSmallShapes;
			continue;
		}

		if (Settings.MinShapeArea > KINDA_SMALL_NUMBER
			&& FMath::Abs(FSvgPolygonCleanup::ComputeArea(Imported.Outer)) < Settings.MinShapeArea)
		{
			++DroppedSmallShapes;
			continue;
		}

		Clipper2Lib::Paths64 HoleSolution = Clipper2Lib::Intersect(Subject, Clipper2Lib::Paths64{ OuterPath }, FillRule);
		for (const Clipper2Lib::Path64& HolePath : HoleSolution)
		{
			if (HolePath.size() < 3)
			{
				continue;
			}
			const double AreaHole = Clipper2Lib::Area(HolePath);
			if (FMath::Abs(AreaHole) >= FMath::Abs(AreaOuterClipper) * 0.98)
			{
				continue;
			}
			TArray<FVector2D> Hole = SvgParserPrivate::FromClipperPath(HolePath, ClipperScale);
			FSvgPolygonCleanup::CleanRing(Hole, Settings);
			if (Hole.Num() >= 3)
			{
				FSvgShapeHole HoleContour;
				HoleContour.Points = MoveTemp(Hole);
				Imported.Holes.Add(MoveTemp(HoleContour));
			}
		}

		Imported.Diagnostics.bSuccess = true;
		Imported.Diagnostics.OuterPointCount = Imported.Outer.Num();
		Imported.Diagnostics.HoleCount = Imported.Holes.Num();
		Imported.Diagnostics.Message = TEXT("Parsed shape");

		OutShapes.Add(MoveTemp(Imported));
	}

	if (DroppedSmallShapes > 0)
	{
		UE_LOG(LogSvgMeshImporter, Log,
			TEXT("[SvgParser] Dropped %d small or degenerate shape(s). Increase Min Shape Area or Simplify Tolerance if needed."),
			DroppedSmallShapes);
	}

	if (OutShapes.IsEmpty())
	{
		OutError = TEXT("Union produced no usable polygons.");
		return false;
	}

	return true;
}



