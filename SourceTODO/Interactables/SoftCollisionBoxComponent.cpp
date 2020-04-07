// Fill out your copyright notice in the Description page of Project Settings.

#include "SoftCollisionBoxComponent.h"
#include "Interactable/GrabbableActor.h"
#include "Interactable/GrabbableSkelMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY(LogSoftCollisionBox);

USoftCollisionBoxComponent::USoftCollisionBoxComponent()
{
	// Setup this component.
	SetCollisionProfileName("SoftColliderBox");
	ComponentTags.Add("SoftCollider");
	SetUseCCD(true);

	// Bind overlap events.
	this->OnComponentBeginOverlap.AddDynamic(this, &USoftCollisionBoxComponent::BeginOverlap);
	this->OnComponentEndOverlap.AddDynamic(this, &USoftCollisionBoxComponent::EndOverlap);

	// Initialise variables.
	debug = false;
}

void USoftCollisionBoxComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure that anything inside of the bounds on begin play does not collide with the hand Skel until the component/actor has left the drawer.
	TArray<UPrimitiveComponent*> currentOverlappingComps;
	this->GetOverlappingComponents(currentOverlappingComps);

	// If anything thats currently overlapped is an interactable that can be grabbed... disable collision with the handSkel mesh.
	for (UPrimitiveComponent* comp : currentOverlappingComps)
	{
		if (AGrabbableActor* isGrabbableActor = Cast<AGrabbableActor>(comp->GetOwner()))
		{
			if (UStaticMeshComponent* grabbable = Cast<UStaticMeshComponent>(isGrabbableActor->grabbableMesh))
			{
				if (!originalValues.Contains(grabbable))
				{
					// Save the original collision response.
					originalValues.Add(grabbable, grabbable->GetCollisionResponseToChannel(ECC_Hand));

					// Apply new collision response.
					grabbable->SetCollisionResponseToChannel(ECC_Hand, ECR_Ignore);
				}
			}
		}
		else if (UGrabbableSkelMesh* isGrabbbaleSkelMesh = Cast<UGrabbableSkelMesh>(comp))
		{
			if (!originalValues.Contains(isGrabbbaleSkelMesh))
			{
				// Save the original collision response.
				originalValues.Add(isGrabbbaleSkelMesh, isGrabbbaleSkelMesh->GetCollisionResponseToChannel(ECC_Hand));

				// Apply new collision response.
				isGrabbbaleSkelMesh->SetCollisionResponseToChannel(ECC_Hand, ECR_Ignore);
			}	
		}
	}

#if DEVELOPMENT
	// If enabled debug the components that have had soft collision enabled.
	if (debug)
	{
		for (TPair<UPrimitiveComponent*, ECollisionResponse>& item : originalValues)
		{
			UE_LOG(LogSoftCollisionBox, Log, TEXT("The component %s, has had its reponse to the hand changed to ignore."), *item.Key->GetName());
		}
	}
#endif
}

void USoftCollisionBoxComponent::BeginOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// If overlap is detected and it is a interactable that can be grabbed, ensure the handSkel ignores these components/actors. NOTE: Prevents forcing through meshes due to static moving collision.
	if (AGrabbableActor* isGrabbableActor = Cast<AGrabbableActor>(OtherActor))
	{
		if (UStaticMeshComponent* grabbable = Cast<UStaticMeshComponent>(isGrabbableActor->grabbableMesh))
		{
			if (!originalValues.Contains(grabbable))
			{
				// Save the original collision response.
				originalValues.Add(grabbable, grabbable->GetCollisionResponseToChannel(ECC_Hand));

				// Apply new collision response.
				grabbable->SetCollisionResponseToChannel(ECC_Hand, ECR_Ignore);
			}
		}
	}
	else if (UGrabbableSkelMesh* isGrabbbaleSkelMesh = Cast<UGrabbableSkelMesh>(OtherComp))
	{
		if (!originalValues.Contains(isGrabbbaleSkelMesh))
		{
			// Save the original collision response.
			originalValues.Add(isGrabbbaleSkelMesh, isGrabbbaleSkelMesh->GetCollisionResponseToChannel(ECC_Hand));

			// Apply new collision response.
			isGrabbbaleSkelMesh->SetCollisionResponseToChannel(ECC_Hand, ECR_Ignore);
		}
	}
}

void USoftCollisionBoxComponent::EndOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// If overlap end is detected, return the component to collide with the hand or whatever collision response it originally had.
	if (AGrabbableActor* isGrabbableActor = Cast<AGrabbableActor>(OtherActor))
	{
		if (UStaticMeshComponent* grabbable = Cast<UStaticMeshComponent>(isGrabbableActor->grabbableMesh))
		{
			if (originalValues.Contains(grabbable))
			{	
				// Apply new collision response.
				isGrabbableActor->grabbableMesh->SetCollisionResponseToChannel(ECC_Hand, *originalValues.Find(grabbable));
				
				// Save the original collision response.
				originalValues.Remove(grabbable);		
			}
		}
	}
	else if (UGrabbableSkelMesh* isGrabbbaleSkelMesh = Cast<UGrabbableSkelMesh>(OtherComp))
	{
		if (originalValues.Contains(isGrabbbaleSkelMesh))
		{
			// Apply new collision response.
			// isGrabbableActor->grabbableMesh->SetCollisionResponseToChannel(ECC_Hand, *originalValues.Find(isGrabbbaleSkelMesh));

			// Save the original collision response.
			originalValues.Remove(isGrabbbaleSkelMesh);
		}
	}
}