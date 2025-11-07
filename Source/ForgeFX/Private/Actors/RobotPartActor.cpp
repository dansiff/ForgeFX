#include "Actors/RobotPartActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HighlightComponent.h"
#include "Materials/MaterialInterface.h"

ARobotPartActor::ARobotPartActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Highlight = CreateDefaultSubobject<UHighlightComponent>(TEXT("Highlight"));
}

void ARobotPartActor::InitializePart(FName InPartName, UStaticMesh* InMesh, const TArray<UMaterialInterface*>& InMaterials)
{
	PartName = InPartName;
	if (InMesh)
	{
		Mesh->SetStaticMesh(InMesh);
	}
	for (int32 i=0;i<InMaterials.Num();++i)
	{
		Mesh->SetMaterial(i, InMaterials[i]);
	}
}

void ARobotPartActor::EnablePhysics(bool bEnable)
{
	if (Mesh)
	{
		Mesh->SetSimulatePhysics(bEnable);
		Mesh->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::QueryOnly);
	}
}

void ARobotPartActor::OnHoverBegin_Implementation()
{
	if (Highlight) Highlight->SetHighlighted(true);
}

void ARobotPartActor::OnHoverEnd_Implementation()
{
	if (Highlight) Highlight->SetHighlighted(false);
}
