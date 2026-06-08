#include "SvgMeshGenerator.h"

#include "SvgParser/SvgParser.h"
#include "SvgTessellation/SvgTessellator.h"
#include "MeshOps/SvgExtruder.h"
#include "MeshOps/SvgChamfer.h"
#include "MeshOps/SvgUvGenerator.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Rendering/SvgProceduralMeshBuilder.h"
#include "ProceduralMeshComponent.h"

namespace SvgMeshGeneratorPrivate
{
	static FBox2D ComputeBounds(const TArray<FSvgImportedShape>& Shapes)
	{
		FBox2D Bounds(EForceInit::ForceInit);
		for (const FSvgImportedShape& Shape : Shapes)
		{
			for (const FVector2D& P : Shape.Outer)
			{
				Bounds += P;
			}
			for (const TArray<FVector2D>& Hole : Shape.Holes)
			{
				for (const FVector2D& P : Hole)
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
}

FSvgMeshBuildResult USvgMeshGenerator::BuildFromSvgFile(const FString& FilePath, const FSvgMeshSettings& Settings)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *FilePath))
	{
		FSvgMeshBuildResult Result;
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("Could not read SVG file: %s"), *FilePath);
		return Result;
	}
	return BuildFromSvgStringInternal(Content, Settings, FilePath);
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
		return Result;
	}

	const FBox2D Bounds = SvgMeshGeneratorPrivate::ComputeBounds(Shapes);
	FSvgMeshData Combined;

	for (FSvgImportedShape& Shape : Shapes)
	{
		FSvgTessellatedCap Cap;
		FString TessError;
		if (!FSvgTessellator::TessellateShape(Shape, Settings.bFlipY, Settings.SvgScale, Cap, TessError))
		{
			Shape.Diagnostics.bSuccess = false;
			Shape.Diagnostics.Message = TessError;
			continue;
		}

		FSvgMeshData Part;
		FSvgExtruder::Extrude(Cap, Settings.ExtrudeDepth, Part);
		FSvgChamfer::ApplyChamfer(Part, Settings);
		FSvgUvGenerator::GenerateUVs(Part, Settings, Bounds);

		Shape.Diagnostics.bSuccess = true;
		Shape.Diagnostics.TriangleCount = Part.Triangles.Num() / 3;
		Shape.Diagnostics.Message = TEXT("Mesh built");

		SvgMeshGeneratorPrivate::AppendMesh(Combined, Part);
	}

	if (Combined.Vertices.IsEmpty())
	{
		Result.bSuccess = false;
		Result.ErrorMessage = FString::Printf(TEXT("No mesh geometry produced from %s"), *SourceLabel);
		Result.Shapes = Shapes;
		return Result;
	}

	Result.bSuccess = true;
	Result.MeshData = MoveTemp(Combined);
	Result.Shapes = Shapes;
	Result.Diagnostics.bSuccess = true;
	Result.Diagnostics.TriangleCount = Result.MeshData.Triangles.Num() / 3;
	Result.Diagnostics.Message = FString::Printf(TEXT("Built from %s"), *SourceLabel);
	return Result;
}

bool USvgMeshGenerator::BuildFromSvgFileToMesh(const FString& FilePath, const FSvgMeshSettings& Settings, UProceduralMeshComponent* TargetMesh, bool bCreateCollision)
{
	const FSvgMeshBuildResult Result = BuildFromSvgFile(FilePath, Settings);
	if (!Result.bSuccess || !TargetMesh)
	{
		return false;
	}
	FSvgProceduralMeshBuilder::ClearMesh(TargetMesh);
	FSvgProceduralMeshBuilder::ApplyMeshData(TargetMesh, Result.MeshData, bCreateCollision);
	return true;
}

bool USvgMeshGenerator::BuildFromSvgStringToMesh(const FString& SvgContent, const FSvgMeshSettings& Settings, UProceduralMeshComponent* TargetMesh, bool bCreateCollision)
{
	const FSvgMeshBuildResult Result = BuildFromSvgString(SvgContent, Settings);
	if (!Result.bSuccess || !TargetMesh)
	{
		return false;
	}
	FSvgProceduralMeshBuilder::ClearMesh(TargetMesh);
	FSvgProceduralMeshBuilder::ApplyMeshData(TargetMesh, Result.MeshData, bCreateCollision);
	return true;
}