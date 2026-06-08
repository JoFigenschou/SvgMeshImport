#include "SvgMeshGenerator.h"

#include "SvgParser/SvgParser.h"
#include "SvgTessellation/SvgTessellator.h"
#include "MeshOps/SvgFlatCapMesh.h"
#include "MeshOps/SvgUvGenerator.h"
#include "SvgMeshImporterLog.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/SvgProceduralMeshBuilder.h"
#include "ProceduralMeshComponent.h"

namespace SvgMeshGeneratorPrivate
{
	static void LogSvgPathCandidates(const FString& FilePath)
	{
		auto LogCandidate = [](const TCHAR* Label, const FString& Candidate)
		{
			const bool bExists = FPaths::FileExists(Candidate);
			UE_LOG(LogSvgMeshImporter, Log,
				TEXT("[SvgMeshGenerator] Path probe %s: '%s' exists=%s"),
				Label,
				*Candidate,
				bExists ? TEXT("yes") : TEXT("no"));
		};

		UE_LOG(LogSvgMeshImporter, Log,
			TEXT("[SvgMeshGenerator] Resolving SVG path input='%s' ProjectDir='%s'"),
			*FilePath,
			*FPaths::ProjectDir());

		LogCandidate(TEXT("as-is"), FilePath);
		LogCandidate(TEXT("absolute"), FPaths::ConvertRelativePathToFull(FilePath));
		LogCandidate(TEXT("project-relative"), FPaths::Combine(FPaths::ProjectDir(), FilePath));
		LogCandidate(TEXT("project-content"), FPaths::Combine(FPaths::ProjectContentDir(), FilePath));
		LogCandidate(TEXT("project-plugins"), FPaths::Combine(FPaths::ProjectPluginsDir(), FilePath));
	}

	static bool TryLoadSvgFile(const FString& FilePath, FString& OutContent, FString& OutResolvedPath)
	{
		const TArray<FString> Candidates = {
			FilePath,
			FPaths::ConvertRelativePathToFull(FilePath),
			FPaths::Combine(FPaths::ProjectDir(), FilePath),
			FPaths::Combine(FPaths::ProjectContentDir(), FilePath),
			FPaths::Combine(FPaths::ProjectPluginsDir(), FilePath)
		};

		TSet<FString> Tried;
		for (const FString& Candidate : Candidates)
		{
			const FString Normalized = FPaths::ConvertRelativePathToFull(Candidate);
			if (Normalized.IsEmpty() || Tried.Contains(Normalized))
			{
				continue;
			}
			Tried.Add(Normalized);

			if (FFileHelper::LoadFileToString(OutContent, *Normalized))
			{
				OutResolvedPath = Normalized;
				UE_LOG(LogSvgMeshImporter, Log,
					TEXT("[SvgMeshGenerator] Loaded SVG from '%s' (%d chars)"),
					*OutResolvedPath,
					OutContent.Len());
				return true;
			}
		}

		return false;
	}
	static FBox2D ComputeBounds(const TArray<FSvgImportedShape>& Shapes)
	{
		FBox2D Bounds(EForceInit::ForceInit);
		for (const FSvgImportedShape& Shape : Shapes)
		{
			for (const FVector2D& P : Shape.Outer)
			{
				Bounds += P;
			}
			for (const FSvgShapeHole& Hole : Shape.Holes)
			{
				for (const FVector2D& P : Hole.Points)
				{
					Bounds += P;
				}
			}
		}
		if (!Bounds.bIsValid)
		{
			Bounds = FBox2D(FVector2D(0.f), FVector2D(1.f));
		}
		return Bounds;
	}

	static void AppendMesh(FSvgMeshData& Acc, const FSvgMeshData& Part)
	{
		const int32 Base = Acc.Vertices.Num();
		Acc.Vertices.Append(Part.Vertices);
		Acc.Normals.Append(Part.Normals);
		Acc.UV0.Append(Part.UV0);
		Acc.Tangents.Append(Part.Tangents);
		for (int32 Tri : Part.Triangles)
		{
			Acc.Triangles.Add(Tri + Base);
		}
	}

	static FSvgMeshData BuildFlatCapMesh(const FSvgTessellatedCap& Cap)
	{
		FSvgMeshData Mesh;
		Mesh.Vertices = Cap.Vertices;
		Mesh.Triangles = Cap.Triangles;
		Mesh.UV0 = Cap.UV0;
		FSvgFlatCapMesh::FixWindingForComponentLocalPositiveZ(Mesh);
		FSvgFlatCapMesh::ApplyComponentLocalFlatBasis(Mesh);
		return Mesh;
	}
}

FSvgMeshBuildResult USvgMeshGenerator::BuildFromSvgFile(const FString& FilePath, const FSvgMeshSettings& Settings)
{
	SvgMeshGeneratorPrivate::LogSvgPathCandidates(FilePath);

	FString Content;
	FString ResolvedPath;
	if (!SvgMeshGeneratorPrivate::TryLoadSvgFile(FilePath, Content, ResolvedPath))
	{
		FSvgMeshBuildResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Could not read SVG file: %s"), *FilePath);
		UE_LOG(LogSvgMeshImporter, Error, TEXT("[SvgMeshGenerator] %s"), *Result.ErrorMessage);
		return Result;
	}
	return BuildFromSvgStringInternal(Content, Settings, ResolvedPath);
}

FSvgMeshBuildResult USvgMeshGenerator::BuildFromSvgString(const FString& SvgContent, const FSvgMeshSettings& Settings)
{
	return BuildFromSvgStringInternal(SvgContent, Settings, TEXT("SVG String"));
}

FSvgMeshBuildResult USvgMeshGenerator::BuildFromSvgStringInternal(const FString& SvgContent, const FSvgMeshSettings& Settings, const FString& SourceLabel)
{
	FSvgMeshBuildResult Result;

	TArray<FSvgImportedShape> Shapes;
	FString ParseError;
	if (!FSvgParser::Parse(SvgContent, Settings, Shapes, ParseError))
	{
		Result.bSuccess = false;
		Result.ErrorMessage = ParseError;
		UE_LOG(LogSvgMeshImporter, Error,
			TEXT("[SvgMeshGenerator] Parse failed for '%s': %s"),
			*SourceLabel,
			*ParseError);
		return Result;
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshGenerator] Parsed '%s' -> %d shape(s)"),
		*SourceLabel,
		Shapes.Num());

	const FBox2D Bounds = SvgMeshGeneratorPrivate::ComputeBounds(Shapes);
	FSvgMeshData Combined;
	TArray<FSvgShapeMesh> ShapeMeshes;
	ShapeMeshes.Reserve(Shapes.Num());

	for (int32 ShapeIndex = 0; ShapeIndex < Shapes.Num(); ++ShapeIndex)
	{
		FSvgImportedShape& Shape = Shapes[ShapeIndex];
		FSvgTessellatedCap Cap;
		FString TessError;
		if (!FSvgTessellator::TessellateShape(Shape, Settings, Cap, TessError))
		{
			Shape.Diagnostics.bSuccess = false;
			Shape.Diagnostics.Message = TessError;
			UE_LOG(LogSvgMeshImporter, Warning,
				TEXT("[SvgMeshGenerator] Tessellation failed for shape %d: %s"),
				ShapeIndex,
				*TessError);
			continue;
		}

		FSvgMeshData Part = SvgMeshGeneratorPrivate::BuildFlatCapMesh(Cap);
		FSvgUvGenerator::GenerateUVs(Part, Settings, Bounds);

		Shape.Diagnostics.bSuccess = true;
		Shape.Diagnostics.TriangleCount = Part.Triangles.Num() / 3;
		Shape.Diagnostics.Message = TEXT("Mesh built");

		FSvgShapeMesh ShapeMesh;
		ShapeMesh.ShapeIndex = ShapeIndex;
		ShapeMesh.MeshData = Part;
		ShapeMesh.Shape = Shape;
		ShapeMeshes.Add(MoveTemp(ShapeMesh));

		SvgMeshGeneratorPrivate::AppendMesh(Combined, Part);
	}

	if (Combined.Vertices.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("No mesh geometry produced from %s"), *SourceLabel);
		Result.Shapes = Shapes;
		UE_LOG(LogSvgMeshImporter, Error,
			TEXT("[SvgMeshGenerator] %s"),
			*Result.ErrorMessage);
		return Result;
	}

	Result.bSuccess = true;
	Result.ShapeMeshes = MoveTemp(ShapeMeshes);
	Result.MeshData = MoveTemp(Combined);
	Result.Shapes = Shapes;
	Result.Diagnostics.bSuccess = true;
	Result.Diagnostics.TriangleCount = Result.MeshData.Triangles.Num() / 3;
	Result.Diagnostics.Message = FString::Printf(TEXT("Built %d shape mesh(es) from %s"), Result.ShapeMeshes.Num(), *SourceLabel);

	FBox MeshBounds(ForceInit);
	for (const FVector& V : Result.MeshData.Vertices)
	{
		MeshBounds += V;
	}
	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgMeshGenerator] Built '%s' shapeMeshes=%d verts=%d tris=%d boundsMin=%s boundsMax=%s"),
		*SourceLabel,
		Result.ShapeMeshes.Num(),
		Result.MeshData.Vertices.Num(),
		Result.Diagnostics.TriangleCount,
		*MeshBounds.Min.ToString(),
		*MeshBounds.Max.ToString());
	return Result;
}

bool USvgMeshGenerator::BuildFromSvgFileToMesh(const FString& FilePath, const FSvgMeshSettings& Settings, UProceduralMeshComponent* TargetMesh, bool bCreateCollision)
{
	const FSvgMeshBuildResult Result = BuildFromSvgFile(FilePath, Settings);
	if (!Result.bSuccess || !TargetMesh)
	{
		return false;
	}
	if (!Result.ShapeMeshes.IsEmpty())
	{
		FSvgProceduralMeshBuilder::ApplyShapeMeshes(TargetMesh, Result.ShapeMeshes, bCreateCollision);
	}
	else
	{
		FSvgProceduralMeshBuilder::ClearMesh(TargetMesh);
		FSvgProceduralMeshBuilder::ApplyMeshData(TargetMesh, Result.MeshData, bCreateCollision);
	}
	return true;
}

bool USvgMeshGenerator::BuildFromSvgStringToMesh(const FString& SvgContent, const FSvgMeshSettings& Settings, UProceduralMeshComponent* TargetMesh, bool bCreateCollision)
{
	const FSvgMeshBuildResult Result = BuildFromSvgString(SvgContent, Settings);
	if (!Result.bSuccess || !TargetMesh)
	{
		return false;
	}
	if (!Result.ShapeMeshes.IsEmpty())
	{
		FSvgProceduralMeshBuilder::ApplyShapeMeshes(TargetMesh, Result.ShapeMeshes, bCreateCollision);
	}
	else
	{
		FSvgProceduralMeshBuilder::ClearMesh(TargetMesh);
		FSvgProceduralMeshBuilder::ApplyMeshData(TargetMesh, Result.MeshData, bCreateCollision);
	}
	return true;
}