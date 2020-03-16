// Fill out your copyright notice in the Description page of Project Settings.

#include "SnappingActor.h"
#include "Interactable/GrabbableActor.h"
#include "Interactable/GrabbableSkelMesh.h"
#include "Player/HandsInterface.h"
#include "Player/VRHand.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/Material.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include <PhysicsEngine/BodyInstance.h>
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogSnappingActor);

ASnappingActor::ASnappingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PrePhysics;

	// Setup the box component as the root component of this actor.
	snapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SnappingBox"));
	snapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	snapBox->SetCollisionResponseToChannel(ECC_Grabbable, ECR_Overlap);
	snapBox->SetCollisionResponseToChannel(ECC_ConstrainedComp, ECR_Overlap);
	snapBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

	// Bind overlap event to this box component.
	snapBox->OnComponentBeginOverlap.AddDynamic(this, &ASnappingActor::OverlapBegin);
	snapBox->OnComponentEndOverlap.AddDynamic(this, &ASnappingActor::OverlapEnd);
	RootComponent = snapBox;

	// Setup the preview component.
	previewComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewCompoenent"));
	previewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Variable defaults.
	snapMode = ESnappingMode::Instant;
	interpMode = EInterpMode::Disabled;
	full = false;
	previewMaterial = UGlobals::GetMaterial(M_Translucent);
	rotateAroundYaw = false;
	snatch = false;
	rotationSpeed = 1.0f;
	timeToInterp = 10.0f;
	locationOffset = FVector::ZeroVector;
	rotationOffset = FRotator::ZeroRotator;
	updateAnim = false;
	snappingTag = "NULL";
	componentSnapped = false;
	lerpSlidableToLimit = false;
	returnSlidableToHand = false;

	// Setup physics handle for physics snap mode.
	physicsHandleSettings = FPhysicsHandleData();
	physicsHandleSettings.handleDataEnabled = true;
	physicsHandleSettings.interpolate = timeToInterp != 0;
	physicsHandleSettings.linearStiffness = 5000.0f;
	physicsHandleSettings.angularStiffness = 5000.0f;
}

void ASnappingActor::BeginPlay()
{
	Super::BeginPlay();

	// Initialise the components if in a sliding or twisting mode for keys, floppy drives etc.
	switch (snapMode)
	{
	case ESnappingMode::Slidable:
		InitSlidingComponent();
	break;
	case ESnappingMode::Twistable:
		InitTwistingComponent();
	break;
	}

	// If there is an actor to snap ensure it is a snappable object and snap it into position. Delay this to next frame to give all other actors a chance to bind to the on release function.
	FTimerDelegate nextFrame;
	nextFrame.BindLambda([&]{ if (actorToSnap) { ForceSnap(actorToSnap); } });
	GetWorldTimerManager().SetTimerForNextTick(nextFrame);
	
}

void ASnappingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Interpolate preview mesh if interpolating is enabled.
	if (interpMode != EInterpMode::Disabled) Interpolate(DeltaTime);

	// If snapped into a sliding mode check hand distance and current slide position to determine if the grabbable actor should be lerped back to the hand.
	if (snapMode == ESnappingMode::Slidable && componentSnapped && overlappingGrabbable && overlappingGrabbable->IsValidLowLevel())
	{
		if ((slidingMesh->interactableSettings.handDistance > 4.0f && slidingMesh->currentPosition == 0.0f || returnSlidableToHand) && checkSlidable)
		{
			// Initiate the lerp to the hand for the slidable/overlappingGrabbbale attached to the sliding mesh.
			if (!returnSlidableToHand && slidingMesh->handRef)
			{
				returnSlidableToHand = true;
				interpolationStartTime = GetWorld()->GetTimeSeconds();
				componentToInterpolate = overlappingGrabbable->grabbableMesh;
				interpStartTransform = componentToInterpolate->GetComponentTransform();
				handRegrab = slidingMesh->handRef;
				slidingMesh->handRef->ReleaseGrabbedActor();
			}
			else return;

			// If lerp to hand is enabled do so otherwise set location to last grab offset.
			if (slidableOptions.lerpToHandOnReturn)
			{
				// Lerp to target position/rotation.
				FVector lerpLocation = handRegrab->grabCollider->GetComponentTransform().TransformPositionNoScale(originalGrabOffset.GetLocation());
				FRotator lerpRotation = handRegrab->grabCollider->GetComponentTransform().TransformRotation(originalGrabOffset.GetRotation()).Rotator();
				float lerpProgress = GetWorld()->GetTimeSeconds() - interpolationStartTime;
				float alpha = FMath::Clamp(lerpProgress / slidableOptions.handLerpTime, 0.0f, 1.0f);
				FVector lerpingLocation = FMath::Lerp(interpStartTransform.GetLocation(), lerpLocation, alpha);
				FRotator lerpingRotation = FMath::Lerp(interpStartTransform.GetRotation().Rotator(), lerpRotation, alpha);
				componentToInterpolate->SetWorldLocationAndRotation(lerpingLocation, lerpingRotation);

				// When complete detach the component from the sliding mesh and grab with the hand.
				if (alpha >= 1.0f)
				{
					// Call unsnapped delegate
					OnSnapDisconnect.Broadcast(overlappingGrabbable->grabbableMesh);

					// Remove grab delegate.
					if (overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);

					// Grab at hand pos.
					overlappingGrabbable->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
					handRegrab->ForceGrab(overlappingGrabbable);

					// Remove old variable values.
					returnSlidableToHand = false;
					componentSnapped = false;
					overlappingGrabbable = nullptr;
				}
			}
			else
			{
				// Call unsnapped delegate
				OnSnapDisconnect.Broadcast(overlappingGrabbable->grabbableMesh);

				// Get original grab offsets before snapped into sliding mode.
				FVector grabbedLocation = handRegrab->grabCollider->GetComponentTransform().TransformPositionNoScale(originalGrabOffset.GetLocation());
				FRotator grabbedRotation = handRegrab->grabCollider->GetComponentTransform().TransformRotation(originalGrabOffset.GetRotation()).Rotator();

				// Remove delegate before re-grabbed.
				if (overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);

				// Disconnect and Set world location of overlapping grabbable then grab it.
				overlappingGrabbable->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				overlappingGrabbable->grabbableMesh->SetWorldLocationAndRotation(grabbedLocation, grabbedRotation, false, nullptr, ETeleportType::TeleportPhysics);
				handRegrab->ForceGrab(overlappingGrabbable);

				// Remove old variable values.
				returnSlidableToHand = false;
				componentSnapped = false;
				overlappingGrabbable = nullptr;
				checkSlidable = false;
			}
		}
		else if (lerpSlidableToLimit)
		{
			// Lerp to the target position.
			float lerpProgress = GetWorld()->GetTimeSeconds() - interpolationStartTime;
			float alpha = FMath::Clamp(lerpProgress / slidableOptions.releasedLerpTime, 0.0f, 1.0f);
			FVector lerpingLocation = FMath::Lerp(slidingStartLoc, relativeSlidingLerpPos, alpha);
			slidingMesh->SetRelativeLocation(lerpingLocation);

			// When the lerp has finished and the slidingMesh is in position.
			if (alpha >= 1.0f)
			{
				lerpSlidableToLimit = false;
				checkSlidable = true;
				OnSnapConnectGrabbable.Broadcast(overlappingGrabbable);
			}
		}
  	}
	else if (!overlappingGrabbable || !overlappingGrabbable->IsValidLowLevel())
	{
		// Remove old variable values.
		returnSlidableToHand = false;
		componentSnapped = false;
		overlappingGrabbable = nullptr;		
	}
}

#if WITH_EDITOR
bool ASnappingActor::CanEditChange(const UProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	// Can only change physics handle settings if in the physics mode(s).
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ASnappingActor, physicsHandleSettings))
	{
		return snapMode == ESnappingMode::PhysicsOnRelease;
	}

	return ParentVal;
}
#endif

void ASnappingActor::OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Get the enum for type of component to use/effect.
	EPreviewMeshSetup compType;
	bool newOverlap = false;
	if (AGrabbableActor* grabbableActor = Cast<AGrabbableActor>(OtherActor))
	{
		if (overlappingGrabbable || (snappingTag != "NULL" && !grabbableActor->ActorHasTag(snappingTag))) return;
		newOverlap = grabbableActor->handRefInfo.handRef || (overlappingGrabbable && grabbableActor == overlappingGrabbable);
		compType = EPreviewMeshSetup::GrabbableActor;

		// If slidable or twistable setup intractable component with overlapped grabbable and return.
		if (snapMode == ESnappingMode::Slidable)
		{
			// Save grabbable info.
			overlappingGrabbable = grabbableActor;
			if (AVRHand* overlappingHand = overlappingGrabbable->handRefInfo.handRef)
			{
				// Save hand offsets.
				originalGrabOffset = FTransform(overlappingGrabbable->handRefInfo.originalPickupRelativeRotation.Quaternion(),
												overlappingGrabbable->handRefInfo.originalRelativePickupOffset, FVector(1.0f));

				// Release the grabbable from the hand.
				overlappingHand->ReleaseGrabbedActor();

				// Snap grabbable to the start of the sliding mesh.
				overlappingGrabbable->grabbableMesh->SetSimulatePhysics(false);
				overlappingGrabbable->AttachToComponent(slidingMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				overlappingGrabbable->grabbableMesh->SetRelativeLocationAndRotation(locationOffset, rotationOffset);

				// Bind to on mesh grabbed so the user cannot grab the grabbable actor, they can only grab the slidingMesh instead through the snapped grabbable actor.
				if (!overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);

				// Grab the sliding static mesh comp.
				overlappingHand->ForceGrab(slidingMesh);
				componentSnapped = true;
				checkSlidable = true;
				return;
			}
			else return;
		}
		else if (snapMode == ESnappingMode::Twistable)
		{
			// Save grabbable.
			overlappingGrabbable = grabbableActor;
			if (AVRHand* overlappingHand = overlappingGrabbable->handRefInfo.handRef)
			{
				// Call twistable snap connect delegate.
				OnSnapConnectGrabbable.Broadcast(overlappingGrabbable);

				// Release the grabbable from the hand.
				overlappingHand->ReleaseGrabbedActor();

				// Snap grabbable to the start of the sliding mesh.
				overlappingGrabbable->grabbableMesh->SetSimulatePhysics(false);
				overlappingGrabbable->AttachToComponent(twistingMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				overlappingGrabbable->grabbableMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				overlappingGrabbable->grabbableMesh->SetRelativeLocationAndRotation(locationOffset, rotationOffset);

				// Bind to on mesh grabbed so the user cannot grab the grabbable actor, they can only grab the twistingMesh instead through the snapped grabbable actor.
				if (!overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);

				// Grab the sliding static mesh comp.
				overlappingHand->ForceGrab(twistingMesh);
				componentSnapped = true;
				return;
			}
			else return;
		}
	}
	else if (UGrabbableSkelMesh* grabbableSkelMesh = Cast<UGrabbableSkelMesh>(OtherComp))
	{
		if (overlappingGrabbableSkel || (snappingTag != "NULL" && !grabbableSkelMesh->ComponentHasTag(snappingTag))) return;
		newOverlap = grabbableSkelMesh->handRef || (overlappingGrabbableSkel && grabbableSkelMesh == overlappingGrabbableSkel);
		compType = EPreviewMeshSetup::GrabbableSkelMesh;
	}
	// Otherwise exit out of this function.
	else return;

	// If there is a new overlap apply changes to the current overlapping component.
	if (newOverlap)
	{
		full = true;

		// If in returning interpolation mode attach back to the snapping box.
		if (previewComponent && interpMode == EInterpMode::Returning) previewComponent->AttachToComponent(snapBox, FAttachmentTransformRules::KeepWorldTransform);

		// Re-init the preview mesh and if preview mesh was successfully created interpolate to the center of the snap box + offset. Also Bind to release function while overlapping.
		UPrimitiveComponent* meshToSetUp = nullptr;
		switch (compType)
		{
		case EPreviewMeshSetup::GrabbableActor:
			overlappingGrabbable = (AGrabbableActor*)OtherActor;
			if (!overlappingGrabbable->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbable->OnMeshReleased.AddDynamic(this, &ASnappingActor::OnGrabbableRealeased);
			if (snapMode != ESnappingMode::Slidable && snapMode != ESnappingMode::Twistable) SetupPreviewMesh(overlappingGrabbable->grabbableMesh, EPreviewMeshSetup::GrabbableActor);
			break;
		case EPreviewMeshSetup::GrabbableSkelMesh:
			overlappingGrabbableSkel = (UGrabbableSkelMesh*)OtherComp;
			if (!overlappingGrabbableSkel->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbableSkel->OnMeshReleased.AddDynamic(this, &ASnappingActor::OnGrabbableRealeased);
			SetupPreviewMesh(overlappingGrabbableSkel, EPreviewMeshSetup::GrabbableSkelMesh);
			break;
		}

		// Depending on the snap mode hide the grabbed mesh in the hand.
		switch (snapMode)
		{
		case ESnappingMode::Instant:
		{
			interpMode = EInterpMode::Disabled;
			FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
			FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
			previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
			switch (compType)
			{
			case EPreviewMeshSetup::GrabbableActor:
				overlappingGrabbable->grabbableMesh->SetVisibility(false, true);
				break;
			case EPreviewMeshSetup::GrabbableSkelMesh:
				overlappingGrabbableSkel->SetVisibility(false, true);
				break;
			}
		}
		break;
		case ESnappingMode::Interpolate:
		{
			StartInterpolation(EInterpMode::Interpolate);
			switch (compType)
			{
			case EPreviewMeshSetup::GrabbableActor:
				overlappingGrabbable->grabbableMesh->SetVisibility(false, true);
				break;
			case EPreviewMeshSetup::GrabbableSkelMesh:
				overlappingGrabbableSkel->SetVisibility(false, true);
				break;
			}
		}
		break;
		case ESnappingMode::PhysicsOnRelease:
		case ESnappingMode::InstantOnRelease:
		case ESnappingMode::InterpolateOnRelease:
		{
			// Interpolate to the center of the snapBox + the location and rotation offset.
			FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
			FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
			previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
			switch (compType)
			{
			case EPreviewMeshSetup::GrabbableActor:
				if (!overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);
				break;
			case EPreviewMeshSetup::GrabbableSkelMesh:
				if (!overlappingGrabbableSkel->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbableSkel->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);
				break;
			}
		}
		break;
		}

		// Perform snatch if need be after delegates are setup.
		if (snatch)
		{
			switch (compType)
			{
			case EPreviewMeshSetup::GrabbableActor:
				overlappingGrabbable->handRefInfo.handRef->ReleaseGrabbedActor();
				break;
			case EPreviewMeshSetup::GrabbableSkelMesh:
				overlappingGrabbableSkel->handRef->ReleaseGrabbedActor();
				break;
			}
		}
	}
}

void ASnappingActor::OverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// If the current overlapping grabbable is valid and is equal to the actor that ended the overlap with this component lose the reference to said grabbable.
	if (overlappingGrabbable)
	{
		if (overlappingGrabbable->handRefInfo.handRef &&  overlappingGrabbable == OtherActor)
		{
			// Attach to hand to stop movement affecting interpolation.
			if (previewComponent) previewComponent->AttachToComponent(overlappingGrabbable->grabbableMesh, FAttachmentTransformRules::KeepWorldTransform);

			// Unbind to release function when end overlap.
			if (overlappingGrabbable->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbable->OnMeshReleased.RemoveDynamic(this, &ASnappingActor::OnGrabbableRealeased);
			if (snapMode == ESnappingMode::InstantOnRelease || snapMode == ESnappingMode::InterpolateOnRelease || snapMode == ESnappingMode::PhysicsOnRelease)
			{
				ResetPreviewMesh();
				if (overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);
				overlappingGrabbable = nullptr;
				componentSnapped = false;
			}
			// Interpolate the preview mesh back to the hand.
			else StartInterpolation(EInterpMode::Returning);
			full = false;	
			componentSnapped = false;
		}
	}
	// Otherwise repeat the process in the case that it is a grabbable skeletal mesh component.
	else if (overlappingGrabbableSkel)
	{
		if (overlappingGrabbableSkel->handRef && overlappingGrabbableSkel == OtherComp)
		{
			// Attach to hand to stop movement affecting interpolation.
			if (previewComponent) previewComponent->AttachToComponent(overlappingGrabbableSkel, FAttachmentTransformRules::KeepWorldTransform, overlappingGrabbableSkel->boneToSnap);

			// Unbind to release function when end overlap.
			if (overlappingGrabbableSkel->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbableSkel->OnMeshReleased.RemoveDynamic(this, &ASnappingActor::OnGrabbableRealeased);
			if (snapMode == ESnappingMode::InstantOnRelease || snapMode == ESnappingMode::InterpolateOnRelease || snapMode == ESnappingMode::PhysicsOnRelease)
			{
				ResetPreviewMesh();
				if (overlappingGrabbableSkel->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbableSkel->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);
				overlappingGrabbableSkel = nullptr;
			}
			// Interpolate the preview mesh back to the hand.
			else StartInterpolation(EInterpMode::Returning);
			full = false;		
		}
	}
}

void ASnappingActor::OnGrabbablePressed(AVRHand* hand, UPrimitiveComponent* compPressed)
{
	if (snapMode == ESnappingMode::Slidable || snapMode == ESnappingMode::Twistable)
	{
		// Ensure component is snapped.
		if (overlappingGrabbable)
		{
			// Disable the grab function for the grabbable.
			overlappingGrabbable->cancelGrab = true;
			// Fix for highlighting not disabling after grabbing.
			IHandsInterface::Execute_EndOverlapping(overlappingGrabbable, hand);

			// Grab either the sliding mesh or twisting mesh depending on which is selected to use in the snap mode.
			if (slidingMesh) hand->ForceGrab(slidingMesh);
			else if (twistingMesh) hand->ForceGrab(twistingMesh);
		}
		return;
	}
	else
	{
		// Show preview mesh if grabbed and not showing.
		bool snap = false;
		if (overlappingGrabbable)
		{
			snap = true;
			SetupPreviewMesh(overlappingGrabbable->grabbableMesh, EPreviewMeshSetup::GrabbableActor);
		}
		else if (overlappingGrabbableSkel)
		{
			snap = true;
			SetupPreviewMesh(overlappingGrabbableSkel, EPreviewMeshSetup::GrabbableSkelMesh);
		}

		// Destroy physics handle if in snap mode.
		if (snapMode == ESnappingMode::PhysicsOnRelease) DestroyPhysicsHandle();

		// Of component is found set it up correctly.
		if (snap)
		{
			previewComponent->AttachToComponent(snapBox, FAttachmentTransformRules::KeepWorldTransform);
			FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
			FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
			previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
		}

		// Broadcast to snapped disconnection delegate.
		OnSnapDisconnect.Broadcast(compPressed);
	}
}

void ASnappingActor::OnGrabbableRealeased(AVRHand* hand, UPrimitiveComponent* compReleased)
{
	// Reset preview mesh as the component has been released.
	ResetPreviewMesh();

	// Get snapping location/rotation.
	FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
	FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;

	// Perform releasing snapping functionality on the current released component.
	if (compReleased)
	{
		// Depending on snap mode, snap the overlapping grabbable to the snapBox location + offset.
		switch (snapMode)
		{
		case ESnappingMode::PhysicsOnRelease:
		{
			// If attaching a physics handle get the correct component setup.
			EPreviewMeshSetup compSetup = EPreviewMeshSetup::GrabbableActor;
			if (overlappingGrabbableSkel) compSetup = EPreviewMeshSetup::GrabbableSkelMesh;
			interpMode = EInterpMode::Disabled;
			previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
			CreateAndAttatchPhysicsHandle(compReleased, compSetup);
		}
		break;
		case ESnappingMode::Interpolate:
		case ESnappingMode::Instant:
			compReleased->SetVisibility(true, true);
		case ESnappingMode::InstantOnRelease:
		{
			// Disable any physics on grabbable skel.
			compReleased->SetSimulatePhysics(false);
			FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
			FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
			compReleased->SetWorldLocationAndRotation(targetLocation, targetRotation);
		}
		break;
		case ESnappingMode::InterpolateOnRelease:
		{
			// Disable any physics on grabbable skel.
			compReleased->SetSimulatePhysics(false);
			StartInterpolation(EInterpMode::InterpolateOverlapping);
		}
		break;
		}
	}

	// Broadcast to snapped connection delegate.
	OnSnapConnect.Broadcast(compReleased, targetLocation, targetRotation);
}

void ASnappingActor::StartInterpolation(EInterpMode mode)
{
	// Start interpolation.
	interpMode = mode;
	interpolationStartTime = GetWorld()->GetTimeSeconds();

	// Get type of component to interpolate.
	if (overlappingGrabbable) compSetup = EPreviewMeshSetup::GrabbableActor;
	else if (overlappingGrabbableSkel) compSetup = EPreviewMeshSetup::GrabbableSkelMesh;
	else
	{
		interpMode = EInterpMode::Disabled;
		return;
	}

	// Change the comp to interpolate if its the overlapping comp to be interpolated.
	componentToInterpolate = previewComponent;
	if (interpMode == EInterpMode::InterpolateOverlapping)
	{
		// Get the correct mesh to interpolate.
		switch (compSetup)
		{
		case EPreviewMeshSetup::GrabbableActor:
			componentToInterpolate = overlappingGrabbable->grabbableMesh;
			break;
		case EPreviewMeshSetup::GrabbableSkelMesh:
			componentToInterpolate = overlappingGrabbableSkel;
			break;
		}
	}

	// Get start location to interp from.
	interpStartTransform = componentToInterpolate->GetComponentTransform();
}

void ASnappingActor::Interpolate(float deltaTime)
{
	if (previewComponent)
	{	
		// Get the target location and rotation to interpolate to from the current interpolation mode.
		switch (interpMode)
		{
		case EInterpMode::InterpolateOverlapping:
		case EInterpMode::Interpolate:
		{
			// Interpolate to the center of the snapBox + the location and rotation offset.
			lerpLocation = snapBox->GetComponentLocation() + locationOffset;
			lerpRotation = snapBox->GetComponentRotation() + rotationOffset;
		}
		break;
		case EInterpMode::Returning:
		{
			if (overlappingGrabbable || overlappingGrabbableSkel)
			{
				switch (compSetup)
				{
					// If grabbable actor interpolate to current grabbed offset from the hand.
				case EPreviewMeshSetup::GrabbableActor:
					lerpLocation = overlappingGrabbable->grabbableMesh->GetComponentLocation();
					lerpRotation = overlappingGrabbable->grabbableMesh->GetComponentRotation();
					break;
					// Otherwise if grabbable skeletal mesh interpolate to the current grabbed bone.
				case EPreviewMeshSetup::GrabbableSkelMesh:
					lerpLocation = overlappingGrabbableSkel->GetComponentLocation();
					lerpRotation = overlappingGrabbableSkel->GetComponentRotation();
					break;
				}
			}
			else return;
		}
		break;
		}

		// Lerp to target position/rotation/scale.
		float lerpProgress = GetWorld()->GetTimeSeconds() - interpolationStartTime;
		float alpha = FMath::Clamp(lerpProgress / timeToInterp, 0.0f, 1.0f);
		FVector lerpingLocation = FMath::Lerp(interpStartTransform.GetLocation(), lerpLocation, alpha);
		FRotator lerpingRotation = FMath::Lerp(interpStartTransform.GetRotation().Rotator(), lerpRotation, alpha);
		componentToInterpolate->SetWorldLocationAndRotation(lerpingLocation, lerpingRotation);

		// When the component has lerped into snapping position or the hand.
		if (alpha >= 1)
		{
			if (interpMode == EInterpMode::Returning)
			{
				// Once finished returning reset the preview mesh and nullify the overlapping grabbable skel after setting them back to visible in-case hidden.
				if (snapMode == ESnappingMode::Interpolate || snapMode == ESnappingMode::Instant)
				{
					switch (compSetup)
					{
					case EPreviewMeshSetup::GrabbableActor:
						overlappingGrabbable->grabbableMesh->SetVisibility(true, true);
						break;
					case EPreviewMeshSetup::GrabbableSkelMesh:
						overlappingGrabbableSkel->SetVisibility(true, true);
						break;
					}
				}
				overlappingGrabbable = nullptr;
				overlappingGrabbableSkel = nullptr;
				ResetPreviewMesh();
			}

			// Disable the interp mode.
			interpMode = EInterpMode::Disabled;
		}
	}
	else
	{
		// If the preview component is null, log warning message.
		UE_LOG(LogSnappingActor, Warning, TEXT("Snapping Component %s, has not previewComponent to interpolate."), *GetName());
		interpMode = EInterpMode::Disabled;
	}
}

bool ASnappingActor::SetupPreviewMesh(UPrimitiveComponent* comp, EPreviewMeshSetup setupType)
{
	// Reset the preview mesh if there is one.
	ResetPreviewMesh();

	// Cannot spawn preview actor when this component is full.
	if (comp)
	{
		// Setup the previewMesh.
		switch (setupType)
		{
		case EPreviewMeshSetup::GrabbableActor:
		{
			// Copy static mesh component.
			UStaticMeshComponent* newStaticMesh = NewObject<UStaticMeshComponent>(this, TEXT("PreviewMesh"));
			newStaticMesh->SetMobility(EComponentMobility::Movable);
			newStaticMesh->RegisterComponent();

			UStaticMeshComponent* staticMeshComp = Cast<UStaticMeshComponent>(comp);
			newStaticMesh->SetStaticMesh(staticMeshComp->GetStaticMesh());
			newStaticMesh->SetWorldScale3D(staticMeshComp->GetComponentScale());
			// If in instant or interpolate snap mode dont change material to preview material, copy it.
			if (snapMode == ESnappingMode::Instant || snapMode == ESnappingMode::Interpolate)
			{
				for (int m = 0; m < newStaticMesh->GetNumMaterials(); m++)
				{
					newStaticMesh->SetMaterial(m, staticMeshComp->GetMaterial(m));
				}
			}
			// Set all materials as the preview material.
			else 
			{
				for (int m = 0; m < newStaticMesh->GetNumMaterials(); m++)
				{
					newStaticMesh->SetMaterial(m, previewMaterial);
				}
			}
	
			previewComponent = newStaticMesh;
		}
		break;
		case EPreviewMeshSetup::GrabbableSkelMesh:
		{
			// Copy Grabbable Skeletal mesh component.
			UGrabbableSkelMesh* newSkelMesh = NewObject<UGrabbableSkelMesh>(this, TEXT("PreviewSkelMesh"));
			newSkelMesh->SetMobility(EComponentMobility::Movable);
			newSkelMesh->RegisterComponent();

			UGrabbableSkelMesh* skelMeshComp = Cast<UGrabbableSkelMesh>(comp);
			if (skelMeshComp->SkeletalMesh) newSkelMesh->SetSkeletalMesh(skelMeshComp->SkeletalMesh);
			newSkelMesh->SetWorldScale3D(skelMeshComp->GetComponentScale());
			if (skelMeshComp->snappedAnimation) newSkelMesh->SetAnimation(skelMeshComp->snappedAnimation); // Apply snapped animation.
			// If in instant or interpolate snap mode dont change material to preview material, copy it.
			if (snapMode == ESnappingMode::Instant || snapMode == ESnappingMode::Interpolate)
			{
				for (int m = 0; m < newSkelMesh->GetNumMaterials(); m++)
				{
					newSkelMesh->SetMaterial(m, skelMeshComp->GetMaterial(m));		
				}
			}
			// Set all materials as the preview material.
			else
			{
				for (int m = 0; m < newSkelMesh->GetNumMaterials(); m++)
				{
					newSkelMesh->SetMaterial(m, previewMaterial);
				}
			}

			previewComponent = newSkelMesh;
		}
		break;
		}

		// Check preview component was spawned.
		if (!previewComponent)
		{
			UE_LOG(LogSnappingActor, Log, TEXT("SetupPreviewMesh has failed to prodice previewComponent in the snapping component %s."), *GetName());
			return false;
		}

		// Move component to duplicate location, disable any collision and set the preview material.
 		previewComponent->SetWorldTransform(comp->GetComponentTransform());
		previewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Loop through and add children from comp to the preview mesh.
		TArray<USceneComponent*> childrenComponents;
		childrenComponents.Add(comp);
		comp->GetChildrenComponents(true, childrenComponents);
		for (int i = 0; i < childrenComponents.Num(); i++)
		{
			// If the child component is a type of mesh duplicate it and attach it to the preview with the preview material.
			UMeshComponent* typeOfMesh = Cast<UMeshComponent>(childrenComponents[i]);
			if (typeOfMesh)
			{
				FName uniqueCompName = MakeUniqueObjectName(previewComponent, typeOfMesh->GetClass(), FName("preview"));
				UMeshComponent* coppiedComp = DuplicateObject(typeOfMesh, previewComponent, uniqueCompName);
				coppiedComp->RegisterComponent();
				if (previewComponent) coppiedComp->AttachToComponent(previewComponent, FAttachmentTransformRules::KeepWorldTransform);
				else return false;

				// Remove all collision and change material to preview material.
				coppiedComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				
				// If in instant or interpolate snap mode dont change material to preview material, copy it.
				if (snapMode != ESnappingMode::Instant && snapMode != ESnappingMode::Interpolate)
				{
					for (int m = 0; m < coppiedComp->GetNumMaterials(); m++)
					{
						coppiedComp->SetMaterial(m, previewMaterial);
					}
				}
			}
		}
		return true;
	}
	else
	{
		UE_LOG(LogSnappingActor, Log, TEXT("Function SpawnGrabbableActor: The snapping component, %s is full or comp is null."), *GetName());
		return false;
	}
}

void ASnappingActor::ResetPreviewMesh()
{
	if (previewComponent)
	{
		// Destroy any child components.
		TArray<USceneComponent*> childrenComponents;
		previewComponent->GetChildrenComponents(true, childrenComponents);
		for (USceneComponent* child : childrenComponents)
		{
			child->DestroyComponent();
		}

		// Destroy main component.
		previewComponent->DestroyComponent();
	}

	// Reset other variables.
	interpMode = EInterpMode::Disabled;
	interpolationStartTime = 0.0f;
}

void ASnappingActor::CreateAndAttatchPhysicsHandle(UPrimitiveComponent* compToAttatch, EPreviewMeshSetup setupType)
{
	UVRPhysicsHandleComponent* newHandle = NewObject<UVRPhysicsHandleComponent>(this, UVRPhysicsHandleComponent::StaticClass());
	newHandle->RegisterComponent();
	if (newHandle)
	{
		switch (setupType)
		{
		case EPreviewMeshSetup::GrabbableActor:
		{
			AGrabbableActor* grabbable = Cast<AGrabbableActor>(compToAttatch->GetOwner());
			if (grabbable)
			{
				physicsHandleSettings.softLinearConstraint = true;
				grabbable->grabbableMesh->SetSimulatePhysics(true);
				newHandle->CreateJointAndFollowLocationWithRotation(grabbable->grabbableMesh, snapBox, NAME_None, grabbable->grabbableMesh->GetComponentLocation(),
													grabbable->grabbableMesh->GetComponentRotation() + rotationOffset, physicsHandleSettings);
				newHandle->grabOffset = false;
				currentHandle = newHandle;
			}
			else newHandle->DestroyComponent();
		}
		break;
		case EPreviewMeshSetup::GrabbableSkelMesh:
		{
			UGrabbableSkelMesh* grabbable = Cast<UGrabbableSkelMesh>(compToAttatch);
			if (grabbable)
			{
				FName bone = grabbable->boneToSnap;
				physicsHandleSettings.softLinearConstraint = false;
				grabbable->SetSimulatePhysics(true);
				newHandle->CreateJointAndFollowLocationWithRotation(grabbable, snapBox, bone, grabbable->GetBoneLocation(bone),
													grabbable->GetBoneQuaternion(bone).Rotator() + rotationOffset, physicsHandleSettings);
				newHandle->grabOffset = false;
				currentHandle = newHandle;
				updateAnim = grabbable->bUpdateJointsFromAnimation;
				if (updateAnim) grabbable->bUpdateJointsFromAnimation = false;
			}
			else newHandle->DestroyComponent();
		}
		break;
		}
	}
	else UE_LOG(LogSnappingActor, Warning, TEXT("Snapping actor %s, could not create a physics handle..."));
}

void ASnappingActor::DestroyPhysicsHandle()
{
	if (currentHandle)
	{
		if (overlappingGrabbableSkel && updateAnim) overlappingGrabbableSkel->bUpdateJointsFromAnimation = true;
		currentHandle->DestroyJoint();
		currentHandle->DestroyComponent();
		currentHandle = nullptr;
		updateAnim = false;
	}
}

void ASnappingActor::ForceSnap(AActor* snappingActor)
{
	// If the actor is a grabbable actor snap into position.
	if (AGrabbableActor* isGrabbableActor = Cast<AGrabbableActor>(snappingActor))
	{
		// If the mode is set to slidable then set up a slidingMesh instead.
		if (snapMode == ESnappingMode::Slidable)
		{
			// Set new overlapping grabbable.
			overlappingGrabbable = isGrabbableActor;

			// Snap grabbable to the end of the sliding mesh.
			overlappingGrabbable->grabbableMesh->SetSimulatePhysics(false);
			overlappingGrabbable->AttachToComponent(slidingMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			overlappingGrabbable->grabbableMesh->SetRelativeLocationAndRotation(locationOffset, rotationOffset);
			FVector insertedLoc = slidingMesh->originalRelativeTransform.GetLocation();
			switch (slidingMesh->currentAxis)
			{
			case ESlideAxis::X:
				insertedLoc.X = slidableOptions.slidingLimit;
				break;
			case ESlideAxis::Y:
				insertedLoc.Y = slidableOptions.slidingLimit;
				break;
			case ESlideAxis::Z:
				insertedLoc.Z = slidableOptions.slidingLimit;
				break;
			}
			slidingMesh->SetRelativeLocation(insertedLoc);

			// Bind to on mesh grabbed so the user cannot grab the grabbable actor, they can only grab the slidingMesh instead through the snapped grabbable actor.
			if (!overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);

			// Call snapped delegate.
			OnSnapConnect.Broadcast(isGrabbableActor->grabbableMesh, FVector::ZeroVector, FRotator::ZeroRotator);
			OnSnapConnectGrabbable.Broadcast(overlappingGrabbable);
			full = true;
			componentSnapped = true;
			checkSlidable = true;
			return;
		}

		// Otherwise snap actor into default snap Modes.
		if (overlappingGrabbable || (snappingTag != "NULL" && !isGrabbableActor->ActorHasTag(snappingTag))) return;
		overlappingGrabbable = isGrabbableActor;
		OnGrabbableRealeased(nullptr, overlappingGrabbable->grabbableMesh);
		if (!overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);
		if (!overlappingGrabbable->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbable->OnMeshReleased.AddDynamic(this, &ASnappingActor::OnGrabbableRealeased);
	}
	// Otherwise look for grabbable skeletal mesh to snap within actor.
	else if (UActorComponent* foundGrabbableSkel = snappingActor->GetComponentByClass(UGrabbableSkelMesh::StaticClass()))
	{
		if (overlappingGrabbableSkel || (snappingTag != "NULL" && !foundGrabbableSkel->ComponentHasTag(snappingTag))) return;
		UGrabbableSkelMesh* isGrabbableSkel = (UGrabbableSkelMesh*)foundGrabbableSkel;
		overlappingGrabbableSkel = isGrabbableSkel;
		OnGrabbableRealeased(nullptr, overlappingGrabbableSkel);
		if (!overlappingGrabbableSkel->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbableSkel->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);
		if (!overlappingGrabbableSkel->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbableSkel->OnMeshReleased.AddDynamic(this, &ASnappingActor::OnGrabbableRealeased);
	}
	else return;

	// Now full.
	full = true;
}

void ASnappingActor::ForceRelease()
{
	// Remove delegates and destroy references.
	if (overlappingGrabbable)
	{
		if (overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);
		if (overlappingGrabbable->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbable->OnMeshReleased.RemoveDynamic(this, &ASnappingActor::OnGrabbableRealeased);
		OnSnapDisconnect.Broadcast(overlappingGrabbable->grabbableMesh);
		overlappingGrabbable = nullptr;
	}
	else if (overlappingGrabbableSkel)
	{
		if (overlappingGrabbableSkel->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbableSkel->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);
		if (overlappingGrabbableSkel->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbableSkel->OnMeshReleased.RemoveDynamic(this, &ASnappingActor::OnGrabbableRealeased);
		OnSnapDisconnect.Broadcast(overlappingGrabbableSkel);
		overlappingGrabbableSkel = nullptr;
	}

	// Empty.
	full = false;
}

void ASnappingActor::ReleaseSlidable()
{
	if (slidingMesh && overlappingGrabbable)
	{
		// Remove delegate before re-grabbed.
		if (overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);
		overlappingGrabbable->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		overlappingGrabbable->grabbableMesh->SetSimulatePhysics(true);
		
		// Remove old variable values.
		returnSlidableToHand = false;
		componentSnapped = false;
		overlappingGrabbable = nullptr;
	}
}

void ASnappingActor::InitSlidingComponent()
{
	// Spawn and setup the sliding component with the slidableOptions struct.
	FName uniqueCompName = MakeUniqueObjectName(previewComponent, USlidableStaticMesh::StaticClass(), FName("slidingComp"));
	slidingMesh = NewObject<USlidableStaticMesh>(this, USlidableStaticMesh::StaticClass());
	slidingMesh->AttachToComponent(snapBox, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	slidingMesh->slideLimit = slidableOptions.slidingLimit;
	slidingMesh->currentAxis = slidableOptions.axisToSlide;
	slidingMesh->RegisterComponent();	

	// Bind to the on released delegate if slidable options lerp to limit on release is enabled.
	slidingMesh->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnSlidableGrabbed);

	// Bind to the on released delegate if slidable options lerp to limit on release is enabled.
	slidingMesh->OnMeshReleased.AddDynamic(this, &ASnappingActor::OnSlidableReleased);
}

void ASnappingActor::InitTwistingComponent()
{
	// Spawn and setup the twistable rotating component with the slidableOptions struct.
	FName uniqueCompName = MakeUniqueObjectName(previewComponent, URotatableStaticMesh::StaticClass(), FName("twistingComp"));
	twistingMesh = NewObject<URotatableStaticMesh>(this, URotatableStaticMesh::StaticClass());
	twistingMesh->AttachToComponent(snapBox, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	twistingMesh->rotateMode = EStaticRotation::Twist;
	twistingMesh->fakePhysics = false;
	twistingMesh->rotationLimit = twistableOptions.keyUnlockAmount;
	if (twistableOptions.lockOnAmount)
	{
		twistingMesh->lockable = true;
		twistingMesh->grabWhileLocked = false;
		twistingMesh->interpolateToLock = false;
		twistingMesh->lockingDistance = 2.0f;
		twistingMesh->unlockingDistance = 3.0f;
		twistingMesh->lockingPoints.Add(twistableOptions.keyUnlockAmount);
		twistingMesh->lockSound = twistableOptions.keyUnlockSound;
		twistingMesh->lockHapticEffect = twistableOptions.keyUnlockHaptics;

		// Bind to locked delegate function.
		twistingMesh->OnRotatableLock.AddDynamic(this, &ASnappingActor::OnTwistableLocked);
	}
	twistingMesh->RegisterComponent();
}

void ASnappingActor::OnTwistableLocked(float lockedAngle, URotatableStaticMesh* rotatable)
{
	// Disable the grabbing ability on the grabbable acting as a twistable when snapped then locked into position.
	if (overlappingGrabbable) overlappingGrabbable->interactableSettings.canInteract = false;
}

void ASnappingActor::OnSlidableGrabbed(AVRHand* hand)
{
	if (overlappingGrabbable)
	{
		// Broadcast to snapped disconnection delegate.
		OnSnapDisconnect.Broadcast(overlappingGrabbable->grabbableMesh);
	}
}

void ASnappingActor::OnSlidableReleased(AVRHand* hand)
{
 	// If the slidable should lerp to the snapped position on release.
 	if (!returnSlidableToHand && overlappingGrabbable)
 	{
		// Disable the lerp to the hand.
		checkSlidable = false;

 		// Get the position to lerp into.
 		relativeSlidingLerpPos = slidingMesh->originalRelativeTransform.GetLocation();
 		switch (slidingMesh->currentAxis)
 		{
 		case ESlideAxis::X:
 			relativeSlidingLerpPos.X = slidableOptions.slidingLimit;
 		break;
 		case ESlideAxis::Y:
 			relativeSlidingLerpPos.Y = slidableOptions.slidingLimit;
 		break;
 		case ESlideAxis::Z:
 			relativeSlidingLerpPos.Z = slidableOptions.slidingLimit;
 		break;
 		}
 
 		// Start the lerp.
 		lerpSlidableToLimit = true;
 		interpolationStartTime = GetWorld()->GetTimeSeconds();
		slidingStartLoc = slidingMesh->RelativeLocation;
		handRegrab = hand;

		// Call delegate.
		if (slidingMesh->currentPosition == slidableOptions.slidingLimit)
		{
			OnSnapConnectGrabbable.Broadcast(overlappingGrabbable);
			return;
		}
 	}
}