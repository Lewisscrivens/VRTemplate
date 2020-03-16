// Fill out your copyright notice in the Description page of Project Settings.

#include "GrabbableActor.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/BoxComponent.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "ConstructorHelpers.h"
#include "Engine/World.h"
#include "Player/VRPawn.h"
#include "Player/VRHand.h"
#include "DrawDebugHelpers.h"
#include "CustomComponent/SimpleTimeline.h"
#include "Kismet/GameplayStatics.h"
#include "VR/VRFunctionLibrary.h"
#include "VR/EffectsContainer.h"
#include <Sound/SoundBase.h>
#include <Components/AudioComponent.h>
#include "Components/ChildActorComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "TimerManager.h"
#include "CustomComponent/ChildActorComponentV2.h"
#include "Actors/Workbench/Hittable.h"

DEFINE_LOG_CATEGORY(LogGrabbable);

AGrabbableActor::AGrabbableActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// The grabbable mesh root component. Default Setup.
	grabbableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	grabbableMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	grabbableMesh->SetCollisionObjectType(ECollisionChannel::ECC_GameTraceChannel6);
	grabbableMesh->SetCollisionResponseToChannel(ECC_Scanner, ECollisionResponse::ECR_Overlap);
	grabbableMesh->SetUseCCD(true);
	grabbableMesh->SetSimulatePhysics(true);
	grabbableMesh->SetGenerateOverlapEvents(true);
	grabbableMesh->ComponentTags.Add(FName("Grabbable"));
	RootComponent = grabbableMesh;

	// Setup audio component for playing effects when hitting other objects.
	grabbableAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("GrabbableAudio"));
	grabbableAudio->SetupAttachment(grabbableMesh);
	grabbableAudio->bAutoActivate = false;

	// Initialise default variables.
	handRefInfo = FGrabInformation();
	otherHandRefInfo = FGrabInformation();
	physicsMaterialWhileGrabbed = UGlobals::GetPhysicalMaterial(PM_NoFriction);

	physicsMaterialWhileGrabbedEnabled = true;
	physicsAttatched = false;
	attatched = false;
	snapToHand = false;
	lerping = false;
	changeMassOnGrab = false;
	considerMassWhenThrown = false;
	hittable = false;

	sweepAccuracy = 1.0f;
	timeToLerp = 0.5f;
	massWhenGrabbed = 0.5f;
	hapticIntensityMultiplier = 1.0f;

	collisionFeedbackOverride = nullptr;
	impactSoundOverride = nullptr;

	secondHandRotationOffset = FRotator(0.0f, 90.0f, 0.0f);
	snapToHandRotationOffset = FRotator::ZeroRotator;
	snapToHandLocationOffset = FVector::ZeroVector;

	grabMode = EGrabMode::AttatchToWithPhysics;
	secondHandGrabMode = ESecondGrabMode::PhysicsHandle;
	lerpMode = EReturnMode::ClearPathToHand;
	collisionType = EOverlapType::Complex;
	
#if DEVELOPMENT
	debug = false;
#endif

	// Initialise default interface settings.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.handMinRumbleDistance = 10.0f;
}

FGrabbableSaveData AGrabbableActor::GenerateSaveData()
{
	FGrabbableSaveData save;

	save.actorClass = GetClass();
	save.transform = GetActorTransform();
	save.velocity = grabbableMesh->GetPhysicsLinearVelocity();
	save.angularVelocity = grabbableMesh->GetPhysicsAngularVelocityInRadians();

	return save;
}

void AGrabbableActor::LoadSaveData(FGrabbableSaveData data)
{
	grabbableMesh->SetPhysicsLinearVelocity(data.velocity);
	grabbableMesh->SetPhysicsAngularVelocityInRadians(data.angularVelocity);
}

EScanType AGrabbableActor::GetScanType_Implementation()
{
	return EScanType::Wiki;
}

FString AGrabbableActor::GetWikiLink_Implementation()
{
	return FString("Grabbable Actor");
}

void AGrabbableActor::BeginPlay()
{
	Super::BeginPlay();

	// Setup sounds for impacts.
	if (impactSoundOverride)
	{
		impactSound = impactSoundOverride;
		grabbableAudio->SetSound(impactSoundOverride);
	}
	else if (AVRPawn* pawn = Cast<AVRPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
	{
		if (USoundBase* soundToUse = pawn->effectsContainer->GetAudioEffect("DefaultCollision"))
		{
			impactSound = soundToUse;
			grabbableAudio->SetSound(soundToUse);
		}
	}
	else UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, cannot find impact audio from override or the pawns effects container."), *GetName());

	// Get haptic effect to play on collisions.
	if (collisionFeedbackOverride) collisionFeedback = collisionFeedbackOverride;
	else if (AVRPawn* pawn = Cast<AVRPawn>(GetWorld()->GetFirstPlayerController()->GetPawn()))
	{
		if (UHapticFeedbackEffect_Base* feedbackEffect = pawn->effectsContainer->GetFeedbackEffect("DefaultCollision")) collisionFeedback = feedbackEffect;
	}
	else UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, cannot find haptic effect from override or the pawns effects container."), *GetName());

	// Setup the on hit delegate to call haptic feed back on the hand. Only if there is an impact sound or haptic feedback effect set for this grabbable.
	if (collisionFeedback || impactSound || hittable)
	{
		grabbableMesh->SetNotifyRigidBodyCollision(true);
		if (!OnActorHit.IsBound()) OnActorHit.AddDynamic(this, &AGrabbableActor::OnHit);
	}

	// Ensure stabilization and velocity count is correct for all grabbables.
	grabbableMesh->BodyInstance.PositionSolverIterationCount = 15.0f;
	grabbableMesh->BodyInstance.VelocitySolverIterationCount = 5.0f;

	// Create array of each mesh that should be checked while grabbed for overlap events.
	FTimerDelegate addIgnored;
	addIgnored.BindLambda([&]() 
	{
		ignoredActors.Add(this);
		TArray<UActorComponent*> foundComponents;
		foundComponents = this->GetComponents().Array();
		for (UActorComponent* comp : foundComponents)
		{
			// Convert and add each foundMesh to collidableMeshes array as a primitive component.
			if (UMeshComponent* isMeshComp = Cast<UMeshComponent>(comp)) collidableMeshes.Add((UPrimitiveComponent*)isMeshComp);
			// Otherwise check if there are any child actors that need to be checked or ignored...
			else if (UChildActorComponent* isChildActor = Cast<UChildActorComponent>(comp)) ignoredActors.Add(isChildActor->GetChildActor());
			// Otherwise check the other custom child actor component class.
			else if (UChildActorComponentV2* isChildActorV2 = Cast<UChildActorComponentV2>(comp)) ignoredActors.Add(isChildActorV2->GetChildActor());
		}
	});
	GetWorldTimerManager().SetTimerForNextTick(addIgnored);
}

#if WITH_EDITOR
bool AGrabbableActor::CanEditChange(const UProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	// Lerp mode, collision type and time to lerp cannot be edited unless grab mode is equal to AttatchToWithPhysics.
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AGrabbableActor, lerpMode) || 
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AGrabbableActor, collisionType) || 
		InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AGrabbableActor, timeToLerp))
	{
		return grabMode == EGrabMode::AttatchToWithPhysics;
	}
	// Only allow physics material reference to be edited while enabled.
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AGrabbableActor, physicsMaterialWhileGrabbed))
	{
		return physicsMaterialWhileGrabbedEnabled;
	}
	// Only allow second hand grabbing mode to be modified when two handed grabbing is enabled.
	else if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AGrabbableActor, secondHandGrabMode) ||
			 InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(AGrabbableActor, secondHandRotationOffset))
	{
		return interactableSettings.twoHandedGrabbing;
	}
		
	return ParentVal;
}
#endif

void AGrabbableActor::OnHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!lerping && OtherActor)
	{
		// Prevents any hitting components that are either balanced on the grabbable or the grabbable balanced on the hand from calling impact sounds and haptic effects.
		if (UPrimitiveComponent* hittingComp = Cast<UPrimitiveComponent>(Hit.Component))
		{
			if (FMath::IsNearlyEqual(hittingComp->GetPhysicsLinearVelocity().Size(), grabbableMesh->GetPhysicsLinearVelocity().Size(), 15.0f)) return;
		}

		// Check if the hit actor is a hand, therefor rumble the hand.
		if (AVRHand* hand = Cast<AVRHand>(OtherActor))
		{
			// Calculate intensity and play bot haptic and the sound if they are currently set.
			float rumbleIntesity = FMath::Clamp(hand->handVelocity.Size() / 250.0f, 0.0f, 1.0f);
			if (collisionFeedback) hand->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);
			if (impactSound) hand->PlaySound(impactSound, rumbleIntesity);

			// If a hand is holding this class goto the area in this function that rumbles the holding hand.
			if (handRefInfo.handRef) goto RumbleHoldingHand;
		}
		// Play impact sound at location of grabbable if it is hit by something other than an actor.
		else
		{
			RumbleHoldingHand:
			float impulseSize = FMath::Abs(NormalImpulse.Size());
			float currentZ = grabbableMesh->GetComponentLocation().Z;

			// If the impulse is big enough and the grabbable is not rolling/sliding along the floor.
 			if (!FMath::IsNearlyEqual(currentZ, lastZ, 0.1f) && grabbableMesh->GetPhysicsLinearVelocity().Size() >= 50.0f)
 			{	
				// Get volume from current intensity and check if the sound should be played.
				float rumbleIntesity = FMath::Clamp(impulseSize / (1200.0f * grabbableMesh->GetMass()), 0.1f, 1.0f);

				// If the audio has a valid sound to play. Make sure the sound was not played within the past 0.3 seconds.
				if (rumbleIntesity > lastRumbleIntensity && lastImpactSoundTime <= GetWorld()->GetTimeSeconds() + 0.3f)
				{
					// Run the hit interface from this grabbable on impact of something with the hittable interface applied to it.
					if (hittable)
					{
						bool hasInterface = OtherActor->GetClass()->ImplementsInterface(UHittable::StaticClass());

						// Check if the hit actor has a hittable interface and if the impulse size is big enough to call hit.
						if (hasInterface && impulseSize > IHittable::Execute_GetImpulseBase(OtherActor))
						{
							// Run hit event on hittable actor.
							IHittable::Execute_OnHammerHitObj(OtherActor, Hit.Location, NormalImpulse);
						}
					}

					// If sound is initialised play it.
					if (grabbableAudio->Sound)
					{
						// Don't play again for set amount of time.
						lastImpactSoundTime = GetWorld()->GetTimeSeconds();
						lastRumbleIntensity = rumbleIntesity;

						// Adjust the audio and Play sound effect.
						grabbableAudio->SetVolumeMultiplier(rumbleIntesity);
						grabbableAudio->Play();

						// Set timer to set lastRumbleIntensity back to 0.0f once the sound has played.
						FTimerDelegate timerDel;
						timerDel.BindUFunction(this, TEXT("ResetLastRumbleIntensity"));
						GetWorld()->GetTimerManager().ClearTimer(lastRumbleHandle);
						GetWorld()->GetTimerManager().SetTimer(lastRumbleHandle, timerDel, grabbableAudio->Sound->GetDuration(), true);
					}
				}
 			}
		}
	}	
}

void AGrabbableActor::ResetLastRumbleIntensity()
{
	lastRumbleIntensity = 0.0f;
}

void AGrabbableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Save last Z. NOTE: Bug fix, stops the impact sound from triggering when the ball is rolling,
	lastZ = grabbableMesh->GetComponentLocation().Z;
}

void AGrabbableActor::PickupAttatchTo()
{
	// Disable physics and attach component to the controller with the static method.
	grabbableMesh->SetSimulatePhysics(false);
	FAttachmentTransformRules attatchRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, true);
	grabbableMesh->AttachToComponent(handRefInfo.targetComponent, attatchRules);

	// Overlap constrained components to know where to enable physics collision.
	grabbableMesh->SetCollisionResponseToChannel(ECC_ConstrainedComp, ECR_Overlap);

	// Enable lerp and attachment.
	attatched = true;

	// Call any delegates for this event.
	OnCollisionChanged.Broadcast(ECR_Overlap);
	OnPhysicsStateChanged.Broadcast(false);

#if DEVELOPMENT
	if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has been grabbed by AttachTo."), *GetName());
#endif
}

void AGrabbableActor::PickupPhysicsHandle(FGrabInformation grabInfo)
{
	// Play sound and haptic effects.
	float rumbleIntesity = FMath::Clamp(handRefInfo.handRef->handVelocity.Size() / 250.0f, 0.0f, 1.0f);
	if (collisionFeedback)
	{
		handRefInfo.handRef->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);

		// Also check if two handed grabbing mode is enabled and rumble both hands in this case.
		if (interactableSettings.twoHandedGrabbing && otherHandRefInfo.handRef)
		{
			otherHandRefInfo.handRef->PlayFeedback(collisionFeedback, rumbleIntesity * hapticIntensityMultiplier);
		}
	}

	// Adjust the audio and Play sound effect.
	grabbableAudio->SetVolumeMultiplier(rumbleIntesity);
	grabbableAudio->Play();

	// Create joint between hand and physics object and enable physics to handle any collisions.
	FVector locationToGrab = grabInfo.targetComponent->GetComponentLocation();
	FRotator rotationToGrab = grabInfo.targetComponent->GetComponentRotation();

	// Grab. Increase stabilization while grabbed to prevent visual errors or snapping...
	grabInfo.handRef->grabHandle->CreateJointAndFollowLocationWithRotation(grabbableMesh, grabInfo.targetComponent, NAME_None, locationToGrab, rotationToGrab, interactableSettings.grabHandleData);
	grabbableMesh->SetSimulatePhysics(true);

	// Block the constrained components now that physics is enabled and collisions can be handled correctly.
	grabbableMesh->SetCollisionResponseToChannel(ECC_ConstrainedComp, ECR_Block);

	// Enable physics attachments and disable lerping as it shouldn't be enabled when physically attatched.
	physicsAttatched = true;

	// Call any delegates for this event.
	OnCollisionChanged.Broadcast(ECR_Block);
	OnPhysicsStateChanged.Broadcast(true);

#if DEVELOPMENT
	if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has been grabbed by PhysicsHandle."), *GetName());
#endif
}

void AGrabbableActor::DropAttatchTo()
{
 	// Dettatch the component of it is attached to a parent component.
 	if (grabbableMesh->GetAttachParent()) grabbableMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
 	grabbableMesh->SetSimulatePhysics(true);
 
 	// Block any constrained components when dropped and not held.
 	grabbableMesh->SetCollisionResponseToChannel(ECC_ConstrainedComp, ECR_Block);
 
 	// The component is no longer attatched.
 	attatched = false;
 
 	// Call any delegates for this event.
 	OnCollisionChanged.Broadcast(ECR_Block);
 	OnPhysicsStateChanged.Broadcast(true);
 
 #if DEVELOPMENT
 	if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has been dropped by AttachTo."), *GetName());
 #endif
}

void AGrabbableActor::DropPhysicsHandle(FGrabInformation grabInfo)
{
	if (grabInfo.handRef)
	{
		// Destroy the joint created by the physics handle.
		grabInfo.handRef->grabHandle->DestroyJoint();

		// Block the constrained components when dropped.
		grabbableMesh->SetCollisionResponseToChannel(ECC_ConstrainedComp, ECR_Block);

		// Component is no longer physically attached.
		physicsAttatched = false;

		// Call collision delegate.
		OnCollisionChanged.Broadcast(ECR_Block);

#if DEVELOPMENT
		if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has been dropped by PhysicsHandle."), *GetName());
#endif
	}
}

bool AGrabbableActor::GetColliding()
{
	if (collisionType == EOverlapType::Complex)
	{
		// Run the collision check using each component making up the grabbable. (Uses collision of each component for check.)
		for (UPrimitiveComponent* primComp : collidableMeshes)
		{
			// Ensure that the collision is enabled on this component otherwise don't bother to check it.
			if (primComp)
			{
				// Get the desired transform of the current component location using original grab offset etc.
				FTransform handTransform;
				handTransform.SetScale3D(primComp->GetComponentScale());
				if (primComp == grabbableMesh)
				{
					handTransform.SetLocation(handRefInfo.worldPickupOffset);
					handTransform.SetRotation(handRefInfo.worldRotationOffset.Quaternion());
				}
				else
				{
					FVector locationOffsetFromRoot = primComp->GetComponentLocation() - grabbableMesh->GetComponentLocation();
					FQuat rotationOffsetFromRoot = primComp->GetComponentQuat() - grabbableMesh->GetComponentQuat();
					handTransform.SetLocation(handRefInfo.worldPickupOffset + locationOffsetFromRoot);
					handTransform.SetRotation(handRefInfo.worldRotationOffset.Quaternion() + rotationOffsetFromRoot);
				}

				// Check for any overlapping components for current primComp. End function if a collision is found and return that there was an overlap.
				TArray<UPrimitiveComponent*> outHit;
				if (UVRFunctionLibrary::ComponentOverlapComponentsByChannel(primComp, handTransform, grabbableMesh->GetCollisionObjectType(), ignoredActors, outHit, true)) return true;
			}
		}

		// Return false as an overlap was not detected.
		return false;
	}
	else
	{
		FHitResult overlappingBounds;
		FVector halfExtent = CalculateComponentsBoundingBoxInLocalSpace().GetExtent() * GetActorScale();
		FCollisionShape box = FCollisionShape::MakeBox(halfExtent);
		FCollisionQueryParams colParam;
		colParam.AddIgnoredActors(ignoredActors);
		// Return overlap result using the actors bounds.
		return GetWorld()->OverlapAnyTestByChannel(handRefInfo.worldPickupOffset, handRefInfo.worldRotationOffset.Quaternion(), grabbableMesh->GetCollisionObjectType(), box, colParam);
	}
}

bool AGrabbableActor::IsActorGrabbed()
{
	return handRefInfo.handRef != nullptr;
}

bool AGrabbableActor::IsActorGrabbedTwoHanded()
{
	return otherHandRefInfo.handRef != nullptr;
}

void AGrabbableActor::LerpingBack(float deltaTime)
{
	// If its already attatched lerp back to the desired grab offset.
	if (lerping)
	{
		// Calculate the current lerp alpha.
		float lerpProgress = GetWorld()->GetTimeSeconds() - lerpStartTime;
		float alpha = FMath::Clamp(lerpProgress / timeToLerp, 0.0f, 1.0f);

		// Get the current grabbed offset of the mesh and lerp the grabbableMesh component to it.
		FVector newLoc = FMath::Lerp(grabbableMesh->GetComponentLocation(), handRefInfo.worldPickupOffset, alpha);
		FRotator newRot = FMath::Lerp(grabbableMesh->GetComponentRotation(), handRefInfo.worldRotationOffset, alpha);
		grabbableMesh->SetWorldLocationAndRotation(newLoc, newRot);

		// When the alpha has reached 1 or more it has finished lerping.
		if (alpha >= 1)
		{
			lerping = false;

#if DEVELOPMENT
			// If debug is enabled log that the grabbable has finished lerping.
			if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has FINISHED lerping."), *GetName());
#endif
		}
#if DEVELOPMENT
		// Otherwise in editor, Print message while lerping back to the hand.
		else if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, is lerping back to the hand %s. %f%"), *GetName(), *handRefInfo.handRef->GetName(), alpha * 100);
#endif
	}
}

void AGrabbableActor::ToggleLerping(bool on)
{
	if (on) lerpStartTime = GetWorld()->GetTimeSeconds();
	lerping = on;
}

FHitResult AGrabbableActor::SweepActor()
{
	// Do a sweep trace of this entire actor. 
	FHitResult* sweepHit = new FHitResult();
	FTransform oldTransform = GetActorTransform();
	FVector newScale = FVector(oldTransform.GetScale3D().X, oldTransform.GetScale3D().Y, oldTransform.GetScale3D().Z);
	newScale *= sweepAccuracy;
	FTransform testTransform = FTransform(handRefInfo.worldRotationOffset, handRefInfo.worldPickupOffset, newScale);
	SetActorScale3D(newScale);
	SetActorTransform(testTransform, true, sweepHit, ETeleportType::TeleportPhysics); // Sweep to the testTransform position.
	SetActorTransform(oldTransform); // Reset after the sweep.

	// Return result.
	return *sweepHit;
}

FTransform AGrabbableActor::GetGrabbedTransform()
{
	return FTransform(handRefInfo.worldRotationOffset, handRefInfo.worldPickupOffset, FVector(1.0f));
}

void AGrabbableActor::UpdateGrabInformation()
{
	handRefInfo.worldPickupOffset = handRefInfo.targetComponent->GetComponentTransform().TransformPosition(handRefInfo.originalRelativePickupOffset);
	
	// Check if need to track rotation of second hand.
	if (interactableSettings.twoHandedGrabbing && otherHandRefInfo.handRef)
	{
		// Update the rotation to target the second hand and return.
		switch (secondHandGrabMode)
		{
		case ESecondGrabMode::TrackRotation:
			// Calculate the direction rotation for the grabbable towards the second grabbing hand.
			FRotator lookAtHandRot = (handRefInfo.handRef->controller->GetComponentLocation() - otherHandRefInfo.handRef->controller->GetComponentLocation()).Rotation();
			FQuat currentRotationChange = (secondHandOriginalTransform.InverseTransformRotation(lookAtHandRot.Quaternion()).Rotator() + secondHandRotationOffset).Quaternion();
			FRotator currentLookAtRot = secondHandGrabbableRot.TransformRotation(currentRotationChange).Rotator();
			handRefInfo.worldRotationOffset = currentLookAtRot;
		return;
		}
	}

	handRefInfo.worldRotationOffset = handRefInfo.targetComponent->GetComponentTransform().TransformRotation(handRefInfo.originalPickupRelativeRotation.Quaternion()).Rotator();
}

void AGrabbableActor::GrabPressed_Implementation(AVRHand* hand)
{
	// Call delegate for being grabbed.
	OnMeshGrabbed.Broadcast(hand, grabbableMesh);

	// Check the grab has not been cancelled through the delegate being called.
	if (!cancelGrab)
	{
		// If already grabbed grab with otherHand if in two handed mode also.
		FGrabInformation* grabbingInfomation;
		if (interactableSettings.twoHandedGrabbing && IsActorGrabbed())
		{
			// Save a reference to the hand.
			grabbingInfomation = &otherHandRefInfo;
			otherHandRefInfo.handRef = hand;
			otherHandRefInfo.targetComponent = hand->grabCollider;
			OnMeshGrabbed.Broadcast(hand, grabbableMesh);

			// Grab the grabbable with a second hand.
			switch (secondHandGrabMode)
			{
			case ESecondGrabMode::PhysicsHandle:
				// Enable physics handle mode on first hand to grab this grabbable.
				DropAttatchTo();
				PickupPhysicsHandle(handRefInfo);

				// Grab with physics handle and disable all rotational constraints.
				PickupPhysicsHandle(otherHandRefInfo);
				break;
			case ESecondGrabMode::TrackRotation:
				// Disable the first hands rotation tracking so we can target the second hand.
				handRefInfo.handRef->grabHandle->updateTargetRotation = false;
				// Save original grab rotation for the second hand.
				FRotator lookAtHandRot = (handRefInfo.handRef->controller->GetComponentLocation() - otherHandRefInfo.handRef->controller->GetComponentLocation()).Rotation();
				secondHandOriginalTransform = FTransform(lookAtHandRot, grabbableMesh->GetComponentLocation(), FVector(1.0f));
				secondHandGrabbableRot = grabbableMesh->GetRelativeTransform();
				break;
			}
		}
		else
		{
			// Save a reference to the hand.
			grabbingInfomation = &handRefInfo;
			handRefInfo.handRef = hand;
			handRefInfo.targetComponent = hand->grabCollider;

			// Dettatch this component from any component or actor before grabbing.
			if (grabbableMesh->GetAttachParent())
			{
				grabbableMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

#if DEVELOPMENT
				if (debug) UE_LOG(LogGrabbable, Log, TEXT("The grabbable actor %s, has been disconnected from its parent when grabbed."), *GetName());
#endif
			}

			// Grab with the correct method depending on the selected grab mode.
			switch (grabMode)
			{
			case EGrabMode::AttatchTo:
			case EGrabMode::AttatchToWithPhysics:
				PickupAttatchTo();
				break;
			case EGrabMode::PhysicsHandle:
				PickupPhysicsHandle(handRefInfo);
				break;
			}

			// Apply mass change if enabled.
			if (changeMassOnGrab) grabbableMesh->SetMassOverrideInKg(NAME_None, massWhenGrabbed, true);

			// Apply and save last physics material.
			if (physicsMaterialWhileGrabbedEnabled && physicsMaterialWhileGrabbed)
			{
				grabbableMesh->GetBodyInstance()->SetPhysMaterialOverride(physicsMaterialWhileGrabbed);
			}
#if DEVELOPMENT
			else if (debug) UE_LOG(LogGrabbable, Warning, TEXT("Cannot update physics material on grab as the physicsMaterialWhileGrabbed is null in the grabbable actor %s."), *GetName());
#endif

			// Save the current original pickup transforms depending on the type of grab.
			if (snapToHand)
			{
				grabbingInfomation->originalPickupRelativeRotation = snapToHandRotationOffset;
				grabbingInfomation->originalRelativePickupOffset = snapToHandLocationOffset;
				ToggleLerping(true); // Lerp to snap offsets once grabbed.
			}
			// Otherwise get the original relative offset for the rotation and location when this is grabbed when compared to the hand scene component.
			else
			{
				FTransform targetTransform = grabbingInfomation->targetComponent->GetComponentTransform();
				grabbingInfomation->originalPickupRelativeRotation = targetTransform.InverseTransformRotation(grabbableMesh->GetComponentRotation().Quaternion()).Rotator();
				grabbingInfomation->originalRelativePickupOffset = targetTransform.InverseTransformPosition(grabbableMesh->GetComponentLocation());
			}
		}

		// Add the hand to the array of ignored actors for collision checks.
		ignoredActors.Add(hand);

		// Save the current world location.
		grabbingInfomation->originalWorldGrabbedLocation = grabbableMesh->GetComponentLocation();

		// Call ending delegate for this function...
		OnMeshGrabbedEnd.Broadcast(hand);
	}
	else cancelGrab = false;
}

void AGrabbableActor::GrabReleased_Implementation(AVRHand* hand)
{
	// If two handed grabbing is enabled and the other hand has grabbed this component.
	AVRHand* newHandRef = nullptr;
	if (interactableSettings.twoHandedGrabbing && otherHandRefInfo.handRef)
	{
		// if its the first hand to grab releasing the component while a second hand is grabbing this grabbable.
		if (handRefInfo.handRef == hand) newHandRef = otherHandRefInfo.handRef;
		// Otherwise if the hand releasing is the second hand to grab this grabbable actor release second hand and return.
		else
		{
			// Release second hand and reset to null.
			switch (secondHandGrabMode)
			{
			case ESecondGrabMode::PhysicsHandle:
				DropPhysicsHandle(otherHandRefInfo);
				break;
			case ESecondGrabMode::TrackRotation:
				// Re-enable the first hands rotation tracking for the joint in the grabHandle.
				handRefInfo.handRef->grabHandle->updateTargetRotation = true;
				break;
			}

			// Clear other hand as its been released or swapped to the handRefInfo...
			otherHandRefInfo.Reset();
			return;
		}	
	}

	// Release component.
	DropAttatchTo();
	DropPhysicsHandle(handRefInfo);

	// Broadcast to delegate.
	OnMeshReleased.Broadcast(hand, grabbableMesh);

	// Remove mass change if enabled.
	if (changeMassOnGrab) grabbableMesh->SetMassOverrideInKg(NAME_None, 0.0f, false);

	// Reset the physics material override to what it was before grabbed.
	if (physicsMaterialWhileGrabbedEnabled && physicsMaterialWhileGrabbed)
	{
		grabbableMesh->GetBodyInstance()->SetPhysMaterialOverride(nullptr);
	}
#if DEVELOPMENT
	else if (debug) UE_LOG(LogGrabbable, Warning, TEXT("Cannot update physics material on grab as the physicsMaterialWhileGrabbed is null in the grabbable actor %s."), *GetName());
#endif

	// Check if we should adjust the velocity based on how heavy the component is.
	if (considerMassWhenThrown)
	{
		float currentMass = grabbableMesh->GetMass();
		if (currentMass > 1.0f)
		{
			// Only take a maximum of 75% velocity from the current.
			float massMultiplier = 1 - FMath::Clamp(FMath::Clamp(currentMass, 0.0f, 20.0f) / 20.0f, 0.0f, 0.6f);
			grabbableMesh->SetPhysicsLinearVelocity(grabbableMesh->GetPhysicsLinearVelocity() * massMultiplier);
			grabbableMesh->SetPhysicsAngularVelocityInRadians(grabbableMesh->GetPhysicsAngularVelocityInRadians() * massMultiplier);
		}
	}

	// Reset values to default. Do last.
	ignoredActors.Remove(hand);
	handRefInfo.Reset();
	otherHandRefInfo.Reset();
	lerping = false;

	// If there was a newHand to be grabbing this comp do so.
	if (newHandRef) newHandRef->ForceGrab(this);
}

void AGrabbableActor::Dragging_Implementation(float deltaTime)
{
	// If this grabbable is grabbed by a hand perform checks to update hand grab distance etc.
	if (grabMode != EGrabMode::AttatchTo && handRefInfo.handRef)
	{
		// Find the current pickup and rotation offset for where the grabbable should be.
		UpdateGrabInformation();

		// Update the distance between the grabbable and the hand to handle releasing if distance is too big.
		FVector currentRelativePickupOffset = handRefInfo.worldPickupOffset - grabbableMesh->GetComponentLocation();
		lastHandGrabDistance = interactableSettings.handDistance;
		interactableSettings.handDistance = currentRelativePickupOffset.Size();

		// If grabbed by the other hand change functionality.
		if (otherHandRefInfo.handRef)
		{
			// Grab the grabbable with a second hand.
			switch (secondHandGrabMode)
			{
			case ESecondGrabMode::PhysicsHandle:
				// Don't need to continue as collisions are always enabled in this two handed grab mode.
				return;
				break;
			case ESecondGrabMode::TrackRotation:
				// Update the direction to target while grabbed.
				if (physicsAttatched) handRefInfo.handRef->grabHandle->UpdateHandleTargetRotation(handRefInfo.worldRotationOffset);
				else if (attatched) grabbableMesh->SetRelativeRotation(handRefInfo.worldRotationOffset);
				break;
			}
		}

		// Update current grabbables velocity change over time.
		lastFrameVelocity = currentFrameVelocity;
		currentFrameVelocity = handRefInfo.handRef->handVelocity.Size();
		currentVelocityChange = FMath::Abs((lastFrameVelocity - currentFrameVelocity) / GetWorld()->GetDeltaSeconds());

		// Handle collisions for collidable grab mode.
		if (grabMode == EGrabMode::AttatchToWithPhysics)
		{
			// If hit end lerp and grab with physics handle to handle collisions...
			if (GetColliding())
			{
				if (lerping) ToggleLerping(false);
				if (!physicsAttatched)
				{
					DropAttatchTo();
					PickupPhysicsHandle(handRefInfo);
					switch (secondHandGrabMode)
					{
					case ESecondGrabMode::TrackRotation:
						if (IsActorGrabbedTwoHanded()) PickupPhysicsHandle(otherHandRefInfo);
						break;
					}
				}
			}
			// If nothing is hit and the component is physics attached, lerp back to the hand.
			else if (physicsAttatched)
			{
				switch (lerpMode)
				{
				case EReturnMode::Default:
				{
					// Attach if not yet attatched and toggle lerping.
					if (!attatched)
					{
						DropPhysicsHandle(handRefInfo);
						PickupAttatchTo();
						switch (secondHandGrabMode)
						{
						case ESecondGrabMode::TrackRotation:
							if (IsActorGrabbedTwoHanded()) DropPhysicsHandle(otherHandRefInfo);
							break;
						}
					}
					ToggleLerping(true);
				}
				break;
				case EReturnMode::ClearPathToHand:
				{
					// Check path back to the hand is clear before lerping.
					FHitResult sweepHit = SweepActor();
					if (!sweepHit.bBlockingHit)
					{
						// If not yet attached do so.
						if (!attatched)
						{
							DropPhysicsHandle(handRefInfo);
							PickupAttatchTo();
							switch (secondHandGrabMode)
							{
							case ESecondGrabMode::TrackRotation:
								if (IsActorGrabbedTwoHanded()) DropPhysicsHandle(otherHandRefInfo);
								break;
							}
						}

						// Start the lerp if not already running.
						if (!lerping) ToggleLerping(true);
					}
					// If the path to the hand is blocked end lerping.
					else if (lerping) ToggleLerping(false);
				}
				break;
				}
			}

			// Lerp back to the hand if enabled.
			if (lerping) LerpingBack(deltaTime);
		}
	}
}

void AGrabbableActor::Overlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::Overlapping_Implementation(hand);
}

void AGrabbableActor::EndOverlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::EndOverlapping_Implementation(hand);
}

void AGrabbableActor::Teleported_Implementation()
{
	if (grabMode == EGrabMode::PhysicsHandle || physicsAttatched)
	{
		// Ensure these values are up to this frame.
		UpdateGrabInformation();

		// Position component in this new location.
		grabbableMesh->SetWorldLocationAndRotation(handRefInfo.worldPickupOffset, handRefInfo.worldRotationOffset, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

FHandInterfaceSettings AGrabbableActor::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void AGrabbableActor::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}
