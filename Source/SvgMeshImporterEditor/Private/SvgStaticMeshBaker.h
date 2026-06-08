#pragma once

#include "CoreMinimal.h"

class ASvgMeshActor;
class UStaticMesh;
struct FSvgMeshBuildResult;
struct FSvgMeshData;

struct FSvgStaticMeshBaker
{
	static bool BakeAndApply(ASvgMeshActor* Actor, const FSvgMeshBuildResult& Result);

private:
	static FString SanitizeAssetName(const FString& Name);
	static FString GetActorBakeRoot(ASvgMeshActor* Actor);
	static void DeleteActorBakedAssets(const FString& BakeRoot);
	static FSvgMeshData ToShapeLocalMesh(const FSvgMeshData& WorldMesh, FVector& OutCenter);
	static UStaticMesh* CreateStaticMeshAsset(
		const FString& PackageName,
		const FString& AssetName,
		const FSvgMeshData& MeshData,
		bool bBuildCollision);
	static bool SaveStaticMeshAsset(UStaticMesh* StaticMesh, const FString& PackageName);
};
