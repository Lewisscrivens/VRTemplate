// Fill out your copyright notice in the Description page of Project Settings.

#include "Project/SnappingRotatableComponent.h"
#include "Interactables/RotatableStaticMesh.h"
#include "TimerManager.h"
#include "Interactables/GrabbableActor.h"
#include "Player/VRHand.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogSnapRotComp);

USnappingRotatableComponent::USnappingRotatableComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Setup collision properties for snapping comp.
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionResponseToChannel(ECC_Interactable, ECollisionResponse::ECR_Overlap);
	SetGenerateOverlapEvents(true);

	// Bind to this components overlap begin and end function.
	OnComponentBeginOverlap.AddDynamic(this, &USnappingRotatableComponent::OverlapBegin);

	// Setup default variables.
	lockOnLimit = true;
	rotatingLimit = 90.0f;
	returningDistance = 15.0f;
	limitReachedSound = nullptr;
	limitReachedHaptics = nullptr;
	handRegrab = nullptr;
	snappedGrabbable = nullptr;
	rotatableMesh = nullptr;
	limitReached = false;
}

void USnappingRotatableComponent::BeginPlay()
{
	Super::BeginPlay();

	// Spawn and initalise the sliding component ready for use.
	InitRotatableComponent();

	// Start timer for updating slidable state.
	FTimerDelegate updateDel;
	updateDel.BindUFunction(this, "UpdateRotatableState");
	GetWorld()->GetTimerManager().SetTimer(updateTimer, updateDel, 0.001f, true);
}

void USnappingRotatableComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//...
}

void USnappingRotatableComponent::UpdateRotatableState()
{
	// If something is snapped continue.
	if (snappedGrabbable && snappedGrabbable->IsValidLowLevel())
	{
		// Call delegates for entering and exiting the limit...
		if (rotatableMesh->cumulativeAngle == rotatingLimit)
		{
			// Only call the first time it is reached.
			if (!limitReached)
			{
				limitReachedDel.Broadcast();
				limitReached = true;
			}
		}
		// Otherwise if it was reached but exited call the exited delegate.
		else if (limitReached)
		{
			limitExitedDel.Broadcast();
			limitReached = false;
		}

		// If the rotatable mesh is grabbed and is moving away disconnect the grabbable. NOTE: Add this back in if u only want to disconnect if no rotation has happened. "&& rotatableMesh->cumulativeAngle == 0.0f."
		if (rotatableMesh->handRef && rotatableMesh->interactableSettings.handDistance > returningDistance)
		{
			// Release the rotatable from the hand.
			AGrabbableActor* grabbable = snappedGrabbable;
			handRegrab = rotatableMesh->handRef;
			rotatableMesh->handRef->ReleaseGrabbedActor();

			// Call un-snapped delegate
			OnSnapDisconnect.Broadcast(snappedGrabbable);

			// Get original grab offsets before snapped into this component.
			FVector grabbedLocation = handRegrab->grabCollider->GetComponentTransform().TransformPositionNoScale(originalGrabOffset.GetLocation());
			FRotator grabbedRotation = handRegrab->grabCollider->GetComponentTransform().TransformRotation(originalGrabOffset.GetRotation()).Rotator();

			// Remove old variable values and bindings.
			if (snappedGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) snappedGrabbable->OnMeshGrabbed.RemoveDynamic(this, &USnappingRotatableComponent::OnGrabbableGrabbed);

			// Disconnect and Set world location of overlapping grabbable then grab it.
			snappedGrabbable->grabbableMesh->SetWorldLocationAndRotation(grabbedLocation, grabbedRotation, false, nullptr, ETeleportType::TeleportPhysics);
			handRegrab->ForceGrab(grabbable);
			snappedGrabbable->grabbableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

			// Snapped grabbable is now disconnected.
			snappedGrabbable = nullptr;
		}
	}
}

void USnappingRotatableComponent::OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
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
			grabbableActor->AttachToComponent(rotatableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			grabbableActor->grabbableMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			grabbableActor->grabbableMesh->SetRelativeLocationAndRotation(locationOffset, rotationOffset);

			// Bind to the snappedGrabbables grabbed function so it can be canceled and redirected to grab the slidngMesh.
			if (!grabbableActor->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) grabbableActor->OnMeshGrabbed.AddDynamic(this, &USnappingRotatableComponent::OnGrabbableGrabbed);

			// Grab the rotatable static mesh comp.
			overlappingHand->ForceGrab(rotatableMesh);
			snappedGrabbable = grabbableActor;

			// Call the snap connect delegate. 
			OnSnapConnect.Broadcast(snappedGrabbable);
			return;
		}
		else return;
	}
}

void USnappingRotatableComponent::InitRotatableComponent()
{
	// Spawn and setup the twistable rotating component with the slidableOptions struct.
	FName uniqueCompName = MakeUniqueObjectName(this->GetOwner(), URotatableStaticMesh::StaticClass(), FName("twistingComp"));
	rotatableMesh = NewObject<URotatableStaticMesh>(this->GetOwner(), URotatableStaticMesh::StaticClass());
	rotatableMesh->AttachToComponent(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	rotatableMesh->rotateMode = EStaticRotation::Twist;
	rotatableMesh->fakePhysics = false;
	rotatableMesh->rotationLimit = rotatingLimit;

	// If lock on limit setup needed values.
	if (lockOnLimit)
	{
		rotatableMesh->lockable = true;
		rotatableMesh->grabWhileLocked = false;
		rotatableMesh->interpolateToLock = false;
		rotatableMesh->lockingDistance = 2.0f;
		rotatableMesh->unlockingDistance = 3.0f;
		rotatableMesh->lockingPoints.Add(rotatingLimit);
		rotatableMesh->lockSound = limitReachedSound;
		rotatableMesh->lockHapticEffect = limitReachedHaptics;

		// Bind to locked delegate function.
		rotatableMesh->OnRotatableLock.AddDynamic(this, &USnappingRotatableComponent::OnRotatableLocked);
	}

	// Register the comp.
	rotatableMesh->RegisterComponent();
}

void USnappingRotatableComponent::OnRotatableLocked(float angle, URotatableStaticMesh* rotatable)
{
	// Disable the grabbing ability on the grabbable acting as a rotatable when snapped then locked into position.
	if (snappedGrabbable) snappedGrabbable->interactableSettings.canInteract = false;
}

void USnappingRotatableComponent::OnGrabbableGrabbed(AVRHand* hand, UPrimitiveComponent* compPressed)
{
	// Ensure component is snapped.
	if (snappedGrabbable)
	{
		// Disable the grab function for the grabbable.
		snappedGrabbable->cancelGrab = true;

		// Fix for highlighting not disabling after grabbing.
		IHandsInterface::Execute_EndOverlapping(snappedGrabbable, hand);

		// Force grab on the rotatable mesh instead.
		hand->ForceGrab(rotatableMesh);
	}
	return;
}

void USnappingRotatableComponent::ForceSnap(AGrabbableActor* actorToSnap)
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
		snappedGrabbable->AttachToComponent(rotatableMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		snappedGrabbable->grabbableMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		snappedGrabbable->grabbableMesh->SetRelativeLocationAndRotation(locationOffset, rotationOffset);

		// Bind to the snappedGrabbables grabbed function so it can be canceled and redirected to grab the slidngMesh.
		if (!snappedGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) snappedGrabbable->OnMeshGrabbed.AddDynamic(this, &USnappingRotatableComponent::OnGrabbableGrabbed);
	}
}

void USnappingRotatableComponent::ForceRelease()
{
	if (rotatableMesh && snappedGrabbable)
	{
		// Remove delegate before re-grabbed.
		if (snappedGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbableGrabbed")) snappedGrabbable->OnMeshGrabbed.RemoveDynamic(this, &USnappingRotatableComponent::OnGrabbableGrabbed);
		snappedGrabbable->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		snappedGrabbable->grabbableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		snappedGrabbable->grabbableMesh->SetSimulatePhysics(true);

		// Remove old variable values.
		snappedGrabbable = nullptr;
	}
}

