// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/VRHand.h"
#include "Player/VRPawn.h"
#include "Player/HandsAnimInstance.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "XRMotionControllerBase.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Project/VRFunctionLibrary.h"
#include "Project/EffectsContainer.h"
#include "Kismet/KismetSystemLibrary.h"
#include <Sound/SoundBase.h>
#include "WidgetInteractionComponent.h"
#include "WidgetComponent.h"

DEFINE_LOG_CATEGORY(LogHand);

AVRHand::AVRHand()
{
	// Tick for this function is ran in the pawn class.
	PrimaryActorTick.bCanEverTick = false; 

	// Setup motion controller. Default setup.
	controller = CreateDefaultSubobject<UMotionControllerComponent>("Controller");
	controller->MotionSource = FXRMotionControllerBase::LeftHandSourceId;
	controller->SetupAttachment(scene);
	controller->bDisableLowLatencyUpdate = true;
	RootComponent = controller;

	// handRoot comp.
	handRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HandRoot"));
	handRoot->SetupAttachment(controller);

	// Skeletal mesh component for the hand model. Default setup.
	handSkel = CreateDefaultSubobject<USkeletalMeshComponent>("handSkel");
	handSkel->SetCollisionProfileName("Hand");
	handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	handSkel->SetupAttachment(handRoot);
	handSkel->SetRenderCustomDepth(true);
	handSkel->SetGenerateOverlapEvents(true);
	handSkel->SetCustomDepthStencilValue(1); // Custom stencil mask material for showing hands through objects.
	handSkel->SetRelativeTransform(FTransform(FRotator(-20.0f, 0.0f, 0.0f), FVector(-18.0f, 0.0f, 0.0f), FVector(0.27f, 0.27f, 0.27f)));

	// Collider to handle physics constrained components. Default setup.
	physicsCollider = CreateDefaultSubobject<UBoxComponent>("PhysicsCollider");
	physicsCollider->SetupAttachment(handRoot);
	physicsCollider->SetCollisionProfileName("PhysicsActorOn");
	physicsCollider->SetRelativeTransform(FTransform(FRotator(-24.0f, 0.0f, 0.0f), FVector(-8.0f, 0.4f, 4.5f), FVector(1.0f, 1.0f, 1.0f)));
	physicsCollider->SetBoxExtent(FVector(9.0f, 2.2f, 4.5f)); 
	physicsCollider->SetSimulatePhysics(true); 
	physicsCollider->SetGenerateOverlapEvents(true);

	// Collider to find interactables. Default setup.
	grabCollider = CreateDefaultSubobject<UBoxComponent>("GrabCollider");
	grabCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	grabCollider->SetCollisionProfileName(FName("HandOverlap"));
	grabCollider->SetupAttachment(handRoot);
	grabCollider->SetRelativeTransform(FTransform(FRotator(-24.0f, 0.0f, 0.0f), FVector(-7.0f, 3.0f, -3.1f), FVector(1.0f, 1.0f, 1.0f)));
	grabCollider->SetBoxExtent(FVector(8.0f, 2.3f, 5.0f));

	// Initialise the physics handles.
	grabHandle = CreateDefaultSubobject<UVRPhysicsHandleComponent>("GrabHandle");
	physicsHandle = CreateDefaultSubobject<UVRPhysicsHandleComponent>("PhysicsHandle");

	// Setup widget interaction components.
	widgetOverlap = CreateDefaultSubobject<USphereComponent>("WidgetOverlap");
	widgetOverlap->SetMobility(EComponentMobility::Movable);
	widgetOverlap->SetupAttachment(handSkel);
	widgetOverlap->SetSphereRadius(3.0f);
	widgetOverlap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	widgetOverlap->SetCollisionObjectType(ECC_Hand);
	widgetOverlap->SetCollisionResponseToAllChannels(ECR_Ignore);
	widgetOverlap->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	widgetInteractor = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractor"));
	widgetInteractor->SetupAttachment(widgetOverlap);
	widgetInteractor->InteractionDistance = 30.0f;
	widgetInteractor->InteractionSource = EWidgetInteractionSource::World;
	widgetInteractor->bEnableHitTesting = true;

	// Ensure fast widget path is disabled as it optimizes away the functionality we need when building in VR.
	GSlateFastWidgetPath = 0;

	// Setup movement direction component.
	movementTarget = CreateDefaultSubobject<USceneComponent>("MovementTarget");
	movementTarget->SetMobility(EComponentMobility::Movable);
	movementTarget->SetupAttachment(handSkel);

	// Initialise the hands audio component.
	handAudio = CreateDefaultSubobject<UAudioComponent>("HandAudio");
	handAudio->SetupAttachment(grabCollider);
	handAudio->bAutoActivate = false;

	// Initialise default variables.
	handEnum = EControllerHand::Left;
	controllerType = EVRController::Index;
	objectToGrab = nullptr;
	objectInHand = nullptr;
	grabbing = false;
	gripping = false;
	foundController = false;
	hideOnGrab = true;	
	active = true;
	handIsLocked = false;
	collisionEnabled = false;
	thumbstick = FVector2D(0.0f, 0.0f);
	distanceFrameCount = 0;

	// PhysicsCollider extent and position values for closed hand state.
	pcClosedExtent = FVector(6.0f, 3.4f, 5.0f);
	pcClosedPositiion = FVector(-12.0f, 1.5f, -0.8f);
	
#if WITH_EDITOR
	debug = false;
	devModeEnabled = false;
#endif
}

void AVRHand::BeginPlay()
{
	Super::BeginPlay();

	// Save the original offset and extent of the physics box Collider for when the hand is open.
	pcOriginalOffset = physicsCollider->RelativeLocation;
	pcOpenExtent = physicsCollider->GetUnscaledBoxExtent();

	// Create a joint between the hand skel and the Collider itself so it tracks towards the hand with a max pushing force.
	FName handRootBoneName = handSkel->GetBoneName(0);
	physicsHandle->CreateJointAndFollowLocationWithRotation(physicsCollider, handSkel, NAME_None, physicsCollider->GetComponentLocation(),
		physicsCollider->GetComponentRotation());

	// Setup widget interaction attachments.
	widgetOverlap->AttachToComponent(handSkel, FAttachmentTransformRules::SnapToTargetNotIncludingScale, "FingerSocket");

	// Setup delegate for overlapping widget component.
	if (!widgetOverlap->OnComponentBeginOverlap.Contains(this, "WidgetInteractorOverlapBegin"))
	{
		widgetOverlap->OnComponentBeginOverlap.AddDynamic(this, &AVRHand::WidgetInteractorOverlapBegin);
	}
}

void AVRHand::SetupHand(AVRHand * oppositeHand, AVRPawn* playerRef, bool dev)
{
	// Initialise class variables.
	player = playerRef;
	otherHand = oppositeHand;
	owningController = player->GetWorld()->GetFirstPlayerController();

	// Use dev mode to disable areas of code when in developer mode.
#if WITH_EDITOR
	devModeEnabled = dev;
#endif

	// Save the original transform of the hand for calculating offsets.
	originalHandTransform = controller->GetComponentTransform();

	// Set up the controller offsets for the current type of controller selected.
	if (!devModeEnabled) SetupControllerOffset();
}

void AVRHand::SetControllerType(EVRController type)
{
	controllerType = type;
	SetupControllerOffset();
}

void AVRHand::SetupControllerOffset()
{
	// Reset the hand transform. (Default for VIVE controller)
	handRoot->SetRelativeTransform(FTransform(FRotator(0.0f), FVector(0.0f), FVector(1.0f)));

	// Depending on the selected option change the offset of the controller...
	switch (controllerType)
	{
	case EVRController::Index:
		handRoot->AddLocalOffset(FVector(-2.4f, 0.0f, -5.3f));
		handRoot->AddLocalRotation(FRotator(-30.0f, 0.0f, 0.0f));
	break;
	case EVRController::Oculus:
		handRoot->AddLocalOffset(FVector(7.5f, 0.0f, 0.0f));
	break;
	}
}

void AVRHand::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Calculate controller velocity as its not simulating physics.
	lastHandPosition = currentHandPosition;
	currentHandPosition = controller->GetComponentLocation();
	handVelocity = (currentHandPosition - lastHandPosition) / DeltaTime;
	
	// Calculate angular velocity for the controller for when its not simulating physics.
	lastHandRotation = currentHandRotation;
	currentHandRotation = controller->GetComponentQuat();
	FQuat deltaRot = lastHandRotation.Inverse() * currentHandRotation;
	FVector axis;
	float angle;
	deltaRot.ToAxisAndAngle(axis, angle);
	angle = FMath::RadiansToDegrees(angle);
	handAngularVelocity = currentHandRotation.RotateVector((axis * angle) / DeltaTime);

	// Keep the physical Collider tracked to the hand correctly.
	UpdatePhysicalCollision(DeltaTime);

	// Update the animation instance variables for the handSkel.
	UpdateAnimationInstance();

	// If an object is grabbed by the hand update the relevant functions.
	if (objectInHand)
	{
		// Execute dragging for the grabbed object.
		IHandsInterface::Execute_Dragging(objectInHand, DeltaTime);

		// Update interactable distance for releasing over max distance.
		CheckInteractablesDistance();
	}
	// Otherwise look for objects to grab...
	else if (!gripping) CheckForOverlappingActors();
}

void AVRHand::WidgetInteractorOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// If the other component was a widget press it.
	if (UWidgetComponent* widgetOverlap = Cast<UWidgetComponent>(OtherComp))
	{
		// Rotate the widget interactor to face what we have overlapped with and press then release the pointer key.
		FVector worldDirection = widgetInteractor->GetComponentLocation() - SweepResult.Location;
		widgetInteractor->SetWorldRotation(worldDirection.Rotation());
		widgetInteractor->PressPointerKey(EKeys::LeftMouseButton);
		widgetInteractor->ReleasePointerKey(EKeys::LeftMouseButton);

		// Rumble the controller to give feedback that the button was successfully pressed.
		PlayFeedback();
	}
}

void AVRHand::Grab()
{
	// The player is current grabbing.
	grabbing = true;

	// If the player is holding something in hand already then its locked so call grab locked function on the object in hand.
	if (objectInHand && handIsLocked) IHandsInterface::Execute_GrabbedWhileLocked(objectInHand);

#if WITH_EDITOR
	// If in dev-mode ensure the trigger is 1.0f when grabbed.
	if (devModeEnabled) trigger = 1.0f;
#endif

	// If the object to grab is new grab it.
	if (objectToGrab && !objectInHand)
	{
		// Release the actor from the other hand if it has the objectToGrab grabbed and the grabbed object does NOT support two handed grabbing.
		if (otherHand && objectToGrab == otherHand->objectInHand)
		{
			FHandInterfaceSettings otherGrabbedObjectSettings = IHandsInterface::Execute_GetInterfaceSettings(otherHand->objectInHand);
			if (!otherGrabbedObjectSettings.twoHandedGrabbing) otherHand->ReleaseGrabbedActor();
		}

		// Disable the handSkel, physicsCollider and grabCollider. Also hide hand if the option is enabled.
		ActivateCollision(false);
		if (hideOnGrab) handSkel->SetVisibility(false);
		
		// Update grabbed variables.
		objectInHand = objectToGrab;
		IHandsInterface::Execute_GrabPressed(objectInHand, this);
		IHandsInterface::Execute_EndOverlapping(objectInHand, this);

		// Feedback to indicate the object has been grabbed.
		PlayFeedback(); 
	}
}

void AVRHand::ForceGrab(UObject* objectToForceGrab)
{
	// Force remove current object in the hand.
	objectInHand = nullptr;

	// Grab the object.
	if (grabbing)
	{	
		objectToGrab = objectToForceGrab;
		Grab();
	}
}

void AVRHand::Drop()
{
#if WITH_EDITOR
	// If in dev-mode ensure trigger value is 0.0f when dropped.
	if (devModeEnabled) trigger = 0.0f;
#endif

	// If there is something in the hand.
	if (objectInHand)
	{
		// Get the objects interface settings.
		FHandInterfaceSettings grabbedObjectSettings = IHandsInterface::Execute_GetInterfaceSettings(objectInHand);
		// Release the object if it is not locked to the hand.
		if (!grabbedObjectSettings.lockedToHand) ReleaseGrabbedActor();
		// Otherwise 
		else
		{
			// Execute release while locked on the interactable.
			if (handIsLocked) IHandsInterface::Execute_ReleasedWhileLocked(objectInHand);
			else handIsLocked = true;
		}
	}

	// End grabbing.
	grabbing = false;
}

void AVRHand::Interact(bool pressed)
{
	if (objectInHand)
	{
		IHandsInterface::Execute_Interact(objectInHand, pressed);
	}
}

void AVRHand::ReleaseGrabbedActor()
{
	// If an object is in the hand.
 	if (objectInHand)
	{	
		// Execute release interactable.
		IHandsInterface::Execute_GrabReleased(objectInHand, this);

		// Nullify grabbed objects variables.
		objectInHand = nullptr;
		objectToGrab = nullptr;
		handIsLocked = false;

		// Show the hands if they are hidden and start checking the hands collision in 0.4f seconds to be re-enabled.
		if (hideOnGrab) handSkel->SetVisibility(true);	
		ActivateCollision(true, 0.6f);
	}
}

void AVRHand::Grip(bool pressed)
{
	// Currently gripping.
	gripping = pressed;

	// Execute the interactables grip pressed and released functions.
 	if (pressed)
 	{
 		if (objectToGrab) IHandsInterface::Execute_GripPressed(objectToGrab, this);
 		else if (objectInHand) IHandsInterface::Execute_GripPressed(objectInHand, this);
 	}
 	else
 	{
 		if (objectToGrab) IHandsInterface::Execute_GripReleased(objectToGrab);
 		else if (objectInHand) IHandsInterface::Execute_GripReleased(objectInHand);
 	}

	// Release the grabbed interactable if the hand is locked and the grip button is released.
	if (objectInHand)
	{	
		FHandInterfaceSettings grabbedObjectSettings = IHandsInterface::Execute_GetInterfaceSettings(objectInHand);
		if (grabbedObjectSettings.lockedToHand && !pressed)
		{
			ReleaseGrabbedActor();
			grabbing = false;
		}	
	}
}

void AVRHand::TeleportHand()
{
	// Used on components that need re-positioning after a teleportation.
	if (objectInHand) IHandsInterface::Execute_Teleported(objectInHand);
}

void AVRHand::UpdateControllerTrackedState()
{
	// If dev mode is enabled.
#if WITH_EDITOR
	if (devModeEnabled)
	{
		foundController = true;
		return;
	}
#endif

	// Only allow the collision on this hand to be enabled if the controller is being tracked.
	bool trackingController = controller->IsTracked();
	if (trackingController)
	{
		if (!foundController)
		{
			ActivateCollision(true);
			foundController = true;
#if WITH_EDITOR
			if (debug) UE_LOG(LogHand, Warning, TEXT("Found and tracking the controller owned by %s"), *GetName());
#endif
		}
	}
	else if (foundController)
	{
		ActivateCollision(false);
		foundController = false;
#if WITH_EDITOR
		if (debug) UE_LOG(LogHand, Warning, TEXT("Lost the controller tracking owned by %s"), *GetName());
#endif
	}
}

void AVRHand::CheckForOverlappingActors()
{
	// Get grabColliders current overlapping components.
	TArray<UPrimitiveComponent*> overlapping;
	grabCollider->GetOverlappingComponents(overlapping);
	UObject* toGrab = nullptr;
	float smallestDistance = 100000.0f;

	// If components are found.
	if (overlapping.Num() > 0)
	{
		// Loop through each overlapping component and find the closest one with an interface.
		for (UPrimitiveComponent* comp : overlapping)
		{
			// If a object with an interface has been found.
			UObject* objectWithInterface = LookForInterface(comp);
			if (objectWithInterface)
			{
				// Make sure this interface is currently allowing interaction.
				FHandInterfaceSettings objectsInterfaceSettings = IHandsInterface::Execute_GetInterfaceSettings(objectWithInterface);
				if (!objectsInterfaceSettings.canInteract)
				{
					// End overlapping before exiting this function.
					if (objectToGrab)
					{
						IHandsInterface::Execute_EndOverlapping(objectToGrab, this);
						objectToGrab = nullptr;
					}
					// Go to next item in the overlapping array.
					continue;
				}

				// Update the closest component and smallest distance value to compare to the next component in the array.
				float currentDistance = (comp->GetComponentLocation() - grabCollider->GetComponentLocation()).Size();
				if (currentDistance < smallestDistance)
				{
					smallestDistance = currentDistance;
					toGrab = objectWithInterface;
				}
			}
		}
	}

	// If the object to grab has changed from null or the last object continue to check whats been changed.
	if (objectToGrab != toGrab)
	{
		// If there was an object To Grab end overlapping. (Un-Highlight)
		if (objectToGrab)
		{
			IHandsInterface::Execute_EndOverlapping(objectToGrab, this);
			objectToGrab = nullptr;
		}

		// If there was an object to grab overlap this object and save its interface for checking variables... (Highlight)
		if (toGrab)
		{
			objectToGrab = toGrab;
			IHandsInterface::Execute_Overlapping(objectToGrab, this);
		}
	}
}

UObject* AVRHand::LookForInterface(USceneComponent* comp)
{
	// Check the component for the interface then work way up each parent until one is found. If not return null.
	bool componentHasTag = comp->ComponentHasTag("Grabbable");
	bool componentHasInterface = comp->GetClass()->ImplementsInterface(UHandsInterface::StaticClass());
	if (componentHasInterface) return comp;
	else
	{
		// Check components actor for the interface before searching through its parents.
		bool actorHasTag = comp->GetOwner()->ActorHasTag(FName("Grabbable"));
		bool actorHasInterface = comp->GetOwner()->GetClass()->ImplementsInterface(UHandsInterface::StaticClass());
		if (actorHasInterface && (actorHasTag || componentHasTag)) return comp->GetOwner();
		// If the components actor doest have the interface and has a parent component look for interface here.
		else if (comp->GetAttachParent())
		{
			// Look through each parent in order from bottom to top, then if nothing is found check the actor itself.
			bool searching = true;
			USceneComponent* parentComponent = comp->GetAttachParent();
			while (searching)
			{
				bool parentHasInterface = parentComponent->GetClass()->ImplementsInterface(UHandsInterface::StaticClass());
				// If the parent has the interface and is grabbable use this component.
				if (parentHasInterface)
				{
					return parentComponent;
					searching = false;
				}
				else if (parentComponent->GetAttachParent()) parentComponent = parentComponent->GetAttachParent();
				// If there are no more attach parents exit the while loop.
				else searching = false;
			}
		}
	}
	return nullptr;
}


void AVRHand::UpdatePhysicalCollision(float deltaTime)
{
	// Update the boxes extent and positioning for when the hand is closed or opened.
	FVector controllerClosedOffset = (pcOriginalOffset - pcClosedPositiion) * -trigger;
	FVector lerpingExt = FMath::Lerp(pcOpenExtent, pcClosedExtent, trigger);
	physicsCollider->SetBoxExtent(lerpingExt);
	FVector newControllerLocation = controller->GetComponentTransform().TransformPosition(controllerClosedOffset);

	// Check if the current location is too far away from the hand. If it is teleport the physicsCollider into position.
	float distanceToController = (controller->GetComponentTransform().TransformPositionNoScale(pcOriginalOffset) - physicsCollider->GetComponentLocation()).Size();
	if (distanceToController >= 18.0f)
	{
		// Check if the location to teleport to is still overlapping something if not teleport physicsCollider back to the hand.
		FVector positionToCheck = controller->GetComponentTransform().TransformPositionNoScale(pcOriginalOffset);
		FTransform newTransform = physicsCollider->GetComponentTransform();
		newTransform.SetLocation(positionToCheck);
		TArray<UPrimitiveComponent*> overlappedComps;
		bool overlapping = UVRFunctionLibrary::ComponentOverlapComponentsByChannel(physicsCollider, newTransform, ECC_Pawn, player->actorsToIgnore, overlappedComps, true);
		if (!overlapping) physicsCollider->SetWorldLocation(positionToCheck, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AVRHand::CheckInteractablesDistance()
{
	if (objectInHand)
	{
		// Get the grabbed objects interface settings.
		FHandInterfaceSettings grabbedObjectSettings = IHandsInterface::Execute_GetInterfaceSettings(objectInHand);

		// Get required variables from the current grabbed objects interface.
		float currentHandGrabDistance = grabbedObjectSettings.handDistance;
		float currentHandMinRumbleDist = grabbedObjectSettings.handMinRumbleDistance;
		bool currentCanRelease = grabbedObjectSettings.canRelease;

		// Release the intractable if the relative distance becomes greater than the set amount in the interactables interface.
		if (grabbedObjectSettings.releaseDistance < currentHandGrabDistance && currentCanRelease)
		{
			// Ensure the distance is the same and not a one of frame.
			if (distanceFrameCount > 1)
			{
				ReleaseGrabbedActor();
				distanceFrameCount = 0;
				return;
			}

			distanceFrameCount++;
			return;
		}
		// Otherwise Rumble the hand with the intensity relative to the distance between the hand an grabbed actor. Use default feedback effect.
		else if (currentHandGrabDistance > currentHandMinRumbleDist && currentCanRelease) PlayFeedback(nullptr, (currentHandGrabDistance - currentHandMinRumbleDist) / 20, true);
		else return;
	}
}

void AVRHand::UpdateAnimationInstance()
{
	// Get the hand animation class and update animation variables.
	UHandsAnimInstance* handAnim = Cast<UHandsAnimInstance>(handSkel->GetAnimInstance());	
	if (handAnim)
	{
		handAnim->pointing = gripping;
		handAnim->fingerClosingAmount = (1 - trigger);
		handAnim->handClosingAmount = trigger * 100.0f;
	}
}

void AVRHand::ResetHandle(UVRPhysicsHandleComponent* handleToReset)
{
	CHECK_RETURN(LogHand, !handleToReset, "The hand class %s, cannot reset a null handle in the ResetPhysicsHandle function.");
	// Reset the joint to its original values.
	handleToReset->ResetJoint();
}

void AVRHand::ActivateCollision(bool open, float openDelay)
{
	if (handSkel)
	{
		if (open)// When the hand is open allow all collision to be enabled after a delay while interactables fall out of the way.
		{
			handSkel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			FTimerDelegate timerDel;
			timerDel.BindUFunction(this, FName("CollisionDelay"));
			GetWorldTimerManager().SetTimer(colTimerHandle, timerDel, 0.1f, true, openDelay);
			collisionEnabled = true;
		}
		else// Disable collision while the hand is closed to prevent accidental interactions.
		{
			handSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			physicsCollider->SetCollisionProfileName("PhysicsActorOff");
			physicsCollider->SetNotifyRigidBodyCollision(false);
			collisionEnabled = false;
			GetWorldTimerManager().ClearTimer(colTimerHandle);
		}

#if WITH_EDITOR
		// Print debug message on activation/deactivation.
		if (debug) UE_LOG(LogHand, Warning, TEXT("Collision in the hand %s, is %s"), *GetName(), collisionEnabled ? TEXT("enabled") : TEXT("disabled"));
#endif
	}
}

void AVRHand::CollisionDelay()
{
	// Only re-enable the collision if the handSkel is no longer overlapping anything.
	TArray<UPrimitiveComponent*> overlappingComps;
	bool overlapping = UVRFunctionLibrary::ComponentOverlapComponentsByChannel(handSkel, handSkel->GetComponentTransform(), ECC_Hand, player->actorsToIgnore, overlappingComps, true);

	// If no longer overlapping any blocking collision to the hand.
	if (!overlapping)
	{
		// Also ensure physics collider is no longer overlapping.
		bool physicsOverlapping = UVRFunctionLibrary::ComponentOverlapComponentsByChannel(physicsCollider, physicsCollider->GetComponentTransform(), ECC_PhysicsBody, player->actorsToIgnore, overlappingComps, true);
		if (!physicsOverlapping)
		{
			// Re-enable collision in this classes colliding components.
			handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			physicsCollider->SetCollisionProfileName("PhysicsActorOn");
			physicsCollider->SetNotifyRigidBodyCollision(true);

			// End this function loop.
			GetWorld()->GetTimerManager().ClearTimer(colTimerHandle);
		}	
	}
}

bool AVRHand::PlaySound(USoundBase* sound, float volume, float pitch, bool replace)
{
	// Ensure the audio component is initialized.
	if (handAudio)
	{
		// If replace is true replace the audio otherwise if playing do not play the sound.
		bool playing = handAudio->IsPlaying();
		bool shouldPlay = replace ? true : !playing;
		if (shouldPlay)
		{
			// Get the sound to play either from passed reference or the default sound.
			USoundBase* soundToPlay = sound ? sound : GetEffects()->GetAudioEffect("DefaultCollision");		
			// Play sound if not nullptr.
			if (soundToPlay)
			{
				if (playing) handAudio->Stop();
				handAudio->SetVolumeMultiplier(volume);
				handAudio->SetPitchMultiplier(pitch);
				handAudio->SetSound(soundToPlay);
				handAudio->Play();
				return true;
			}
			else return false;
		}
		else return false;
	}
	else
	{
		UE_LOG(LogHand, Log, TEXT("PlaySound: The sound could not be played as the refference to the hand audio component has been lost or not initlaised in the hand class %s."), *GetName());
		return false;
	}
}

void AVRHand::StopSound(bool fade, float timeToFade)
{
	if (fade) handAudio->FadeOut(timeToFade, 0.0f);
	else handAudio->Stop();
}

bool AVRHand::PlayFeedback(UHapticFeedbackEffect_Base* feedback, float intensity, bool replace)
{
	if (owningController)
	{
		// Check if should replace feedback effect. If don't replace, still playing and the new intensity is weaker don't play haptic effect.
		bool shouldPlay = replace ? true : (IsPlayingFeedback() ? GetCurrentFeedbackIntensity() < intensity : true);

		// If should play then play the given feedback haptic effect on this hand classes controller.
		if (shouldPlay)
		{
			// If feedback is null use default haptic feedback otherwise use the feedback pointer passed into this function.
			UHapticFeedbackEffect_Base* feedbackToUse = feedback;
			if (!feedbackToUse) feedbackToUse = GetEffects()->GetFeedbackEffect("Default");

			// Play the given haptic effect if not nullptr.
			if (feedbackToUse)
			{
				currentHapticIntesity = intensity;
				owningController->PlayHapticEffect(feedbackToUse, handEnum, intensity * player->hapticIntensity, false);
				return true;
			}
			else return false;
		}
		else return false;
	}	
	else
	{
	    UE_LOG(LogHand, Log, TEXT("PlayFeedback: The feedback could not be played as the refference to the owning controller has been lost in the hand class %s."), *GetName());
		return false;
	}
}

UEffectsContainer* AVRHand::GetEffects()
{
	if (player && player->IsValidLowLevel()) return player->GetPawnEffects();
	else return nullptr;
}

float AVRHand::GetCurrentFeedbackIntensity()
{
	if (IsPlayingFeedback()) return currentHapticIntesity;
	else return 0.0f;
}

bool AVRHand::IsPlayingFeedback()
{
	bool isPlayingHapticEffect = false;
	if (owningController)
	{
		if (handEnum == EControllerHand::Left && owningController->ActiveHapticEffect_Left.IsValid()) isPlayingHapticEffect = true;
		else if (owningController->ActiveHapticEffect_Right.IsValid()) isPlayingHapticEffect = true;
	}

	// Return if this hand classes controller is playing a haptic effect.
	return isPlayingHapticEffect;
}

void AVRHand::Disable(bool disable)
{
	bool toggle = !disable;

	// Deactivate hand components.
	handSkel->SetActive(toggle);
	physicsCollider->SetActive(toggle);
	grabCollider->SetActive(toggle);

	// Hide hand in game.
	handSkel->SetVisibility(toggle);
	physicsCollider->SetVisibility(toggle);
	grabCollider->SetVisibility(toggle);

	// Deactivate hand colliders.
	if (toggle)
	{
		handSkel->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		physicsCollider->SetCollisionProfileName("PhysicsActorOn");
		grabCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		handSkel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		physicsCollider->SetCollisionProfileName("PhysicsActorOff");
		grabCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Disable this classes tick.
	this->SetActorTickEnabled(toggle);
	active = toggle;
}
