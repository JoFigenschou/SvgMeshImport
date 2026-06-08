#include "Components/SvgShapeMeshComponent.h"

USvgShapeMeshComponent::USvgShapeMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMobility(EComponentMobility::Static);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetIsVisualizationComponent(true);
	bSelectable = false;
}
