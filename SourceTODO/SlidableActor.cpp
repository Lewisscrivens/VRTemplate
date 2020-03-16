// Fill out your copyright notice in the Description page of Project Settings.

#include "SlidableActor.h"
#include "Player/VRHand.h"
#include "Player/VRPawn.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "CustomComponent/SimpleTimeline.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "GameFramework/PlayerController.h"
#include "VR/EffectsContainer.h"
#include <Kismet/GameplayStatics.h>

DEFINE_LOG_CATEGORY(LogSlidableActor);

ASlidableActor::ASlidableActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork; // Avoids visual glitches when resetting position of physics pivot when out of bounds.

	// Initialise the root.
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	root->SetMobility(EComponentMobility::Movable);
	RootComponent = root;

	// Initialise the physics pivot for handling linear constrained physics.
	pivot = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Pivot"));
	pivot->SetupAttachment(root);
	pivot->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	pivot->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	pivot->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	pivot->ConstraintInstance.ProfileInstance.LinearLimit.bSoftConstraint = false;// Fix for pivot breakage under force.

	// Initialise Slidables audio component.
	slidableAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("SlidableAudio"));
	slidableAudio->SetupAttachment(slidingMesh);
	slidableAudio->bAutoActivate = false;

	// Initialise the direction pointer.
	slidableXDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("slidableXDirection"));
	slidableXDirection->SetupAttachment(pivot);

	// Initialise variables.
	handRef = nullptr;
	targetComponent = nullptr;
	currentSlidableMode = ESlidableMode::GrabStaticCollision;
	boneToGrab = NAME_None;
	compToGrab = NAME_None;
	componentToGrab = nullptr;
	simulatePhysics = true;
	limitedToRange = true;
	friction = 0.5f;
	restitution = 0.2f;
	sliderLimit = FVector::ZeroVector;
	currentSliderLimit = FVector::ZeroVector;
	centerConstraint = false;
	hapticIntensity = 1.0f;
	impactSoundIntensity = 1.5f;

#if DEVELOPMENT
	debug = false;
#endif

	// Initialise interface variables.
	interactableSettings.releaseDistance = 25.0f;
	interactableSettings.handMinRumbleDistance = 15.0f;
}

void ASlidableActor::BeginPlay()
{
	Super::BeginPlay();

	// Set up audio for this class.
	if (slidingSound) slidableAudio->SetSound(slidingSound);
	if (!impactSound || !impactHapticEffect)
	{
		if (APlayerController* playerController = GetWorld()->GetFirstPlayerController())
		{
			if (AVRPawn* player = Cast<AVRPawn>(playerController->GetPawn()))
			{
				if (!impactSound) impactSound = player->effectsContainer->GetAudioEffect("DefaultCollision");
				if (!impactHapticEffect) impactHapticEffect = player->effectsContainer->GetFeedbackEffect("DefaultCollision");
			}
		}
	}

	// By default the component to grab is the sliding mesh, although if there is a compToGrab use that instead.
	componentToGrab = slidingMesh;
	if (currentSlidableMode == ESlidableMode::GrabPhysics)
	{
		for (UActorComponent* comp : GetComponents().Array())
		{
			if (comp->GetFName() == compToGrab) componentToGrab = Cast<UPrimitiveComponent>(comp);
		}
	}

	// Set-up ignored components...
	for (AActor* actor : ignoredActors)
	{
		slidingMesh->IgnoreActorWhenMoving(actor, true);
	}

	// Set all child primitive components to constrainedComponents to prevent collision errors.
	TArray<UActorComponent*> childComponents = this->GetComponents().Array();
	for (UActorComponent* comp : childComponents)
	{
		UPrimitiveComponent* primComp = Cast<UPrimitiveComponent>(comp);
		if (primComp) primComp->SetCollisionObjectType(ECollisionChannel::ECC_GameTraceChannel10);
	}

	// Create a copy of the original actors transform.
	originalTransform = GetActorTransform();
	FTransform originalRelativeTransform = slidingMesh->GetRelativeTransform();
	sliderRelativePosition = pivot->GetComponentTransform().InverseTransformPositionNoScale(slidingMesh->GetComponentLocation());

	// Check if this slidable is limited to a range.
	xLimited = sliderLimit.X != 0;
	yLimited = sliderLimit.Y != 0;
	zLimited = sliderLimit.Z != 0;
	if (!xLimited && !yLimited && !zLimited) limitedToRange = false;

	// Update the current limits for both axis.
	if (xLimited) UpdateLimit(sliderLimit.X, currentSliderLimit.X);
	if (yLimited) UpdateLimit(sliderLimit.Y, currentSliderLimit.Y);
	if (zLimited) UpdateLimit(sliderLimit.Z, currentSliderLimit.Z);

	// If this Slidable simulates physics set up the physics pivot.
	if (simulatePhysics)
	{
		CheckConstraintBounds();
		SetupConstraint();
	}
}

void ASlidableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update the current Slidable values if grabbed or physically moving.
	if (simulatePhysics && (slidingMesh->GetPhysicsLinearVelocity().Size() > 0 || handRef))
	{
		// Keep track of relative location.
		sliderRelativePosition = pivot->GetComponentTransform().InverseTransformPositionNoScale(slidingMesh->GetComponentLocation());
		if (activeAxis > 1) CheckConstraintBounds();

		// Keep track of positional velocity.
		currentPosition = slidingMesh->GetComponentLocation();
		currentVelocity = (currentPosition - lastPosition) / DeltaTime;
		lastPosition = currentPosition;

		// Update audio and haptic effects.
		UpdateAudioAndHaptics();
	}

#if DEVELOPMENT
	ShowConstraintBounds();
#endif
}

#if DEVELOPMENT
void ASlidableActor::ShowConstraintBounds()
{
	// By default treat the slidingMesh bounds and show the constraint.
	if (debug)
	{
		FBox currBounds;
		if (UStaticMeshComponent* isStaticMesh = Cast<UStaticMeshComponent>(slidingMesh))
		{
			if (isStaticMesh->GetStaticMesh()) currBounds = isStaticMesh->GetStaticMesh()->GetBoundingBox();
			else
			{
				UE_LOG(LogSlidableActor, Warning, TEXT("The Slidable Actor %s, cannot find a static mesh set. Destroying this object..."), *GetName());
				Destroy(); 
				return;
			}
		}
		else if (USkeletalMeshComponent* isSkelMesh = Cast<USkeletalMeshComponent>(slidingMesh)) currBounds = isSkelMesh->GetBodyInstance(boneToGrab)->GetBodyBounds();
		FVector meshExtent = currBounds.GetExtent() * slidingMesh->GetComponentScale();
		FVector extent = (currentSliderLimit / 2) + meshExtent;
		FVector debugLocation = pivot->GetComponentTransform().TransformPositionNoScale(-refferenceOffset);
		DrawDebugBox(GetWorld(), debugLocation, extent, pivot->GetComponentRotation().Quaternion(), FColor::Blue, false, 0.1f, 0.0f, 2.0f);
	}
}
#endif

void ASlidableActor::UpdateLimit(float originalLimit, float& posLimit)
{
	// Calculate the limit for the current values.
	if (originalLimit < 0) posLimit = FMath::Abs(originalLimit);
	else posLimit = originalLimit;
	activeAxis++;
}

float ASlidableActor::GetRefferenceOffset(float axis)
{
	// Get reference offset for axis.
	float pos = 0.0f;
	float posAxis = FMath::Abs(axis);
	if (axis < 0) pos = posAxis / 2.0f;
	else pos = posAxis / -2.0f;
	return pos;
}

void ASlidableActor::SetupConstraint()
{
	// Setup the pivot.
	slidingMesh->SetSimulatePhysics(true);
	pivot->SetConstrainedComponents(nullptr, NAME_None, slidingMesh, NAME_None);

	if (limitedToRange)
	{
		// Get the biggest limit and apply it to the pivot, get the outside limit needs for square movement if both axis are active.
		float limitToUse = currentSliderLimit.GetMax();
		if (activeAxis > 1)
		{
			// c^2 = a^2 + b^2
			float squaredMaxLimit = FMath::Square(limitToUse);
			limitToUse = FMath::Sqrt(squaredMaxLimit + squaredMaxLimit);// Ensure the limit is big enough to encompass the constraints per axis limit...
			if (zLimited) limitToUse *= 2; // Bug Fix...
		}

		// Constrain the specified axis. Use half the limit and the reference position will be offset accounting for this.
		if (xLimited) pivot->SetLinearXLimit(ELinearConstraintMotion::LCM_Limited, limitToUse / 2);
		if (yLimited) pivot->SetLinearYLimit(ELinearConstraintMotion::LCM_Limited, limitToUse / 2);
		if (zLimited) pivot->SetLinearZLimit(ELinearConstraintMotion::LCM_Limited, limitToUse / 2);

		// Offset the pivot to the correct location given the axis that are constrained, if this option is desired within the current Slidable.
		FVector newReffOffset;
		if (!centerConstraint)
		{
			float xPos = 0.0f;
			float yPos = 0.0f;
			float zPos = 0.0f;
			if (xLimited) xPos = GetRefferenceOffset(sliderLimit.X);
			if (yLimited) yPos = GetRefferenceOffset(sliderLimit.Y);
			if (zLimited) zPos = GetRefferenceOffset(sliderLimit.Z);
			refferenceOffset = FVector(xPos, yPos, zPos);

			// Get reference offset world location from constraint pivot.
			constraintOffset = pivot->GetComponentTransform().TransformPositionNoScale(refferenceOffset);
			// Get relative location from the sliding mesh to the pivots reference offset. 
			// NOTE: Take away the current offset of the sliding mesh to position the constraint correctly. (So the slidingMesh can be rotated and offset from the starting position.)
			newReffOffset = slidingMesh->GetComponentTransform().InverseTransformPositionNoScale(constraintOffset) - slidingMesh->GetComponentTransform().InverseTransformPositionNoScale(pivot->GetComponentLocation());
		}
		// If centered to the mesh position the reference location of the joint at the pivot components location.
		else newReffOffset = slidingMesh->GetComponentTransform().InverseTransformPositionNoScale(pivot->GetComponentLocation());
		
		// Apply found pivot reference position.
		pivot->SetConstraintReferencePosition(EConstraintFrame::Frame2, newReffOffset);

		// Setup Friction. (Target 0 velocity on all axis with the strength "friction")
		pivot->SetLinearVelocityDrive(true, true, true);
		pivot->SetLinearDriveParams(0.0f, friction, 0.0f);			
	}
	// If there is no limit print debug message.
	else UE_LOG(LogSlidableActor, Warning, TEXT("The slidable skeletal slidingMesh actor: %s, is not currently active due to no limit being set."), *GetFullName(this));
}

void ASlidableActor::InRange(bool& inRangeXPointer, bool& inRangeYPointer, bool& inRangeZPointer)
{
	// Check if the slidableMesh is currently in range.
	if (centerConstraint)
	{
		inRangeXPointer = sliderRelativePosition.X >= -currentSliderLimit.X / 2 && sliderRelativePosition.X <= currentSliderLimit.X / 2;
		inRangeYPointer = sliderRelativePosition.Y >= -currentSliderLimit.Y / 2 && sliderRelativePosition.Y <= currentSliderLimit.Y / 2;
		inRangeZPointer = sliderRelativePosition.Z >= -currentSliderLimit.Z / 2 && sliderRelativePosition.Z <= currentSliderLimit.Z / 2;
	}
	else
	{
		if (sliderLimit.X < 0) inRangeXPointer = sliderRelativePosition.X >= -currentSliderLimit.X && sliderRelativePosition.X <= 0;
		else inRangeXPointer = sliderRelativePosition.X <= currentSliderLimit.X && sliderRelativePosition.X >= 0;
		if (sliderLimit.Y < 0) inRangeYPointer = sliderRelativePosition.Y >= -currentSliderLimit.Y && sliderRelativePosition.Y <= 0;
		else inRangeYPointer = sliderRelativePosition.Y <= currentSliderLimit.Y && sliderRelativePosition.Y >= 0;
		if (sliderLimit.Z < 0) inRangeZPointer = sliderRelativePosition.Z >= -currentSliderLimit.Z && sliderRelativePosition.Z <= 0;
		else inRangeZPointer = sliderRelativePosition.Z <= currentSliderLimit.Z && sliderRelativePosition.Z >= 0;
	}
}

void ASlidableActor::CheckConstraintBounds()
{
	// Range checks for pivot. Max && Min.
	bool inRangeX, inRangeY, inRangeZ;
	InRange(inRangeX, inRangeY, inRangeZ);

	// If either x or y is not within the constrained range reset position and apply any velocity. 
	// TAKE NOTE: If Z axis is not in range it will always be falling out of range due to gravity.
	if (!inRangeX || !inRangeY || !inRangeZ)
	{
		sliderRelativePosition = ClampPosition(sliderRelativePosition); // Get the closest position within the pivot from the current relative position.
		FVector closestClampedPos = pivot->GetComponentTransform().TransformPositionNoScale(sliderRelativePosition); // Using transform to take rotation into account.
		FVector currentPhysicsVel = slidingMesh->GetPhysicsLinearVelocity();

		// Update world location. Reset physics, thats handled later in this function.
		slidingMesh->SetWorldLocation(closestClampedPos, false, nullptr, ETeleportType::TeleportPhysics);

		// Handle physics after resetting the position. See if it needs to bounce off the walls of the pivot.
		if (!inRangeX) currentPhysicsVel.X = (currentPhysicsVel.X * -1) * restitution;
		if (!inRangeY) currentPhysicsVel.Y = (currentPhysicsVel.Y * -1) * restitution;
		if (!inRangeZ) currentPhysicsVel.Z = currentPhysicsVel.Z * restitution; // NOTE: FIX FOR ANGLE SLIDING

		// Update physics.
		slidingMesh->SetPhysicsLinearVelocity(currentPhysicsVel);
	}
}

FVector ASlidableActor::ClampPosition(FVector position)
{
	// Clamp current location of the slidingMesh to the closest location within the constraint.
	FVector clampedPosition;
	if (centerConstraint)
	{
		clampedPosition.X = FMath::Clamp(position.X, -currentSliderLimit.X / 2, currentSliderLimit.X / 2);
		clampedPosition.Y = FMath::Clamp(position.Y, -currentSliderLimit.Y / 2, currentSliderLimit.Y / 2);
		clampedPosition.Z = FMath::Clamp(position.Z, -currentSliderLimit.Z / 2, currentSliderLimit.Z / 2);
	}
	else
	{
		if (sliderLimit.X < 0) clampedPosition.X = FMath::Clamp(position.X, -currentSliderLimit.X, 0.0f);
		else clampedPosition.X = FMath::Clamp(position.X, 0.0f, currentSliderLimit.X);
		if (sliderLimit.Y < 0) clampedPosition.Y = FMath::Clamp(position.Y, -currentSliderLimit.Y, 0.0f);
		else clampedPosition.Y = FMath::Clamp(position.Y, 0.0f, currentSliderLimit.Y);
		if (sliderLimit.Z < 0) clampedPosition.Z = FMath::Clamp(position.Z, -currentSliderLimit.Z, 0.0f);
		else clampedPosition.Z = FMath::Clamp(position.Z, 0.0f, currentSliderLimit.Z);
	}
	return clampedPosition;
}

void ASlidableActor::UpdateSlidable(float DeltaTime)
{
	// Not using transform position as it breaks the pivot offset due to rotation of the hand being taken into account...
	FVector currentWorldOffset = targetComponent->GetComponentLocation() - originalGrabOffset;

	// If not using the physics handle...
	if (currentSlidableMode != ESlidableMode::GrabPhysics)
	{
		// Get the relative offset.
		FVector currentRelativeOffset = pivot->GetComponentTransform().InverseTransformPositionNoScale(currentWorldOffset);

		// Apply grabbed location.
		FVector clampedGrabbedPosition = ClampPosition(currentRelativeOffset);
		FVector currentGrabbedRelatvePosition = pivot->GetComponentTransform().TransformPositionNoScale(clampedGrabbedPosition);
		FHitResult* hitResult = new FHitResult();
		if (currentSlidableMode == ESlidableMode::GrabStaticCollision) slidingMesh->SetWorldLocation(currentGrabbedRelatvePosition, true, hitResult, ETeleportType::TeleportPhysics);
		else slidingMesh->SetWorldLocation(currentGrabbedRelatvePosition, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// Update the hand grab distance for handling when to release the actor etc.
	interactableSettings.handDistance = (currentWorldOffset - slidingMesh->GetComponentLocation()).Size();
}

void ASlidableActor::UpdateAudioAndHaptics()
{
	// Play haptic effect if grabbed.
	FVector pos = sliderRelativePosition;
	float velocitySize = FMath::Abs(currentVelocity.Size());
	if (handRef && slidingHapticEffect)
	{
		if (!FMath::IsNearlyEqual(lastHapticFeedbackPosition.Size(), pos.Size(), hapticSlideDelay))
		{
			lastHapticFeedbackPosition = pos;
			float intensity = FMath::Clamp(velocitySize / 200.0f, 0.0f, 2.0f);
			handRef->PlayFeedback(slidingHapticEffect, intensity * hapticIntensity);
		}
	}

	// Check if the slidable has recently hit the constraint limits.
	bool atConstrainedLimit = false;
	if (centerConstraint)
	{
		bool xAtLimit = FMath::IsNearlyEqual(pos.X, -currentSliderLimit.X / 2, 0.5f) || FMath::IsNearlyEqual(pos.X, currentSliderLimit.X / 2, 0.5f);
		bool yAtLimit = FMath::IsNearlyEqual(pos.Y, -currentSliderLimit.Y / 2, 0.5f) || FMath::IsNearlyEqual(pos.Y, currentSliderLimit.Y / 2, 0.5f);
		bool zAtLimit = FMath::IsNearlyEqual(pos.Z, -currentSliderLimit.Z / 2, 0.5f) || FMath::IsNearlyEqual(pos.Z, currentSliderLimit.Z / 2, 0.5f);
		if ((xLimited && xAtLimit) || (yLimited && yAtLimit) || (zLimited && zAtLimit)) atConstrainedLimit = true;
	}
	else
	{
		bool xAtLimit = (sliderLimit.X < 0 ? FMath::IsNearlyEqual(pos.X, -currentSliderLimit.X, 0.5f) : FMath::IsNearlyEqual(pos.X, currentSliderLimit.X, 0.5f)) || FMath::IsNearlyEqual(pos.X, 0.0f, 0.5f);
		bool yAtLimit = (sliderLimit.Y < 0 ? FMath::IsNearlyEqual(pos.Y, -currentSliderLimit.Y, 0.5f) : FMath::IsNearlyEqual(pos.Y, currentSliderLimit.Y, 0.5f)) || FMath::IsNearlyEqual(pos.Y, 0.0f, 0.5f);
		bool zAtLimit = (sliderLimit.Z < 0 ? FMath::IsNearlyEqual(pos.Z, -currentSliderLimit.Z, 0.5f) : FMath::IsNearlyEqual(pos.Z, currentSliderLimit.Z, 0.5f)) || FMath::IsNearlyEqual(pos.Z, 0.0f, 0.5f);
		if ((xLimited && xAtLimit) || (yLimited && yAtLimit) || (zLimited && zAtLimit)) atConstrainedLimit = true;
	}

	// If at a constrained limit play impact sound and haptic effect if grabbed.
	if (atConstrainedLimit)
	{
		// If angular velocity change is high enough.
		if (velocitySize > 5.0f && imapctSoundEnabled)
		{
			// Calculate intensity of effects.
			float intensity = FMath::Clamp(velocitySize / 200.0f, 0.0f, 1.0f);

			// If grabbed play haptic effect.
			if (handRef && impactHapticEffect) handRef->PlayFeedback(impactHapticEffect, intensity * hapticIntensity);

			// Play audio if set and not playing the impact sound currently.
			if (impactSound)
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), impactSound, slidableAudio->GetComponentLocation(), intensity * impactSoundIntensity);
				imapctSoundEnabled = false;
			}
		}
	}
	// Otherwise if not at the constraint start and impact audio is playing
	else if (!imapctSoundEnabled) imapctSoundEnabled = true;

	// Play sound effect if sounds are set. Set volume of sound from positional change over time...
	if (slidingSound)
	{
		// Get volume to play dragging/friction audio from the current  velocity. The higher the louder the sound.
		float volume = FMath::Clamp(velocitySize / 200.0f, 0.0f, 1.0f);
		float interpolatedVolume = FMath::FInterpTo(slidableAudio->VolumeMultiplier, volume * 2.0f, GetWorld()->GetDeltaSeconds(), 10.0f);

		// If the slider audio is playing update the volume from current velocity.
		if (slidableAudio->IsPlaying()) slidableAudio->SetVolumeMultiplier(interpolatedVolume);
		// Otherwise if audio is not playing start playing the audio.
		else
		{
			slidableAudio->SetVolumeMultiplier(volume);
			slidableAudio->Play();
		}
	}
}


void ASlidableActor::GrabPressed_Implementation(AVRHand* hand)
{
	// Save hand ref for use while grabbed.
	handRef = hand;
	targetComponent = handRef->grabCollider;

	// Grab the Slidable with the correct mode...
	switch (currentSlidableMode)
	{
	case ESlidableMode::GrabPhysics:

		// Grab the Slidable with the physics handle.
		handRef->grabHandle->CreateJointAndFollowLocation(componentToGrab, targetComponent, boneToGrab, targetComponent->GetComponentLocation(), interactableSettings.grabHandleData);
		break;
	case ESlidableMode::GrabStatic:
	case ESlidableMode::GrabStaticCollision:

		// Disable physics on this constrained actor...
		if (simulatePhysics) slidingMesh->SetSimulatePhysics(false);
		break;
	}

	// Call slidingMesh grabbed delegate for use in blueprints.
	OnMeshGrabbed.Broadcast(handRef);

	// Get the grab offset from the pivots root position.
	originalGrabOffset = targetComponent->GetComponentLocation() - slidingMesh->GetComponentLocation();
}

void ASlidableActor::GrabReleased_Implementation(AVRHand* hand)
{
	// Call slidingMesh released delegate for use in blueprints.
	OnMeshReleased.Broadcast(handRef);

	// Release the Slidable with the correct mode...
	switch (currentSlidableMode)
	{
	case ESlidableMode::GrabPhysics:

		// Grab the slidable with the physics handle.
		handRef->grabHandle->DestroyJoint();
		handRef = nullptr;
		break;
	case ESlidableMode::GrabStatic:
	case ESlidableMode::GrabStaticCollision:

		// Re-enable physics when released and re-attach to the pivot...
		if (simulatePhysics)
		{
			SetupConstraint();
			slidingMesh->SetAllPhysicsLinearVelocity(handRef->handVelocity, false);
			slidingMesh->SetAllPhysicsAngularVelocityInDegrees(handRef->handAngularVelocity, false);
		}
		handRef = nullptr;
		break;
	}
}

void ASlidableActor::Dragging_Implementation(float deltaTime)
{
	if (handRef) UpdateSlidable(deltaTime);
}

void ASlidableActor::Overlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::Overlapping_Implementation(hand);
}

void ASlidableActor::EndOverlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::EndOverlapping_Implementation(hand);
}

void ASlidableActor::GrabbedWhileLocked_Implementation()
{
	if (handRef) OnGrabbedWhileLocked.Broadcast();
}

void ASlidableActor::ReleasedWhileLocked_Implementation()
{
	if (handRef) OnReleasedWhileLocked.Broadcast();
}

FHandInterfaceSettings ASlidableActor::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void ASlidableActor::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}
