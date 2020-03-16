// Fill out your copyright notice in the Description page of Project Settings.

#include "GrabbableSkelMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/BoxComponent.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Player/VRPawn.h"
#include "Player/VRHand.h"
#include "VR/VRFunctionLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "VR/EffectsContainer.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY(LogGrabbableSkelComp);

UGrabbableSkelMesh::UGrabbableSkelMesh()
{
	// Setup grabbable collision properties.
	SetCollisionProfileName(FName("Grabbable"));
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SetUseCCD(true);
	bMultiBodyOverlap = true;
	SetGenerateOverlapEvents(true);
	SetNotifyRigidBodyCollision(true);

	// Defaults.
	handRef = nullptr;
	otherHandRef = nullptr;
	boneToGrab = NAME_None;
	snapToHandRotationOffset = FRotator::ZeroRotator;
	grabbed = false;
	grabFromClosestBone = false;
	softHandle = false;
	checkCollision = true;
	centerPhysicsJoint = true;
	adjustInertiaFromArray = true;
	timeToLerp = 0.4f;
	lerping = false;
	hapticIntensityMultiplier = 1.0f;

#if DEVELOPMENT
	debug = false;
#endif

	// Initialise default interface variables.
	interactableSettings.grabHandleData.handleDataEnabled = true;
	interactableSettings.grabHandleData.softAngularConstraint = false;
	interactableSettings.grabHandleData.softLinearConstraint = false;
	interactableSettings.grabHandleData.interpolate = false;
	interactableSettings.grabHandleData.interpSpeed = 10.0f;
	interactableSettings.handMinRumbleDistance = 10.0f;
	interactableSettings.releaseDistance = 30.0f;
}

void UGrabbableSkelMesh::BeginPlay()
{
	Super::BeginPlay();

	// Disable hand distance if collision is disabled to stop comp being released on collision.
	if (!checkCollision) interactableSettings.canRelease = false;

	// Decide weather grab closest bone is enabled or disabled.
	if (boneToGrab == NAME_None) grabFromClosestBone = true;

	// Setup default impact haptic effect and audio.
	if (impactSoundOverride) impactSound = impactSoundOverride;
	else if (AVRPawn* pawn = Cast<AVRPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
	{
		if (USoundBase* soundToUse = pawn->effectsContainer->GetAudioEffect("DefaultCollision")) impactSound = soundToUse;
	}
	else UE_LOG(LogGrabbableSkelComp, Log, TEXT("The grabbable skeletal component %s, cannot find impact audio from override or the pawns effects container."), *GetName());

	// Get haptic effect to play on collisions.
	if (collisionFeedbackOverride) collisionFeedback = collisionFeedbackOverride;
	else if (AVRPawn* pawn = Cast<AVRPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
	{
		if (UHapticFeedbackEffect_Base* feedbackEffect = pawn->effectsContainer->GetFeedbackEffect("DefaultCollision")) collisionFeedback = feedbackEffect;
	}
	else UE_LOG(LogGrabbableSkelComp, Log, TEXT("The grabbable skeletal component %s, cannot find haptic effect from override or the pawns effects container."), *GetName());

	// Enable hit events and bind to this classes function.
	SetNotifyRigidBodyCollision(true);
	BodyInstance.SetInstanceNotifyRBCollision(true);
	if  (!OnComponentHit.Contains(this, "OnHit")) this->OnComponentHit.AddDynamic(this, &UGrabbableSkelMesh::OnHit);
}

void UGrabbableSkelMesh::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!lerping && OtherComp)
	{
		// Prevents any hitting components that are either balanced on the grabbable or the grabbable balanced on the hand from calling impact sounds and haptic effects.
		if (UPrimitiveComponent* hittingComp = Cast<UPrimitiveComponent>(OtherComp))
		{
			if (FMath::IsNearlyEqual(hittingComp->GetPhysicsLinearVelocity().Size(), GetPhysicsLinearVelocity().Size(), 15.0f)) return;
		}

		// Check if the hit actor is a hand, therefor rumble the hand.
		if (AVRHand* hand = Cast<AVRHand>(OtherActor))
		{
			// Calculate intensity and play bot haptic and the sound if they are currently set.
			float rumbleIntesity = FMath::Clamp(hand->handVelocity.Size() / 250.0f, 0.0f, 1.0f);
			if (collisionFeedback) hand->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);
			if (impactSound) hand->PlaySound(impactSound, rumbleIntesity);

			// If a hand is holding this class goto the area in this function that rumbles the holding hand.
			if (handRef) goto RumbleHoldingHand;
		}
		// Play impact sound at location of grabbable if it is hit by something other than an actor.
		else
		{
		RumbleHoldingHand:
			float impulseSize = FMath::Abs(NormalImpulse.Size());
			float currentZ = GetComponentLocation().Z;

			// If the impulse is big enough and the grabbable is not rolling/sliding along the floor.
			if (!FMath::IsNearlyEqual(currentZ, lastZ, 0.1f) && GetPhysicsLinearVelocity().Size() >= 50.0f)
			{
				// Get volume from current intensity and check if the sound should be played.
				float rumbleIntesity = FMath::Clamp(impulseSize / (1200.0f * GetMass()), 0.1f, 1.0f);

				// If the audio has a valid sound to play. Make sure the sound was not played within the past 0.3 seconds.
				if (rumbleIntesity > lastRumbleIntensity && lastImpactSoundTime <= GetWorld()->GetTimeSeconds() + 0.3f)
				{
					// If sound is initialised play it.
					if (impactSound)
					{
						// Don't play again for set amount of time.
						lastImpactSoundTime = GetWorld()->GetTimeSeconds();
						lastRumbleIntensity = rumbleIntesity;

						// Play sound effect.
						UGameplayStatics::PlaySoundAtLocation(GetWorld(), impactSound, GetComponentLocation(), rumbleIntesity);

						// Set timer to set lastRumbleIntensity back to 0.0f once the sound has played.
						FTimerDelegate timerDel;
						timerDel.BindUFunction(this, TEXT("ResetLastRumbleIntensity"));
						GetWorld()->GetTimerManager().ClearTimer(lastRumbleHandle);
						GetWorld()->GetTimerManager().SetTimer(lastRumbleHandle, timerDel, impactSound->GetDuration(), true);
					}
				}
			}
		}
	}

	// Save the last hit time.
	lastHitTime = GetWorld()->GetTimeSeconds();
	// Save last Z. NOTE: Bug fix, stops the impact sound from triggering when the ball is rolling,
	lastZ = GetComponentLocation().Z;
}

void UGrabbableSkelMesh::ResetLastRumbleIntensity()
{
	lastRumbleIntensity = 0.0f;
}

bool UGrabbableSkelMesh::GetRecentlyHit()
{
	return GetWorld()->GetTimeSeconds() - lastHitTime <= 0.2f;
}

void UGrabbableSkelMesh::PickupPhysicsHandle(AVRHand* hand)
{
	// Setup the closest body to the controller, but if the boneToGrab had a name preset grab that bone instead.
	// OR the second hand is grabbing this component.
	FName boneName = NAME_None;
	if (grabFromClosestBone)
	{
		if (otherHandRef && otherHandRef == hand)
		{
			otherBoneToGrab = UpdateComponentsClosestBody(hand);
			boneName = otherBoneToGrab;
		}
		else
		{
			boneToGrab = UpdateComponentsClosestBody(hand);
			boneName = boneToGrab;
		}
	}

	// Ensure physics is enabled.
	SetSimulatePhysics(true);

	// Create joint between hand and physics object and enable physics to handle any collisions.
	FVector locationToGrab;
	FRotator rotationToGrab;
	if (centerPhysicsJoint)
	{
		locationToGrab = GetBoneLocation(boneName);
		rotationToGrab = GetBoneQuaternion(boneName).Rotator();
	}
	else
	{
		locationToGrab = hand->grabCollider->GetComponentLocation();
		rotationToGrab = hand->grabCollider->GetComponentRotation();
	}

	// Grab the mesh using the hands physics handle.
	hand->grabHandle->CreateJointAndFollowLocationWithRotation(this, hand->grabCollider, boneName, locationToGrab, rotationToGrab, interactableSettings.grabHandleData);
}

void UGrabbableSkelMesh::DropPhysicsHandle(AVRHand* hand)
{
	// Destroy the joint created by the physics handle.
	hand->grabHandle->DestroyJoint();

	// If body instance was changed reset.
	if (softHandle) ToggleSoftPhysicsHandle(false);
}

FName UGrabbableSkelMesh::UpdateComponentsClosestBody(AVRHand* hand)
{
	// Find the closest bone and set it as the bone to grab.
	FClosestPointOnPhysicsAsset closest;
	bool foundBone = GetClosestPointOnPhysicsAsset(hand->grabCollider->GetComponentLocation(), closest, false);
	if (foundBone) return closest.BoneName;
	else return NAME_None;
}

bool UGrabbableSkelMesh::IsMeshGrabbed()
{
	return grabbed;
}

FTransform UGrabbableSkelMesh::GetGrabbedTransform()
{
	return FTransform(worldRotationOffset, worldPickupOffset, FVector(1.0f));
}

void UGrabbableSkelMesh::LerpingBack(float deltaTime)
{
	// Get the current target location of the joint from the current grabbed bones world position.
	FTransform currentTargetTransform;
	if (centerPhysicsJoint) currentTargetTransform = GetBoneTransform(GetBoneIndex(boneToGrab));
	else
	{
		FTransform boneTransform = GetBodyInstance(boneToGrab)->GetUnrealWorldTransform();
		currentTargetTransform.SetLocation(boneTransform.TransformPositionNoScale(originalBoneOffset.GetLocation()));
		currentTargetTransform.SetRotation(boneTransform.TransformRotation(originalBoneOffset.GetRotation()));
	}

	// Lerp the transform from currentTargetTransform to the endTargetTransform and apply the location to the grabHandle (Physics handle grabbing this comp). 
	float lerpProgress = GetWorld()->GetTimeSeconds() - lerpStartTime;
	float alpha = FMath::Clamp(lerpProgress / timeToLerp, 0.0f, 1.0f);
	FTransform lerpedTransform = UVRFunctionLibrary::LerpT(currentTargetTransform, handRef->grabHandle->GetTargetLocation(), alpha);
	handRef->grabHandle->SetTarget(lerpedTransform, true);

	// Once lerp is complete reset the joint values to track the hand movement again.
	if (alpha >= 1) ToggleLerping(false);

#if DEVELOPMENT
	// Print if currently lerping.
	if (debug) UE_LOG(LogGrabbableSkelComp, Log, TEXT("The grabbable skeletal mesh %s, is lerping back to the hand %s."), *GetName(), *handRef->GetName());
#endif
}

void UGrabbableSkelMesh::ToggleLerping(bool on)
{
	if (on)
	{
		// Setup handle to allow custom target location for lerping back to the hand.
		FPhysicsHandleData newData = handRef->grabHandle->handleData;
		newData.updateTargetLocation = false;
		handRef->grabHandle->UpdateJointValues(newData);

		// Setup to lerp back to the hand. (Check and ran in dragging function...)
		lerping = true;
		lerpStartTime = GetWorld()->GetTimeSeconds();

		// Set the joints target location to start at the bone locations joint location currently.
		FTransform newTargetTransform;
		if (centerPhysicsJoint) newTargetTransform = GetBoneTransform(GetBoneIndex(boneToGrab));
		else
		{
			FTransform boneTransform = GetBodyInstance(boneToGrab)->GetUnrealWorldTransform();
			newTargetTransform.SetLocation(boneTransform.TransformPositionNoScale(originalBoneOffset.GetLocation()));
			newTargetTransform.SetRotation(boneTransform.TransformRotation(originalBoneOffset.GetRotation()));
		}
		handRef->grabHandle->SetTarget(newTargetTransform, true);
	}
	else
	{
		// Reset anything that might still be enabled.
		FPhysicsHandleData newData = handRef->grabHandle->handleData;
		newData.updateTargetLocation = true;
		handRef->grabHandle->UpdateJointValues(newData);
		lerping = false;
	}
}

void UGrabbableSkelMesh::ToggleSoftPhysicsHandle(bool on)
{
	// Ensure that the hand isn't null.
	if (handRef)
	{
		// Enable/Disable soft linear constraint...
		if (on)
		{
			// Play sound and haptic effects.
			float rumbleIntesity = FMath::Clamp(handRef->handVelocity.Size() / 250.0f, 0.0f, 1.0f);
			if (collisionFeedback)
			{
				handRef->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);

				// Also check if two handed grabbing mode is enabled and rumble both hands in this case.
				if (interactableSettings.twoHandedGrabbing && otherHandRef)
				{
					otherHandRef->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);
				}
			}

			// Play sound effect.
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), impactSound, GetComponentLocation(), rumbleIntesity);

			// Make current bone act like larger bone, so it effects parent bones correctly when grabbed and in soft constraint mode.
			if (adjustInertiaFromArray)
			{
				FBodyInstance* bodyInst = GetBodyInstance(boneToGrab);
				originalIntertia = bodyInst->InertiaTensorScale;
				float intertiaMultiplier = 2.2f;
				bodyInst->InertiaTensorScale = originalIntertia * intertiaMultiplier;
				bodyInst->UpdateMassProperties();
			}
		}
		else
		{
			// Reset bone inertia/mass properties if enabled.
			if (adjustInertiaFromArray)
			{
				GetBodyInstance(boneToGrab)->InertiaTensorScale = originalIntertia;
				GetBodyInstance(boneToGrab)->UpdateMassProperties();
			}
		}

		// Update current soft handle value.
		handRef->grabHandle->ToggleDrive(on, on);
		if (interactableSettings.twoHandedGrabbing && otherHandRef) otherHandRef->grabHandle->ToggleDrive(on, on);
		softHandle = on;	
	}
}

void UGrabbableSkelMesh::GrabPressed_Implementation(AVRHand* hand)
{
	// Already grabbed and two handed mode is enabled.
	if (handRef && interactableSettings.twoHandedGrabbing)
	{
		// Save new hand and grab with physics handle.
		otherHandRef = hand;

		// Broadcast to delegate.
		OnMeshGrabbed.Broadcast(otherHandRef, this);
	}
	else
	{
		// Setup the hand reference.
		handRef = hand;
		grabbed = true;

		// Broadcast to delegate.
		OnMeshGrabbed.Broadcast(handRef, this);

		// If we dont have a pointer to the hand we have already been released by a delegate
		if (!handRef) return;

		// Save original offsets for checking again overlaps at current grabbed location and lerping back to the hand.
		if (checkCollision)
		{
			// Setup ignored actors for use in draggingImplementation.
			ignored.Add(GetOwner());
			ignored.Add(handRef);

			// Get the original relative offset for the rotation and location when this is grabbed when compared to the hand scene component.
			FTransform targetTransform = handRef->grabCollider->GetComponentTransform();
			FTransform boneTransform = GetBodyInstance(boneToGrab)->GetUnrealWorldTransform();
			originalRelativePickupOffset = targetTransform.InverseTransformPositionNoScale(boneTransform.GetLocation());
			originalRelativePickupRotation = targetTransform.InverseTransformRotation(boneTransform.GetRotation()).Rotator();

			// Save offset bone so less calculations have to be done in the lerp function.
			originalBoneOffset.SetLocation(boneTransform.InverseTransformPositionNoScale(targetTransform.GetLocation()));
			originalBoneOffset.SetRotation(boneTransform.InverseTransformRotation(targetTransform.GetRotation()));
		}
	}

	// Grab with the correct method.
	PickupPhysicsHandle(hand);

	// NOTE: Temporary bug fix. After grabbed un-highlight everything. 
	TArray<USceneComponent*> components;
	GetChildrenComponents(true, components);
	components.Add(this);
	for (USceneComponent* comp : components)
	{
		if (UPrimitiveComponent* isPrimitive = Cast<UPrimitiveComponent>(comp))
		{
			if (isPrimitive->bRenderCustomDepth)
			{
				// Reset stencil value to 0 which has no post process material.
				isPrimitive->SetCustomDepthStencilValue(0);
				isPrimitive->SetRenderCustomDepth(false);
			}
		}
	}
}

void UGrabbableSkelMesh::GrabReleased_Implementation(AVRHand* hand)
{
	// If dropped by a second grabbed hand.
	if (otherHandRef == hand)
	{
		// Reset mesh back to normal.
		DropPhysicsHandle(hand);

		// Broadcast released delegate.
		OnMeshReleased.Broadcast(handRef, this);

		//Reset default values.
		otherHandRef = nullptr;
		ignored.Remove(hand);
		return;
	}

	// Reset mesh back to normal.
	DropPhysicsHandle(hand);

	// Update velocity from hand movement when releasing this component. (BUG FIX)
	SetAllPhysicsLinearVelocity(handRef->handVelocity, false);
	SetAllPhysicsAngularVelocityInDegrees(handRef->handAngularVelocity, false);

	// Broadcast released delegate.
	OnMeshReleased.Broadcast(handRef, this);

	// Reset values to default. Do last.
	handRef = nullptr;
	grabbed = false;
	
	// Reset collision variables if enabled.
	if (checkCollision)
	{
		lerping = false;
		ignored.Empty();
	}
}

void UGrabbableSkelMesh::Dragging_Implementation(float deltaTime)
{
	if (handRef && checkCollision)
	{
		// Find the current pickup and rotation offset for where the grabbable should be.
		FVector grabbedBodyLocation = GetBodyInstance(boneToGrab)->GetUnrealWorldTransform().GetLocation();
		FTransform controllerTransform = handRef->grabCollider->GetComponentTransform();
		worldPickupOffset = controllerTransform.TransformPositionNoScale(originalRelativePickupOffset);
		worldRotationOffset = controllerTransform.TransformRotation(originalRelativePickupRotation.Quaternion()).Rotator();

		// Update the hand grab distance to help hand class decide when to release this component.
		FVector currentRelativePickupOffset = worldPickupOffset - grabbedBodyLocation;
		lastHandGrabDistance = interactableSettings.handDistance;
		interactableSettings.handDistance = currentRelativePickupOffset.Size();

		// If grabbed by two hands don't check for collisions as physics is always enabled.
		if (interactableSettings.twoHandedGrabbing && otherHandRef)
		{
			if (!softHandle) ToggleSoftPhysicsHandle(true);
			return;
		}

		// Update trace variables to check if the bones are too close to other bodies or static objects.
		FHitResult hitResult;
		FBodyInstance* currentGrabbedBody = GetBodyInstance(boneToGrab);
		FVector boneExtent = currentGrabbedBody->GetBodyBounds().GetExtent();
		FTransform boneTransform = currentGrabbedBody->GetUnrealWorldTransform();

		// Show debugging information if in editor and if debug is enabled.
		EDrawDebugTrace::Type trace = EDrawDebugTrace::None;
#if DEVELOPMENT
		if (debug) trace = EDrawDebugTrace::ForOneFrame;
#endif
		// If hit and stiff handle is enabled active soft handle.
		if (UKismetSystemLibrary::BoxTraceSingleByProfile(GetWorld(), boneTransform.GetLocation(), worldPickupOffset, boneExtent, worldRotationOffset, "Grabbable", false, ignored, trace, hitResult, true) || GetRecentlyHit())
		{
			if (!softHandle)
			{
				ToggleSoftPhysicsHandle(true);
				if (lerping) ToggleLerping(false);
#if DEVELOPMENT
				if (debug) UE_LOG(LogTemp, Warning, TEXT("SkeletalGrabbableMesh, %s is now using soft constraint on the physics handle."), *GetName());
#endif
			}
		}
		// Otherwise if soft handle is still active disable it.
		else if (softHandle)
		{
			ToggleSoftPhysicsHandle(false);
			if (!lerping) ToggleLerping(true);
#if DEVELOPMENT
			if (debug) UE_LOG(LogTemp, Warning, TEXT("SkeletalGrabbableMesh, %s is now using interpolating back to the correct grabbed location."), *GetName());
#endif
		}

		// If lerping the handle location do so.
		if (lerping) LerpingBack(deltaTime);
	}
}

void UGrabbableSkelMesh::Overlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::Overlapping_Implementation(hand);
}

void UGrabbableSkelMesh::EndOverlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::EndOverlapping_Implementation(hand);
}

void UGrabbableSkelMesh::Teleported_Implementation()
{
	if (handRef)
	{
		// Release, set new location and grab again.
		handRef->grabHandle->DestroyJoint();

		// Get grabbed bone and controller transform.
		UPrimitiveComponent* targetComponent = handRef->grabCollider;
		FTransform controllerTransform = targetComponent->GetComponentTransform();
		FTransform boneTransform = GetBodyInstance(boneToGrab)->GetUnrealWorldTransform();

		// Get relative bone offset for location and rotation to the component itself in world space.
		FVector relBoneLoc = boneTransform.InverseTransformPositionNoScale(GetComponentLocation());
		FQuat relBoneRot = boneTransform.InverseTransformRotation(GetComponentQuat());

		// Get the world location expected for the grabbed bone.
		FVector newLoc = controllerTransform.TransformPositionNoScale(originalRelativePickupOffset);
		FQuat newRot = controllerTransform.TransformRotation(originalRelativePickupRotation.Quaternion());

		// Get the component location and rotation relative to how the bone should be positioned.
		FTransform newTransform = FTransform(newRot, newLoc, FVector(1));
		newLoc = newTransform.TransformPositionNoScale(relBoneLoc);
		newRot = newTransform.TransformRotation(relBoneRot);

		// Teleport this component to its new transform.
		this->SetWorldLocationAndRotation(newLoc, newRot, false, nullptr, ETeleportType::TeleportPhysics);

		// Re-Initialise the joint at the new location.
		handRef->grabHandle->CreateJointAndFollowLocationWithRotation(this, targetComponent, boneToGrab,
			targetComponent->GetComponentLocation(), targetComponent->GetComponentRotation(), interactableSettings.grabHandleData);
	}
}

FHandInterfaceSettings UGrabbableSkelMesh::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void UGrabbableSkelMesh::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}
