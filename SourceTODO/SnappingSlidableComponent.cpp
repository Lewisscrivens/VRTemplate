// Fill out your copyright notice in the Description page of Project Settings.

#include "SnappingSlidableComponent.h"
#include "SlidableStaticMesh.h"
#include "TimerManager.h"
#include "Interactable/GrabbableActor.h"
#include "Player/VRHand.h"
#include "GameFramework/Actor.h"
#include "GameMode/GM_Basement.h"

DEFINE_LOG_CATEGORY(LogSnapSlidingComp);

USnappingSlidableComponent::USnappingSlidableComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Setup collision properties for snapping comp.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToChannel(ECC_Grabbable, ECollisionResponse::ECR_Overlap);
	SetGenerateOverlapEvents(true);
	SetUseCCD(true);
	bMultiBodyOverlap = true;

	// Bind to this components overlap begin and end function.
	OnComponentBeginOverlap.AddDynamic(this, &USnappingSlidableComponent::OverlapBegin);

	// Setup default variables for this class.
	axisToSlide = ESlideAxis::X;
	slidingLimit = 10.0f;
	releasedLerpTime = 0.8f;
}

void USnappingSlidableComponent::BeginPlay()
{
	Super::BeginPlay();

	// Spawn and initalise the sliding component ready for use.
	InitSlidingComponent();

	// Start timer for updating slidable state.
	FTimerDelegate updateSlidable;
	updateSlidable.BindUFunction(this, "UpdateSlidableState");
	GetWorld()->GetTimerManager().SetTimer(updateTimer, updateSlidable, 0.001f, true);
}

void USnappingSlidableComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void USnappingSlidableComponent::UpdateSlidableState()
{
	// If snapped into a sliding mode check hand distance and current slide position to determine if the grabbable actor should be lerped back to the hand.
	if (snappedGrabbable && snappedGrabbable->IsValidLowLevel())
	{
		if (slidingMesh->handRef && slidingMesh->interactableSettings.handDistance > 4.0f && slidingMesh->currentPosition == 0.0f)
		{
			// Release the slidable from the hand.
			AGrabbableActor* grabbable = snappedGrabbable;
			handRegrab = slidingMesh->handRef;
			slidingMesh->handRef->ReleaseGrabbedActor();

			// Call unsnapped delegate
			OnSnapDisconnect.Broadcast(snappedGrabbable);

			// Get original grab offsets before snapped into sliding mode.
			FVector grabbedLocation = handRegrab->grabCollider->GetComponentTransform().TransformPositionNoScale(originalGrabOffset.GetLocation());
			FRotator grabbedRotation = handRegrab->grabCollider->GetComponentTransform().TransformRotation(originalGrabOffset.GetRotation()).Rotator();

			// Remove old variable values and bindings.
			if (snappedGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) snappedGrabbable->OnMeshGrabbed.RemoveDynamic(this, &USnappingSlidableComponent::OnGrabbableGrabbed);

			// Disconnect and Set world location of overlapping grabbable then grab it.
			snappedGrabbable->grabbableMesh->SetWorldLocationAndRotation(grabbedLocation, grabbedRotation, false, nullptr, ETeleportType::TeleportPhysics);
			handRegrab->ForceGrab(grabbable);

			// Snapped grabbable is now disconnected.
			snappedGrabbable = nullptr;
		}
		else if (lerpSlidableToLimit && !slidingMesh->handRef)
		{
			// Lerp to the target position.
			float lerpProgress = GetWorld()->GetTimeSeconds() - interpolationStartTime;
			float alpha = FMath::Clamp(lerpProgress / releasedLerpTime, 0.0f, 1.0f);
			FVector lerpingLocation = FMath::Lerp(slidingStartLoc, relativeSlidingLerpPos, alpha);
			slidingMesh->SetRelativeLocation(lerpingLocation);

			// When the lerp has finished and the slidingMesh is in position.
			if (alpha >= 1.0f)
			{
				lerpSlidableToLimit = false;
				OnSnapConnect.Broadcast(snappedGrabbable);
			}
		}
	}
}

void USnappingSlidableComponent::OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Continue if the overlapped actor is a grabbable actor.
	if (AGrabbableActor* grabbableActor = Cast<AGrabbableActor>(OtherActor))
	{
		// Return if the overlapped grabbable actor doesn't have the correct snap tag or there is currently something already snapped.
		if (snappedGrabbable || (snappingTag != "NULL" && !grabbableActor->ActorHasTag(snappingTag)) || !grabbableActor->handRefInfo.handRef) return;

		// Save grabbable info.
		if (AVRHand* overlappingHand = grabbableActor->handRefInfo.handRef)
		{
			// Save hand offsets.
			originalGrabOffset = FTransform(grabbableActor->handRefInfo.originalPickupRelativeRotation.Quaternion(), grabbableActor->handRefInfo.originalRelativePickupOffset, FVector(1.0f));

			// Release the grabbable from the hand.
			overlappingHand->ReleaseGrabbedActor();

			// Snap grabbable to the start of the sliding mesh.
			grabbableActor->grabbableMesh->SetSimulatePhysics(false);
			grabbableActor->AttachToComponent(slidingMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			grabbableActor->grabbableMesh->SetRelativeLocationAndRotation(locationOffset, rotationOffset);

			// Bind to the snappedGrabbables grabbed function so it can be canceled and redirected to grab the slidngMesh.
			if (!grabbableActor->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) grabbableActor->OnMeshGrabbed.AddDynamic(this, &USnappingSlidableComponent::OnGrabbableGrabbed);

			// Grab the sliding static mesh comp.
			overlappingHand->ForceGrab(slidingMesh);
			snappedGrabbable = grabbableActor;
			return;
		}
		else return;
	}
}

void USnappingSlidableComponent::InitSlidingComponent()
{
	// Spawn and setup the sliding component with the slidableOptions struct.
	FName uniqueCompName = MakeUniqueObjectName(this->GetOwner(), USlidableStaticMesh::StaticClass(), FName("slidingComp"));
	slidingMesh = NewObject<USlidableStaticMesh>(this->GetOwner(), USlidableStaticMesh::StaticClass());
	slidingMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	slidingMesh->slideLimit = slidingLimit;
	slidingMesh->currentAxis = axisToSlide;
	slidingMesh->RegisterComponent();

	// Bind to the on released delegate if slidable options lerp to limit on release is enabled.
	slidingMesh->OnMeshGrabbed.AddDynamic(this, &USnappingSlidableComponent::OnSlidableGrabbed);

	// Bind to the on released delegate if slidable options lerp to limit on release is enabled.
	slidingMesh->OnMeshReleased.AddDynamic(this, &USnappingSlidableComponent::OnSlidableReleased);
}

void USnappingSlidableComponent::OnSlidableGrabbed(AVRHand* hand)
{
	if (snappedGrabbable)
	{
		// Broadcast to snapped disconnection delegate.
		OnSnapDisconnect.Broadcast(snappedGrabbable);
	}
}

void USnappingSlidableComponent::OnSlidableReleased(AVRHand* hand)
{
	// If the slidable should lerp to the snapped position on release.
	if (snappedGrabbable)
	{
		// Get the position to lerp into.
		relativeSlidingLerpPos = slidingMesh->originalRelativeTransform.GetLocation();
		switch (slidingMesh->currentAxis)
		{
		case ESlideAxis::X:
			relativeSlidingLerpPos.X = slidingLimit;
			break;
		case ESlideAxis::Y:
			relativeSlidingLerpPos.Y = slidingLimit;
			break;
		case ESlideAxis::Z:
			relativeSlidingLerpPos.Z = slidingLimit;
			break;
		}

		// Call delegate.
		if (slidingMesh->currentPosition == slidingLimit)
		{
			OnSnapConnect.Broadcast(snappedGrabbable);
			return;
		}

		// Otherwise start the lerp.
		lerpSlidableToLimit = true;
		interpolationStartTime = GetWorld()->GetTimeSeconds();
		slidingStartLoc = slidingMesh->RelativeLocation;
	}
}

void USnappingSlidableComponent::OnGrabbableGrabbed(AVRHand* hand, UPrimitiveComponent* compPressed)
{
	// Ensure component is snapped.
	if (snappedGrabbable)
	{
		// Disable the grab function for the grabbable.
		snappedGrabbable->cancelGrab = true;
		// Fix for highlighting not disabling after grabbing.
		IHandsInterface::Execute_EndOverlapping(snappedGrabbable, hand);
		// Force grab on the sliding mesh instead.
		hand->ForceGrab(slidingMesh);
	}
	return;
}

void USnappingSlidableComponent::ForceSnap(AGrabbableActor* actorToSnap)
{
	// Continue if the actor to snap is valid.
	if (actorToSnap && actorToSnap->IsValidLowLevel())
	{
		// Return if the overlapped grabbable actor doesn't have the correct snap tag or there is currently something already snapped.
		if (snappedGrabbable || (snappingTag != "NULL" && !actorToSnap->ActorHasTag(snappingTag))) return;

		// Save grabbable info.
		snappedGrabbable = actorToSnap;

		// Snap grabbable to the start of the sliding mesh.
		snappedGrabbable->grabbableMesh->SetSimulatePhysics(false);
		snappedGrabbable->AttachToComponent(slidingMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		snappedGrabbable->grabbableMesh->SetRelativeLocationAndRotation(locationOffset, rotationOffset);

		// Bind to the snappedGrabbables grabbed function so it can be canceled and redirected to grab the slidngMesh.
		if (!snappedGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) snappedGrabbable->OnMeshGrabbed.AddDynamic(this, &USnappingSlidableComponent::OnGrabbableGrabbed);

		// Start the lerp into the slidable limit
		OnSlidableReleased(nullptr);
	}
}

void USnappingSlidableComponent::ForceRelease()
{
	if (slidingMesh && snappedGrabbable)
	{
		// Remove delegate before re-grabbed.
		if (snappedGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) snappedGrabbable->OnMeshGrabbed.RemoveDynamic(this, &USnappingSlidableComponent::OnGrabbableGrabbed);
		snappedGrabbable->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		snappedGrabbable->grabbableMesh->SetSimulatePhysics(true);

		// Remove old variable values.
		snappedGrabbable = nullptr;
		lerpSlidableToLimit = false;
	}
}

