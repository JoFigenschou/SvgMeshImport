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
	static FVector2D ToVec2(float X, float Y, bool bFlipX, bool bFlipY, float SvgWidth, float SvgHeight)
	{
		const float XVal = bFlipX ? (SvgWidth - X) : X;
		const float YVal = bFlipY ? (SvgHeight - Y) : Y;
		return FVector2D(XVal, YVal);
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

	static FVector2D ComputeLoopCentroid(const TArray<FVector2D>& Loop)
	{
		if (Loop.IsEmpty())
		{
			return FVector2D::ZeroVector;
		}

		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& Point : Loop)
		{
			Sum += Point;
		}

		return Sum / static_cast<float>(Loop.Num());
	}

	static bool IsLoopInsideOuter(
		const TArray<FVector2D>& Inner,
		const TArray<FVector2D>& Outer,
		float ClipperScale)
	{
		if (Inner.Num() < 3 || Outer.Num() < 3)
		{
			return false;
		}

		const FVector2D Centroid = ComputeLoopCentroid(Inner);
		const Clipper2Lib::Path64 OuterPath = ToClipperPath(Outer, ClipperScale);
		const Clipper2Lib::Point64 TestPoint(
			static_cast<int64>(FMath::RoundToDouble(static_cast<double>(Centroid.X) * static_cast<double>(ClipperScale))),
			static_cast<int64>(FMath::RoundToDouble(static_cast<double>(Centroid.Y) * static_cast<double>(ClipperScale))));

		return Clipper2Lib::PointInPolygon(TestPoint, OuterPath) == Clipper2Lib::PointInPolygonResult::IsInside;
	}

	static FBox2D ComputeLoopBounds(const TArray<TArray<FVector2D>>& RawLoops)
	{
		FBox2D Bounds(EForceInit::ForceInit);
		for (const TArray<FVector2D>& Loop : RawLoops)
		{
			for (const FVector2D& Point : Loop)
			{
				Bounds += Point;
			}
		}
		return Bounds;
	}

	static bool ExtractPathLoop(
		const NSVGpath* Path,
		bool bFlipX,
		bool bFlipY,
		float SvgWidth,
		float SvgHeight,
		float Scale,
		const FSvgMeshSettings& Settings,
		TArray<FVector2D>& OutLoop)
	{
		OutLoop.Reset();
		if (!Path || Path->npts < 4)
		{
			return false;
		}

		const FVector2D Start = ToVec2(Path->pts[0], Path->pts[1], bFlipX, bFlipY, SvgWidth, SvgHeight) * Scale;
		OutLoop.Add(Start);

		for (int32 I = 0; I < Path->npts - 1; I += 3)
		{
			const float* P = &Path->pts[I * 2];
			const FVector2D P0 = ToVec2(P[0], P[1], bFlipX, bFlipY, SvgWidth, SvgHeight) * Scale;
			const FVector2D P1 = ToVec2(P[2], P[3], bFlipX, bFlipY, SvgWidth, SvgHeight) * Scale;
			const FVector2D P2 = ToVec2(P[4], P[5], bFlipX, bFlipY, SvgWidth, SvgHeight) * Scale;
			const FVector2D P3 = ToVec2(P[6], P[7], bFlipX, bFlipY, SvgWidth, SvgHeight) * Scale;
			FlattenCubic(P0, P1, P2, P3, Settings.CurveTolerance, OutLoop);
		}

		RemoveDuplicatePoints(OutLoop);
		FSvgPolygonCleanup::CleanRing(OutLoop, Settings);
		return OutLoop.Num() >= 3;
	}

	static FString ShapeIdToName(const NSVGshape* Shape, int32 FallbackIndex)
	{
		if (Shape && Shape->id[0] != '\0')
		{
			return FString(UTF8_TO_TCHAR(Shape->id));
		}

		return FString::Printf(TEXT("Shape_%d"), FallbackIndex);
	}

	struct FNamedLoop
	{
		FString GroupId;
		TArray<FVector2D> Loop;
	};

	static void AddMergedShape(
		const FString& ShapeName,
		TArray<TArray<FVector2D>> Parts,
		const FSvgMeshSettings& Settings,
		TArray<FSvgImportedShape>& OutShapes)
	{
		Parts.RemoveAll([](const TArray<FVector2D>& Part) { return Part.Num() < 3; });
		if (Parts.IsEmpty())
		{
			return;
		}

		Parts.Sort([](const TArray<FVector2D>& A, const TArray<FVector2D>& B)
		{
			return FMath::Abs(FSvgPolygonCleanup::ComputeArea(A)) > FMath::Abs(FSvgPolygonCleanup::ComputeArea(B));
		});

		FSvgImportedShape Imported;
		Imported.ShapeName = ShapeName;
		Imported.Outer = Parts[0];
		FSvgPolygonCleanup::CleanRing(Imported.Outer, Settings);
		if (Imported.Outer.Num() < 3)
		{
			return;
		}

		for (int32 PartIndex = 1; PartIndex < Parts.Num(); ++PartIndex)
		{
			TArray<FVector2D> ExtraPart = Parts[PartIndex];
			FSvgPolygonCleanup::CleanRing(ExtraPart, Settings);
			if (ExtraPart.Num() < 3)
			{
				continue;
			}

			if (Settings.bCutHoles
				&& IsLoopInsideOuter(
					ExtraPart,
					Imported.Outer,
					static_cast<float>(SVG_MESH_IMPORTER_CLIPPER_SCALE)))
			{
				FSvgShapeHole HoleContour;
				HoleContour.Points = MoveTemp(ExtraPart);
				Imported.Holes.Add(MoveTemp(HoleContour));
			}
			else
			{
				FSvgShapeOuterPart OuterPart;
				OuterPart.Points = MoveTemp(ExtraPart);
				Imported.AdditionalOuters.Add(MoveTemp(OuterPart));
			}
		}

		Imported.Diagnostics.bSuccess = true;
		Imported.Diagnostics.OuterPointCount = Imported.Outer.Num();
		Imported.Diagnostics.HoleCount = Imported.Holes.Num();
		Imported.Diagnostics.Message = Parts.Num() > 1
			? FString::Printf(
				TEXT("Merged %d part(s) by SVG id (%d hole(s), %d extra outer(s))"),
				Parts.Num(),
				Imported.Holes.Num(),
				Imported.AdditionalOuters.Num())
			: TEXT("Parsed shape by SVG id");
		OutShapes.Add(MoveTemp(Imported));
	}

	static bool BuildShapesGroupedById(
		const TArray<FNamedLoop>& NamedLoops,
		const FSvgMeshSettings& Settings,
		float CanvasArea,
		TArray<FSvgImportedShape>& OutShapes)
	{
		TArray<TArray<FVector2D>> RawLoops;
		RawLoops.Reserve(NamedLoops.Num());
		for (const FNamedLoop& NamedLoop : NamedLoops)
		{
			RawLoops.Add(NamedLoop.Loop);
		}

		const FBox2D LoopBounds = ComputeLoopBounds(RawLoops);
		const double BoundsArea = FMath::Max(
			static_cast<double>(LoopBounds.GetArea()),
			static_cast<double>(CanvasArea));

		int32 DroppedSmallLoops = 0;
		int32 DroppedBackgroundLoops = 0;

		Clipper2Lib::FillRule FillRule = (Settings.WindingRule == ESvgWindingRule::EvenOdd)
			? Clipper2Lib::FillRule::EvenOdd
			: Clipper2Lib::FillRule::NonZero;

		const float ClipperScale = static_cast<float>(SVG_MESH_IMPORTER_CLIPPER_SCALE);

		TMap<FString, TArray<const FNamedLoop*>> LoopsById;
		for (const FNamedLoop& NamedLoop : NamedLoops)
		{
			LoopsById.FindOrAdd(NamedLoop.GroupId).Add(&NamedLoop);
		}

		TArray<FString> GroupIds;
		LoopsById.GetKeys(GroupIds);
		GroupIds.Sort();

		for (const FString& GroupId : GroupIds)
		{
			const TArray<const FNamedLoop*>& GroupLoops = LoopsById[GroupId];
			Clipper2Lib::Paths64 Subject;
			Subject.reserve(static_cast<size_t>(GroupLoops.Num()));

			for (const FNamedLoop* NamedLoop : GroupLoops)
			{
				const TArray<FVector2D>& RawLoop = NamedLoop->Loop;
				if (RawLoop.Num() < 3)
				{
					continue;
				}

				const double LoopArea = FMath::Abs(FSvgPolygonCleanup::ComputeArea(RawLoop));
				if (Settings.MinShapeArea > KINDA_SMALL_NUMBER && LoopArea < Settings.MinShapeArea)
				{
					++DroppedSmallLoops;
					continue;
				}

				if (BoundsArea > KINDA_SMALL_NUMBER && LoopArea > BoundsArea * 0.85)
				{
					++DroppedBackgroundLoops;
					continue;
				}

				Subject.push_back(SvgParserPrivate::ToClipperPath(RawLoop, ClipperScale));
			}

			if (Subject.empty())
			{
				continue;
			}

			const Clipper2Lib::Paths64 Solution = Clipper2Lib::Union(Subject, FillRule);
			TArray<TArray<FVector2D>> Parts;
			Parts.Reserve(static_cast<int32>(Solution.size()));
			for (const Clipper2Lib::Path64& SolutionPath : Solution)
			{
				if (SolutionPath.size() < 3)
				{
					continue;
				}

				Parts.Add(SvgParserPrivate::FromClipperPath(SolutionPath, ClipperScale));
			}

			AddMergedShape(GroupId, MoveTemp(Parts), Settings, OutShapes);
		}

		if (DroppedSmallLoops > 0 || DroppedBackgroundLoops > 0)
		{
			UE_LOG(LogSvgMeshImporter, Log,
				TEXT("[SvgParser] Grouped by id: dropped %d small loop(s), %d background loop(s)."),
				DroppedSmallLoops,
				DroppedBackgroundLoops);
		}

		return !OutShapes.IsEmpty();
	}

	static void AbsorbContainedShapesAsHoles(
		TArray<FSvgImportedShape>& Shapes,
		const FSvgMeshSettings& Settings)
	{
		if (!Settings.bCutHoles || Shapes.Num() < 2)
		{
			return;
		}

		const float ClipperScale = static_cast<float>(SVG_MESH_IMPORTER_CLIPPER_SCALE);
		TArray<bool> bConsumed;
		bConsumed.Init(false, Shapes.Num());

		TArray<int32> Order;
		Order.Reserve(Shapes.Num());
		for (int32 I = 0; I < Shapes.Num(); ++I)
		{
			Order.Add(I);
		}

		Order.Sort([&Shapes](int32 A, int32 B)
		{
			return FMath::Abs(FSvgPolygonCleanup::ComputeArea(Shapes[A].Outer))
				> FMath::Abs(FSvgPolygonCleanup::ComputeArea(Shapes[B].Outer));
		});

		for (int32 OrderIndex = Order.Num() - 1; OrderIndex >= 0; --OrderIndex)
		{
			const int32 Candidate = Order[OrderIndex];
			if (bConsumed[Candidate] || Shapes[Candidate].Outer.Num() < 3)
			{
				continue;
			}

			const double CandidateArea = FMath::Abs(FSvgPolygonCleanup::ComputeArea(Shapes[Candidate].Outer));
			for (int32 HostOrderIndex = 0; HostOrderIndex < Order.Num(); ++HostOrderIndex)
			{
				const int32 Host = Order[HostOrderIndex];
				if (Host == Candidate || bConsumed[Host] || Shapes[Host].Outer.Num() < 3)
				{
					continue;
				}

				const double HostArea = FMath::Abs(FSvgPolygonCleanup::ComputeArea(Shapes[Host].Outer));
				if (CandidateArea >= HostArea * 0.98)
				{
					continue;
				}

				if (!IsLoopInsideOuter(Shapes[Candidate].Outer, Shapes[Host].Outer, ClipperScale))
				{
					continue;
				}

				FSvgShapeHole HoleContour;
				HoleContour.Points = Shapes[Candidate].Outer;
				Shapes[Host].Holes.Add(MoveTemp(HoleContour));
				Shapes[Host].Diagnostics.HoleCount = Shapes[Host].Holes.Num();
				bConsumed[Candidate] = true;
				break;
			}
		}

		TArray<FSvgImportedShape> Remaining;
		Remaining.Reserve(Shapes.Num());
		for (int32 I = 0; I < Shapes.Num(); ++I)
		{
			if (!bConsumed[I])
			{
				Remaining.Add(MoveTemp(Shapes[I]));
			}
		}

		const int32 AbsorbedCount = Shapes.Num() - Remaining.Num();
		if (AbsorbedCount > 0)
		{
			UE_LOG(LogSvgMeshImporter, Log,
				TEXT("[SvgParser] Absorbed %d contained shape(s) as holes."),
				AbsorbedCount);
		}

		Shapes = MoveTemp(Remaining);
	}
}

bool FSvgParser::Parse(const FString& SvgContent, FSvgMeshSettings& Settings, TArray<FSvgImportedShape>& OutShapes, FString& OutError)
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
	const float SvgWidth = Image->width;
	const float Scale = FMath::Max(KINDA_SMALL_NUMBER, Settings.SvgScale);
	const float CanvasArea = FMath::Max(1.f, SvgWidth * Scale * SvgHeight * Scale);
	const float ClipperScale = static_cast<float>(SVG_MESH_IMPORTER_CLIPPER_SCALE);

	TArray<SvgParserPrivate::FNamedLoop> NamedLoops;
	TArray<TArray<FVector2D>> RawLoops;
	int32 FilledElementCount = 0;
	int32 ShapeElementIndex = 0;

	for (NSVGshape* Shape = Image->shapes; Shape != nullptr; Shape = Shape->next, ++ShapeElementIndex)
	{
		if (!(Shape->flags & NSVG_FLAGS_VISIBLE))
		{
			continue;
		}

		if (Shape->fill.type == NSVG_PAINT_NONE)
		{
			continue;
		}

		++FilledElementCount;

		const FString GroupId = SvgParserPrivate::ShapeIdToName(Shape, ShapeElementIndex);
		for (NSVGpath* Path = Shape->paths; Path != nullptr; Path = Path->next)
		{
			TArray<FVector2D> Loop;
			if (!SvgParserPrivate::ExtractPathLoop(Path, Settings.bFlipX, Settings.bFlipY, SvgWidth, SvgHeight, Scale, Settings, Loop))
			{
				continue;
			}

			SvgParserPrivate::FNamedLoop NamedLoop;
			NamedLoop.GroupId = GroupId;
			NamedLoop.Loop = MoveTemp(Loop);
			NamedLoops.Add(MoveTemp(NamedLoop));
			RawLoops.Add(NamedLoops.Last().Loop);
		}
	}

	nsvgDelete(Image);

	if (RawLoops.IsEmpty())
	{
		OutError = TEXT("No filled paths found in SVG.");
		return false;
	}

	const int32 AutoThreshold = FMath::Max(2, Settings.AutoSeparatePathThreshold);
	if (FilledElementCount >= AutoThreshold && Settings.bUnionShapes)
	{
		Settings.bUnionShapes = false;
		UE_LOG(LogSvgMeshImporter, Warning,
			TEXT("[SvgParser] Auto-disabled Union Shapes for multi-path SVG (%d filled element(s), %d loop(s))."),
			FilledElementCount,
			RawLoops.Num());
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgParser] Found %d filled element(s) and %d path loop(s). UnionShapes=%s CutHoles=%s"),
		FilledElementCount,
		RawLoops.Num(),
		Settings.bUnionShapes ? TEXT("true") : TEXT("false"),
		Settings.bCutHoles ? TEXT("true") : TEXT("false"));

	if (!Settings.bUnionShapes)
	{
		if (!SvgParserPrivate::BuildShapesGroupedById(NamedLoops, Settings, CanvasArea, OutShapes))
		{
			OutError = TEXT("No usable SVG shapes found.");
			return false;
		}

		SvgParserPrivate::AbsorbContainedShapesAsHoles(OutShapes, Settings);

		UE_LOG(LogSvgMeshImporter, Log,
			TEXT("[SvgParser] Parsed %d shape mesh(es) grouped by SVG id from %d loop(s)."),
			OutShapes.Num(),
			RawLoops.Num());
		return true;
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
		Imported.ShapeName = FString::Printf(TEXT("Union_%d"), OutShapes.Num());
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

		if (Settings.bCutHoles)
		{
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

	SvgParserPrivate::AbsorbContainedShapesAsHoles(OutShapes, Settings);

	return true;
}



