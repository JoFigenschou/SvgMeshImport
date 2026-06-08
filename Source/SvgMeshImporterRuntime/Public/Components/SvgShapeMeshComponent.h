#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

#include "SvgShapeMeshComponent.generated.h"

/** Per-shape display mesh. Not individually movable in the editor — move the SvgMeshActor instead. */
UCLASS(ClassGroup = (SVG), meta = (BlueprintSpawnableComponent))
class SVGMESHIMPORTERRUNTIME_API USvgShapeMeshComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	USvgShapeMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
