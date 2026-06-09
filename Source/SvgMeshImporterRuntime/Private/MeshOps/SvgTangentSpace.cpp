#include "MeshOps/SvgTangentSpace.h"

#include "SvgMeshImporterLog.h"

namespace SvgTangentSpacePrivate
{
	static bool IsCapNormal(const FVector& Normal)
	{
		return FMath::Abs(Normal.Z) > 0.9f;
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

	static void RecomputeSmoothNormalsFromTriangles(FSvgMeshData& Mesh)
	{
		TArray<FVector> Accumulated;
		TArray<int32> Counts;
		Accumulated.Init(FVector::ZeroVector, Mesh.Vertices.Num());
		Counts.Init(0, Mesh.Vertices.Num());

		for (int32 T = 0; T < Mesh.Triangles.Num(); T += 3)
		{
			const int32 I0 = Mesh.Triangles[T];
			const int32 I1 = Mesh.Triangles[T + 1];
			const int32 I2 = Mesh.Triangles[T + 2];
			if (!Mesh.Vertices.IsValidIndex(I0)
				|| !Mesh.Vertices.IsValidIndex(I1)
				|| !Mesh.Vertices.IsValidIndex(I2))
			{
				continue;
			}

			const FVector FaceNormal = ComputeFaceNormal(Mesh.Vertices, I0, I1, I2);
			Accumulated[I0] += FaceNormal;
			Accumulated[I1] += FaceNormal;
			Accumulated[I2] += FaceNormal;
			++Counts[I0];
			++Counts[I1];
			++Counts[I2];
		}

		Mesh.Normals.SetNum(Mesh.Vertices.Num());
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			if (Counts[I] > 0)
			{
				Mesh.Normals[I] = Accumulated[I].GetSafeNormal();
			}
			else if (Mesh.Normals.IsValidIndex(I))
			{
				Mesh.Normals[I] = Mesh.Normals[I].GetSafeNormal();
			}
			else
			{
				Mesh.Normals[I] = FVector::UpVector;
			}
		}
	}

	static void RecomputeSmoothSideNormals(FSvgMeshData& Mesh, const TArray<bool>& bCapVertices)
	{
		TArray<FVector> Accumulated;
		TArray<int32> Counts;
		Accumulated.Init(FVector::ZeroVector, Mesh.Vertices.Num());
		Counts.Init(0, Mesh.Vertices.Num());

		for (int32 T = 0; T < Mesh.Triangles.Num(); T += 3)
		{
			const int32 I0 = Mesh.Triangles[T];
			const int32 I1 = Mesh.Triangles[T + 1];
			const int32 I2 = Mesh.Triangles[T + 2];
			if (!Mesh.Vertices.IsValidIndex(I0)
				|| !Mesh.Vertices.IsValidIndex(I1)
				|| !Mesh.Vertices.IsValidIndex(I2))
			{
				continue;
			}

			const bool bCapTri = bCapVertices.IsValidIndex(I0) && bCapVertices[I0]
				&& bCapVertices.IsValidIndex(I1) && bCapVertices[I1]
				&& bCapVertices.IsValidIndex(I2) && bCapVertices[I2];
			if (bCapTri)
			{
				continue;
			}

			const FVector FaceNormal = ComputeFaceNormal(Mesh.Vertices, I0, I1, I2);
			const int32 Indices[3] = { I0, I1, I2 };
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Index = Indices[Corner];
				if (bCapVertices.IsValidIndex(Index) && bCapVertices[Index])
				{
					continue;
				}

				Accumulated[Index] += FaceNormal;
				++Counts[Index];
			}
		}

		for (int32 I = 0; I < Mesh.Normals.Num(); ++I)
		{
			if (bCapVertices.IsValidIndex(I) && bCapVertices[I])
			{
				continue;
			}

			if (Counts[I] > 0)
			{
				Mesh.Normals[I] = Accumulated[I].GetSafeNormal();
			}
		}
	}

	static void WeldCoincidentSideNormals(FSvgMeshData& Mesh, const TArray<bool>& bCapVertices)
	{
		if (Mesh.Vertices.IsEmpty() || Mesh.Normals.Num() != Mesh.Vertices.Num())
		{
			return;
		}

		FBox Bounds(ForceInit);
		for (const FVector& Vertex : Mesh.Vertices)
		{
			Bounds += Vertex;
		}

		const float GridSize = FMath::Max(Bounds.GetSize().GetMax() * 0.0001f, 0.001f);
		const float InvGridSize = 1.f / GridSize;

		auto MakePositionKey = [InvGridSize](const FVector& Position) -> uint64
		{
			const int32 X = FMath::RoundToInt(Position.X * InvGridSize);
			const int32 Y = FMath::RoundToInt(Position.Y * InvGridSize);
			const int32 Z = FMath::RoundToInt(Position.Z * InvGridSize);
			return (static_cast<uint64>(static_cast<uint32>(X)) << 42)
				| (static_cast<uint64>(static_cast<uint32>(Y)) << 21)
				| static_cast<uint64>(static_cast<uint32>(Z));
		};

		TMap<uint64, TArray<int32>> PositionGroups;
		PositionGroups.Reserve(Mesh.Vertices.Num());
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			PositionGroups.FindOrAdd(MakePositionKey(Mesh.Vertices[I])).Add(I);
		}

		for (const TPair<uint64, TArray<int32>>& Group : PositionGroups)
		{
			TArray<int32> SideIndices;
			SideIndices.Reserve(Group.Value.Num());

			for (int32 Index : Group.Value)
			{
				if (!bCapVertices.IsValidIndex(Index) || !bCapVertices[Index])
				{
					SideIndices.Add(Index);
				}
			}

			if (SideIndices.Num() < 2)
			{
				continue;
			}

			FVector Average = FVector::ZeroVector;
			for (int32 Index : SideIndices)
			{
				Average += Mesh.Normals[Index];
			}

			const FVector Smoothed = Average.GetSafeNormal();
			if (Smoothed.IsNearlyZero())
			{
				continue;
			}

			for (int32 Index : SideIndices)
			{
				Mesh.Normals[Index] = Smoothed;
			}
		}
	}

	static void NegateSideNormals(FSvgMeshData& Mesh, const TArray<bool>* bCapVertices = nullptr)
	{
		for (int32 I = 0; I < Mesh.Normals.Num(); ++I)
		{
			const bool bIsCap = bCapVertices && bCapVertices->IsValidIndex(I)
				? (*bCapVertices)[I]
				: IsCapNormal(Mesh.Normals[I]);
			if (!bIsCap)
			{
				Mesh.Normals[I] = -Mesh.Normals[I];
			}
		}
	}

	static void FlipSideTriangleWinding(FSvgMeshData& Mesh)
	{
		for (int32 T = 0; T < Mesh.Triangles.Num(); T += 3)
		{
			const int32 I0 = Mesh.Triangles[T];
			const int32 I1 = Mesh.Triangles[T + 1];
			const int32 I2 = Mesh.Triangles[T + 2];
			if (!Mesh.Normals.IsValidIndex(I0)
				|| !Mesh.Normals.IsValidIndex(I1)
				|| !Mesh.Normals.IsValidIndex(I2))
			{
				continue;
			}

			if (IsCapNormal(Mesh.Normals[I0])
				&& IsCapNormal(Mesh.Normals[I1])
				&& IsCapNormal(Mesh.Normals[I2]))
			{
				continue;
			}

			Swap(Mesh.Triangles[T + 1], Mesh.Triangles[T + 2]);
		}
	}

	static FVector ProjectOntoTangentPlane(const FVector& Axis, const FVector& Normal)
	{
		FVector Tangent = Axis - Normal * FVector::DotProduct(Axis, Normal);
		Tangent.Z = 0.f;
		if (!Tangent.Normalize())
		{
			Tangent = FVector(1.f, 0.f, 0.f);
		}
		return Tangent;
	}

	static FVector ComputeSideTangentFromCross(
		const FVector& Normal,
		bool bNormalCrossUp)
	{
		FVector Tangent = bNormalCrossUp
			? FVector::CrossProduct(Normal, FVector::UpVector)
			: FVector::CrossProduct(FVector::UpVector, Normal);
		Tangent.Z = 0.f;
		if (!Tangent.Normalize())
		{
			Tangent = FVector(1.f, 0.f, 0.f);
		}

		return Tangent;
	}

	static void ApplyCrossBasedTangents(
		FSvgMeshData& Mesh,
		bool bNormalCrossUp,
		bool bFlipTangentY)
	{
		Mesh.Tangents.SetNum(Mesh.Vertices.Num());
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			const FVector Normal = Mesh.Normals.IsValidIndex(I)
				? Mesh.Normals[I].GetSafeNormal()
				: FVector::UpVector;

			if (IsCapNormal(Normal))
			{
				Mesh.Tangents[I] = FProcMeshTangent(0.f, -1.f, 0.f);
				continue;
			}

			const FVector Tangent = ComputeSideTangentFromCross(Normal, bNormalCrossUp);
			Mesh.Tangents[I] = FProcMeshTangent(Tangent, bFlipTangentY);
		}
	}

	static void ApplyProjectedAxisTangents(FSvgMeshData& Mesh, const FVector& Axis)
	{
		Mesh.Tangents.SetNum(Mesh.Vertices.Num());
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			const FVector Normal = Mesh.Normals.IsValidIndex(I)
				? Mesh.Normals[I].GetSafeNormal()
				: FVector::UpVector;

			if (IsCapNormal(Normal))
			{
				Mesh.Tangents[I] = FProcMeshTangent(0.f, -1.f, 0.f);
				continue;
			}

			const FVector Tangent = ProjectOntoTangentPlane(Axis, Normal);
			Mesh.Tangents[I] = FProcMeshTangent(Tangent, false);
		}
	}

	static void ApplyAxisCrossNormalTangents(FSvgMeshData& Mesh, const FVector& Axis)
	{
		Mesh.Tangents.SetNum(Mesh.Vertices.Num());
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			const FVector Normal = Mesh.Normals.IsValidIndex(I)
				? Mesh.Normals[I].GetSafeNormal()
				: FVector::UpVector;

			if (IsCapNormal(Normal))
			{
				Mesh.Tangents[I] = FProcMeshTangent(0.f, -1.f, 0.f);
				continue;
			}

			FVector Tangent = FVector::CrossProduct(Axis, Normal);
			Tangent.Z = 0.f;
			if (!Tangent.Normalize())
			{
				Tangent = FVector(1.f, 0.f, 0.f);
			}
			Mesh.Tangents[I] = FProcMeshTangent(Tangent, false);
		}
	}

	static void ApplyUVEdgeDirectionTangents(FSvgMeshData& Mesh)
	{
		Mesh.Tangents.SetNum(Mesh.Vertices.Num());
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			const FVector Normal = Mesh.Normals.IsValidIndex(I)
				? Mesh.Normals[I].GetSafeNormal()
				: FVector::UpVector;

			if (IsCapNormal(Normal))
			{
				Mesh.Tangents[I] = FProcMeshTangent(0.f, -1.f, 0.f);
				continue;
			}

			FVector Tangent(1.f, 0.f, 0.f);
			if (Mesh.UV0.IsValidIndex(I))
			{
				const FVector2D& Uv = Mesh.UV0[I];
				float BestDistSq = MAX_FLT;
				for (int32 J = 0; J < Mesh.Vertices.Num(); ++J)
				{
					if (J == I || !Mesh.UV0.IsValidIndex(J))
					{
						continue;
					}

					const FVector2D& OtherUv = Mesh.UV0[J];
					if (!FMath::IsNearlyEqual(Uv.Y, OtherUv.Y, KINDA_SMALL_NUMBER))
					{
						continue;
					}

					if (FMath::IsNearlyEqual(Uv.X, OtherUv.X, KINDA_SMALL_NUMBER))
					{
						continue;
					}

					const float DistSq = FVector::DistSquared(Mesh.Vertices[I], Mesh.Vertices[J]);
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						Tangent = (Mesh.Vertices[J] - Mesh.Vertices[I]).GetSafeNormal();
					}
				}
			}

			Tangent.Z = 0.f;
			if (!Tangent.Normalize())
			{
				Tangent = ComputeSideTangentFromCross(Normal, false);
			}

			Mesh.Tangents[I] = FProcMeshTangent(Tangent, false);
		}
	}

	static void ApplyUVGradientTangents(FSvgMeshData& Mesh)
	{
		TArray<FVector> TangentSum;
		TArray<float> FlipWeightSum;
		TArray<int32> Counts;
		TangentSum.Init(FVector::ZeroVector, Mesh.Vertices.Num());
		FlipWeightSum.Init(0.f, Mesh.Vertices.Num());
		Counts.Init(0, Mesh.Vertices.Num());

		for (int32 T = 0; T < Mesh.Triangles.Num(); T += 3)
		{
			const int32 I0 = Mesh.Triangles[T];
			const int32 I1 = Mesh.Triangles[T + 1];
			const int32 I2 = Mesh.Triangles[T + 2];
			if (!Mesh.Vertices.IsValidIndex(I0)
				|| !Mesh.Vertices.IsValidIndex(I1)
				|| !Mesh.Vertices.IsValidIndex(I2)
				|| !Mesh.UV0.IsValidIndex(I0)
				|| !Mesh.UV0.IsValidIndex(I1)
				|| !Mesh.UV0.IsValidIndex(I2))
			{
				continue;
			}

			const FVector P0 = Mesh.Vertices[I0];
			const FVector P1 = Mesh.Vertices[I1];
			const FVector P2 = Mesh.Vertices[I2];
			const FVector2D Uv0 = Mesh.UV0[I0];
			const FVector2D Uv1 = Mesh.UV0[I1];
			const FVector2D Uv2 = Mesh.UV0[I2];

			const FVector E1 = P1 - P0;
			const FVector E2 = P2 - P0;
			const FVector2D DeltaUv1 = Uv1 - Uv0;
			const FVector2D DeltaUv2 = Uv2 - Uv0;
			const float Denom = DeltaUv1.X * DeltaUv2.Y - DeltaUv2.X * DeltaUv1.Y;
			if (FMath::Abs(Denom) <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float InvDenom = 1.f / Denom;
			FVector Tangent = (DeltaUv2.Y * E1 - DeltaUv1.Y * E2) * InvDenom;
			Tangent.Z = 0.f;
			if (!Tangent.Normalize())
			{
				continue;
			}

			const FVector Normal = ComputeFaceNormal(Mesh.Vertices, I0, I1, I2);
			const bool bFlipTangentY = FVector::DotProduct(
				FVector::CrossProduct(Normal, Tangent),
				FVector::UpVector) < 0.f;

			const int32 Indices[3] = { I0, I1, I2 };
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 Index = Indices[Corner];
				TangentSum[Index] += Tangent;
				FlipWeightSum[Index] += bFlipTangentY ? 1.f : 0.f;
				++Counts[Index];
			}
		}

		Mesh.Tangents.SetNum(Mesh.Vertices.Num());
		for (int32 I = 0; I < Mesh.Vertices.Num(); ++I)
		{
			const FVector Normal = Mesh.Normals.IsValidIndex(I)
				? Mesh.Normals[I].GetSafeNormal()
				: FVector::UpVector;

			if (IsCapNormal(Normal))
			{
				Mesh.Tangents[I] = FProcMeshTangent(0.f, -1.f, 0.f);
				continue;
			}

			if (Counts[I] > 0)
			{
				FVector Tangent = TangentSum[I].GetSafeNormal();
				if (Tangent.IsNearlyZero())
				{
					Tangent = ComputeSideTangentFromCross(Normal, false);
				}
				const bool bFlipTangentY = FlipWeightSum[I] > (0.5f * Counts[I]);
				Mesh.Tangents[I] = FProcMeshTangent(Tangent, bFlipTangentY);
			}
			else
			{
				Mesh.Tangents[I] = FProcMeshTangent(
					ComputeSideTangentFromCross(Normal, false),
					false);
			}
		}
	}

	static const TCHAR* GetModeLabel(ESvgTangentSpaceMode Mode)
	{
		switch (Mode)
		{
		case ESvgTangentSpaceMode::Default: return TEXT("0 - Up x Normal");
		case ESvgTangentSpaceMode::NormalCrossUp: return TEXT("1 - Normal x Up");
		case ESvgTangentSpaceMode::UpCrossNormal_FlipY: return TEXT("2 - Up x Normal, Flip Y");
		case ESvgTangentSpaceMode::NormalCrossUp_FlipY: return TEXT("3 - Normal x Up, Flip Y");
		case ESvgTangentSpaceMode::FaceNormals: return TEXT("4 - Smooth Face Normals + Up x Normal");
		case ESvgTangentSpaceMode::NegateSideNormals: return TEXT("5 - Negate Side Normals + Up x Normal");
		case ESvgTangentSpaceMode::FromUVGradient: return TEXT("6 - UV Gradient Tangent");
		case ESvgTangentSpaceMode::UVEdgeDirection: return TEXT("7 - UV Edge Direction");
		case ESvgTangentSpaceMode::FlipSideWinding: return TEXT("8 - Flip Side Winding + Face Normals");
		case ESvgTangentSpaceMode::WorldXProjected: return TEXT("9 - World X Projected");
		case ESvgTangentSpaceMode::WorldYProjected: return TEXT("10 - World Y Projected");
		case ESvgTangentSpaceMode::ForwardCrossNormal: return TEXT("11 - Forward x Normal");
		case ESvgTangentSpaceMode::RightCrossNormal: return TEXT("12 - Right x Normal");
		default: return TEXT("Unknown");
		}
	}
}

void FSvgTangentSpaceBuilder::ApplySmoothNormals(FSvgMeshData& Mesh)
{
	if (Mesh.Vertices.IsEmpty())
	{
		return;
	}

	TArray<bool> bCapVertices;
	bCapVertices.SetNum(Mesh.Normals.Num());
	for (int32 I = 0; I < Mesh.Normals.Num(); ++I)
	{
		bCapVertices[I] = SvgTangentSpacePrivate::IsCapNormal(Mesh.Normals[I]);
	}

	SvgTangentSpacePrivate::RecomputeSmoothSideNormals(Mesh, bCapVertices);
	SvgTangentSpacePrivate::WeldCoincidentSideNormals(Mesh, bCapVertices);

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgTangentSpace] Applied smooth side normals to %d vertices (caps preserved)."),
		Mesh.Vertices.Num());
}

void FSvgTangentSpaceBuilder::Apply(FSvgMeshData& Mesh, ESvgTangentSpaceMode Mode)
{
	if (Mesh.Vertices.IsEmpty())
	{
		return;
	}

	switch (Mode)
	{
	case ESvgTangentSpaceMode::Default:
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, false, false);
		break;

	case ESvgTangentSpaceMode::NormalCrossUp:
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, true, false);
		break;

	case ESvgTangentSpaceMode::UpCrossNormal_FlipY:
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, false, true);
		break;

	case ESvgTangentSpaceMode::NormalCrossUp_FlipY:
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, true, true);
		break;

	case ESvgTangentSpaceMode::FaceNormals:
		SvgTangentSpacePrivate::RecomputeSmoothNormalsFromTriangles(Mesh);
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, false, false);
		break;

	case ESvgTangentSpaceMode::NegateSideNormals:
		SvgTangentSpacePrivate::NegateSideNormals(Mesh);
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, false, false);
		break;

	case ESvgTangentSpaceMode::FromUVGradient:
		SvgTangentSpacePrivate::ApplyUVGradientTangents(Mesh);
		break;

	case ESvgTangentSpaceMode::UVEdgeDirection:
		SvgTangentSpacePrivate::ApplyUVEdgeDirectionTangents(Mesh);
		break;

	case ESvgTangentSpaceMode::FlipSideWinding:
		SvgTangentSpacePrivate::FlipSideTriangleWinding(Mesh);
		SvgTangentSpacePrivate::RecomputeSmoothNormalsFromTriangles(Mesh);
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, false, false);
		break;

	case ESvgTangentSpaceMode::WorldXProjected:
		SvgTangentSpacePrivate::ApplyProjectedAxisTangents(Mesh, FVector::ForwardVector);
		break;

	case ESvgTangentSpaceMode::WorldYProjected:
		SvgTangentSpacePrivate::ApplyProjectedAxisTangents(Mesh, FVector::RightVector);
		break;

	case ESvgTangentSpaceMode::ForwardCrossNormal:
		SvgTangentSpacePrivate::ApplyAxisCrossNormalTangents(Mesh, FVector::ForwardVector);
		break;

	case ESvgTangentSpaceMode::RightCrossNormal:
		SvgTangentSpacePrivate::ApplyAxisCrossNormalTangents(Mesh, FVector::RightVector);
		break;

	default:
		SvgTangentSpacePrivate::ApplyCrossBasedTangents(Mesh, false, false);
		break;
	}

	UE_LOG(LogSvgMeshImporter, Log,
		TEXT("[SvgTangentSpace] Applied mode '%s' to %d vertices."),
		SvgTangentSpacePrivate::GetModeLabel(Mode),
		Mesh.Vertices.Num());
}
