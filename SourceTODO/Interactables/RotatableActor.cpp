// Fill out your copyright notice in the Description page of Project Settings.

#include "RotatableActor.h"
#include "VR/VRFunctionLibrary.h"
#include "Player/VRHand.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "CustomComponent/SimpleTimeline.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "VR/EffectsContainer.h"

DEFINE_LOG_CATEGORY(LogRotatable);

ARotatableActor::ARotatableActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Initialise the physics pivot for handling rotational physics.
	pivot = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("Pivot"));
	RootComponent = pivot;

	// Setup constrained component.
	rotator = CreateDefaultSubobject<UBoxComponent>("RotatingMeshHolder");
	rotator->SetCollisionProfileName("ConstrainedComponent");
	rotator->SetBoxExtent(FVector::ZeroVector);
	rotator->SetupAttachment(pivot);

	// Initialise the direction pointers.
	rotationStart = CreateDefaultSubobject<UArrowComponent>(TEXT("Direction"));
	rotationStart->SetupAttachment(pivot);
	rotationStart->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	rotationStart->SetRelativeScale3D(FVector(0.4f));
	rotationStart->ArrowColor = FColor::Red;
	rotationAxis = CreateDefaultSubobject<UArrowComponent>(TEXT("Axis"));
	rotationAxis->SetupAttachment(pivot);
	rotationAxis->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	rotationAxis->SetRelativeScale3D(FVector(0.4f));
	rotationAxis->ArrowColor = FColor::Blue;

	// Initialise default constraint settings.		
	pivot->SetConstrainedComponents(nullptr, NAME_None, rotator, NAME_None);
	pivot->SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	pivot->SetAngularTwistLimit(EAngularConstraintMotion::ACM_Locked, 0.0f);
	pivot->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.0f);

	// Prevent rotatable from exceeding its rotation limit with a soft constraint with a high stiffness value as stiff constraint has visual errors.
	pivot->SetAngularSwing2Limit(ACM_Locked, 0.0f);
	pivot->SetAngularTwistLimit(ACM_Locked, 0.0f);
	pivot->ConstraintInstance.ProfileInstance.ConeLimit.bSoftConstraint = true;
	pivot->ConstraintInstance.ProfileInstance.ConeLimit.Stiffness = 1000000.0f;
	pivot->ConstraintInstance.ProfileInstance.ConeLimit.Damping = 0.0f;

	// Initialise rotators audio component.
	rotatorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("RotatorAudio"));
	rotatorAudio->SetupAttachment(pivot);
	rotatorAudio->bAutoActivate = false;

	// Initialise variables.
	handRef = nullptr;
	grabLocation = nullptr;
	returnCurve = nullptr;
	returnTimeline = nullptr;
	lockHapticEffect = nullptr; 
	imapctSoundEnabled = true;
	rotatingHapticEffect = nullptr;
	constrainedState = EConstraintState::Start;
	rotateMode = ERotateMode::StaticRotationCollision;
	simulatePhysics = true;
	limitedToRange = true;
	lockable = false;
	locked = false;
	lockWhileGrabbed = true;
	flipped = false;
	releaseOnOverRotation = true;
	grabWhileLocked = false;
	hapticRotationDelay = 0.1f;
	hapticIntensityMultiplier = 1.5f;
	lockingDistance = 5.0f;
	unlockingDistance = 10.0f;
	overRotationLimit = 50.0f;
	friction = 1.0f;
	rotationLimit = 180.0f;
	currentRotationLimit = 0.0f;
	revolutionCount = 0;
	cumulativeAngle = 0.0f;
	firstRun = true;
	lastYawAngle = 0.0f;
	startRotation = 0.0f;
	isReturning = false;

#if DEVELOPMENT
	debug = false;
#endif

	// Initialise interface variables.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.handMinRumbleDistance = 5.0f;
	interactableSettings.grabHandleData = FPhysicsHandleData(true, 200.0f, 200.0f, 8000.0f, 8000.0f, 50.0f);
}

void ARotatableActor::BeginPlay()
{
	Super::BeginPlay();

	// If rotatable component was not found return and destroy this class.
	if (rotator->GetNumChildrenComponents() == 0)
	{
		UE_LOG(LogRotatable, Warning, TEXT("The Rotatable Actor %s, cannot find a child staticMesh for grabbing, component has been destroyed..."), *GetName());
		Destroy();
		return;
	}

	// Save original relative rotation of the rotator.
	meshOriginalRelative = rotator->RelativeRotation;
	
	// Set up audio for this class.
	imapctSoundEnabled = true;
	if (rotatingSound) rotatorAudio->SetSound(rotatingSound);

	// Ensure all default variables are applied to private variables.
	cumulativeAngle = startRotation;
	actualCumulativeAngle = startRotation;
	lastHapticFeedbackRotation = startRotation;

	// Update the variables for the rotation limit.
	if (rotationLimit == 0)
	{
		limitedToRange = false;
		UE_LOG(LogRotatable, Warning, TEXT("The rotatable actor %s, has not rotation limit!"), *GetName());
		SetActorTickEnabled(false);
		return;
	}
	else
	{
		// Calculate current limit at the start of the game.
		if (rotationLimit < 0)
		{
			flipped = true;
			currentRotationLimit = FMath::Abs(rotationLimit);
		}
		else
		{
			flipped = false;
			currentRotationLimit = rotationLimit;
		}
	}

	// Setup pivot and physics values if this instance is simulating physics also Ensure that physics is enabled in this mode.
	if (rotateMode == ERotateMode::PhysicsRotation) simulatePhysics = true;
	if (simulatePhysics)
	{
		// If the constrained component is for some reason not set do it here.
		pivot->SetConstrainedComponents(nullptr, NAME_None, rotator, NAME_None);

		// Adjust any component variables.
		rotator->SetSimulatePhysics(true);
		rotator->SetMassOverrideInKg(NAME_None, 1.0f, true);

		// Ensure friction values are set.
		if (friction != 0.0f)
		{
			pivot->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
			pivot->SetAngularVelocityDrive(true, false);
			pivot->SetAngularDriveParams(0.0f, friction, 0.0f);
			pivot->SetAngularVelocityTarget(FVector(0));
		}

		// Initialise the constraint.	
		UpdateConstraintMode();
	}

	// If locked on begin play lock at start angle.
	if (lockable && locked)
	{
		Lock(startRotation);
		OnRotatableLock.Broadcast(startRotation);
	}

	// Setup the timeline to use a curve to rotate back to certain positions using SetRotation function etc.
	if (returnCurve) returnTimeline = USimpleTimeline::MAKE(returnCurve, "RotatableTimeline", this, "Returning", "ReturningEnd", this);
	else UE_LOG(LogRotatable, Warning, TEXT("The rotatable actor %s, has no curve so timeline functions will not work."), *GetName());
}

void ARotatableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update if not locked.
	if (!locked)
	{
		UpdateRotatable(DeltaTime);
		// Update the rotatable from the hand if grabbed.
		if (handRef)
		{
			if (rotateMode != ERotateMode::PhysicsRotation) UpdateRotation();
		}	
	}
}

#if WITH_EDITOR
void ARotatableActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get the name of the property that was changed.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If the property was the start rotation update it.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARotatableActor, startRotation))
	{
		// If the start rotation is changed update the yaw rotation of this rotatable actor if its within the specified rotation limit.
		if (rotationLimit < 0 ? startRotation < 0 && startRotation >= rotationLimit : startRotation >= 0 && startRotation <= rotationLimit)
		{
			rotator->SetRelativeRotation(FRotator(0.0f, startRotation, 0.0f));

			// Setup default cumulative rotation from current yaw rotation.
			cumulativeAngle = startRotation;
			actualCumulativeAngle = cumulativeAngle;
		}
		// Clamp start rotation within its limits.
		else startRotation = rotationLimit < 0 ? FMath::Clamp(startRotation, rotationLimit, 0.0f) : FMath::Clamp(startRotation, 0.0f, rotationLimit);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ARotatableActor, friction))
	{
		// Update if greater than 0 if not reset.
		if (friction > 0)
		{
			pivot->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
			pivot->SetAngularVelocityDrive(true, false);
			pivot->SetAngularDriveParams(0.0f, friction, 0.0f);
			pivot->SetAngularVelocityTarget(FVector(0));
		}
		else
		{
			pivot->SetAngularVelocityDrive(false, false);
			pivot->SetAngularDriveParams(0.0f, 0.0f, 0.0f);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif


void ARotatableActor::UpdateConstraintMode()
{
	float positiveCumulativeAngle = FMath::Abs(cumulativeAngle);
	if (currentRotationLimit <= 180.0f) UpdateConstraint(EConstraintState::Bellow180);
	else
	{
		if (positiveCumulativeAngle > 90.0f)
		{
			if (positiveCumulativeAngle < currentRotationLimit - 90.0f) UpdateConstraint(EConstraintState::Middle);
			else UpdateConstraint(EConstraintState::End);
		}
		else UpdateConstraint(EConstraintState::Start);
	}
}

void ARotatableActor::UpdateConstraint(EConstraintState state)
{
	// Setup the correct angular pivot for the yaw dependent on the variables initialized.
	if (limitedToRange)
	{
		switch (state)
		{
			// If the pivot is currently bellow a currentlimit of 360 degrees, the pivot will not need frame by frame updates.
			case EConstraintState::Bellow180:
			{
				// Update the current reference position.
				UpdateConstraintRefference(currentRotationLimit / 2);
				pivot->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, currentRotationLimit / 2);
			}
			break;
			// Setup the starting constrained variables. (Allow pivot to move between 0 and 90 if the limit is over 360 degrees).
			case EConstraintState::Start:
			{
				// Update the reference to be in the middle of 180 degrees.
				UpdateConstraintRefference(90.0f);
				// Set the swing limit to 180 degrees, so 90.
				pivot->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 90.0f);
			}
			break;
			// Setup the Middle constrained variables. (Setup a free pivot).
			case EConstraintState::Middle:
			{
				// Set the swing to free until the current angle is close enough to the end or start to re-enable the pivot.
				pivot->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Free, 0.0f);
			}
			break;
			// Setup the ending constrained variables. (Allow rotation up to the currentLimit if the limit is over 360 degrees).
			case EConstraintState::End:
			{
				// Get the local angle 90 away from the ending cumulative angle/limit.
				float localAngle;
				UKismetMathLibrary::FMod(currentRotationLimit, 360.0f, localAngle);
				float endingAngle = (360 + localAngle) - 90.0f;
				// Update the reference to be in the middle of 180 degrees.
				UpdateConstraintRefference(endingAngle);
				// Set the swing limit to 180 degrees, so 90.
				pivot->SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 90.0f);
			}
			break;
		}
		// Update the current state.
		constrainedState = state;
	}
}

void ARotatableActor::UpdateConstraintRefference(float constraintAngle)
{
	// Get the correct flipped value for the original yaw rotation.
	float angle = -constraintAngle;
	if (flipped) angle *= -1;
	
	// Get new offset.
 	FRotator rotationOffset = FRotator(0.0f, angle, 0.0f);
 	FVector rotationOffsetForward = rotationOffset.Quaternion().GetForwardVector();
 	FVector rotationOffsetRight = rotationOffset.Quaternion().GetRightVector();

 	// Offset the pivot reference to half way through the current constrained limit on the swing axis.
 	pivot->SetConstraintReferenceOrientation(EConstraintFrame::Frame2, rotationOffsetForward, rotationOffsetRight);	
}

void ARotatableActor::UpdateGrabbedRotation()
{
	// Get the correct world offset depending on current rotate mode.
	FVector handOffset;
	if (rotateMode == ERotateMode::TwistRotation) handOffset = grabLocation->GetComponentLocation();
	else handOffset = handRef->grabCollider->GetComponentLocation();

	// Get the current and original local angle of hand location.
	FVector currentWorldOffset = pivot->GetComponentTransform().InverseTransformPositionNoScale(handOffset);
	float currentAngleOfHand = UVRFunctionLibrary::GetYawAngle(currentWorldOffset);
	float originalAngleOfHand = UVRFunctionLibrary::GetYawAngle(handStartLocation);

	// Get the delta rotator normalized of the two angles to get the rotation between the two.
	FRotator rotationOffset = (FRotator(0.0f, currentAngleOfHand, 0.0f) - FRotator(0.0f, originalAngleOfHand, 0.0f)).GetNormalized();
	// Get the local rotation from the meshes starting world position as the component disables/re-enables physics so relative rotations are broken/disconnected.
	FRotator originalLocalRotation = UVRFunctionLibrary::GetRelativeRotationFromWorld(meshStartRotation, pivot->GetComponentTransform());	
	FRotator finalRotation = originalLocalRotation + rotationOffset;

	// Update the grab distance for this rotatable actor...
	UpdateHandGrabDistance();

	// Update the current yaw angle.
	currentYawAngle = finalRotation.Yaw;
	if (firstRun) lastYawAngle = currentYawAngle;
}

void ARotatableActor::UpdateHandGrabDistance()
{
// 	// Release from hand if the actualCumulativeAngle becomes too much greater than the cumulative angle.
// 	if (releaseOnOverRotation && limitedToRange && FMath::Abs(actualCumulativeAngle - cumulativeAngle) >= overRotationLimit)
// 	{
// 		handRef->ReleaseGrabbedActor();
// 		if (debug) UE_LOG(LogRotatable, Log, TEXT("The rotatable grabbed has released due to over rotation limit being exceeded at rotation %s."), *FString::SanitizeFloat(actualCumulativeAngle));
// 	}
// 	
// 	else
// 	{
// 		
// 	}

	// Check the distance from the hand grabbed position to the current hand position.
	if (rotateMode == ERotateMode::TwistRotation)
	{
		FVector currentHandExpectedOffset = pivot->GetComponentTransform().TransformPositionNoScale(twistingHandOffset);
		interactableSettings.handDistance = FMath::Abs((currentHandExpectedOffset - handRef->grabCollider->GetComponentLocation()).Size());
		if (debug) // Draw debugging information...
		{
			DrawDebugPoint(GetWorld(), currentHandExpectedOffset, 5.0f, FColor::Blue, true, 0.0f, 0.0f); // Draw expected hand position.
			DrawDebugPoint(GetWorld(), grabLocation->GetComponentLocation(), 5.0f, FColor::Red, true, 0.0f, 0.0f); // Draw direction position.
		}
	}
	else
	{
		interactableSettings.handDistance = FMath::Abs((grabLocation->GetComponentLocation() - handRef->grabCollider->GetComponentLocation()).Size());
		if (debug)
		{
			UE_LOG(LogRotatable, Log, TEXT("The distance between the hand and current grabbed rotatable is %s."), *FString::SanitizeFloat(interactableSettings.handDistance));
			DrawDebugPoint(GetWorld(), grabLocation->GetComponentLocation(), 5.0f, FColor::Blue, true, 0.0f, 0.0f); // Draw expected hand position.
		}
	}
	// Draw any debug information. (Draw the hands current position.)
	if (debug) DrawDebugPoint(GetWorld(), handRef->grabCollider->GetComponentLocation(), 5.0f, FColor::Green, true, 0.0f, 0.0f);
}

void ARotatableActor::UpdateRotatable(float DeltaTime)
{
	// Get the current yaw angle to update the cumulative angle of this rotatable.
	if (handRef)
	{
		// Get current yaw angle of hand depending on the grab mode.
		switch (rotateMode)
		{
		case ERotateMode::StaticRotation:
		case ERotateMode::StaticRotationCollision:
		case ERotateMode::TwistRotation:
			// Update the currentYawAngle to point towards the hand grabbed location.
			UpdateGrabbedRotation();
		break;
		case ERotateMode::PhysicsRotation:
			// Just update the current relative yaw angle from the worlds rotation as the values are only in need of being updated.
			currentYawAngle = UVRFunctionLibrary::GetRelativeRotationFromWorld(rotator->GetComponentRotation(), pivot->GetComponentTransform()).Yaw;
		break;
		}
	} 
	// If not grabbed, just update the current relative yaw angle from the worlds rotation.
	else currentYawAngle = UVRFunctionLibrary::GetRelativeRotationFromWorld(rotator->GetComponentRotation(), pivot->GetComponentTransform()).Yaw;

	// Get the current angle change to add/remove from the currentCumulativeAngle.
	float currentAngleChange;
	if (!firstRun) currentAngleChange = currentYawAngle - lastYawAngle;
	else firstRun = false;
	lastYawAngle = currentYawAngle; 

	// If the angle change is too big/small remove the error.
	if (currentAngleChange < -100.0f) currentAngleChange += 360.0f;
	else if (currentAngleChange > 100.0f) currentAngleChange -= 360.0f;
	float lastAngularVelocity = angularVelocity;
	angularVelocity = FMath::Abs(currentAngleChange) / DeltaTime;

	// Update the current cumulative angle and clamp it to its max and min rotation in the yaw axis.
	actualCumulativeAngle += currentAngleChange;
	cumulativeAngle = actualCumulativeAngle;
	if (flipped) cumulativeAngle = FMath::Clamp(cumulativeAngle, -currentRotationLimit, 0.0f);
	else cumulativeAngle = FMath::Clamp(cumulativeAngle, 0.0f, currentRotationLimit);

	// Update the constraints reference position and limits depending on the current cumulative angle. Only if the current limit is greater than 180 degrees.
	if (constrainedState != EConstraintState::Bellow180)
	{
		// Update revolution count...
		revolutionCount = cumulativeAngle / 360.0f;
		UpdateConstraintMode();
	}

	// Update the rotatable's audio and haptic events.
	UpdateAudioAndHaptics();

	// Handle locking functionality if it is enabled.
	// NOTE: Only check if there are locking points in the array.
	if (lockable && lockingPoints.Num() > 0) UpdateRotatableLock();
}

void ARotatableActor::UpdateAudioAndHaptics()
{
	// Play haptic effect if grabbed.
	if (handRef && rotatingHapticEffect)
	{
		if (!FMath::IsNearlyEqual(lastHapticFeedbackRotation, cumulativeAngle, hapticRotationDelay))
		{
			lastHapticFeedbackRotation = cumulativeAngle;
			float intensity = FMath::Clamp(angularVelocity / 250.0f, 0.0f, 2.0f);
			handRef->PlayFeedback(rotatingHapticEffect, intensity * hapticIntensityMultiplier);
		}
	}

	// If rotator cumulative angle is close to the start or end of the constraints bounds and velocity change is high enough, play haptic effect and impact sound.
	bool isAtConstraintLimit = FMath::IsNearlyEqual(cumulativeAngle, flipped ? -currentRotationLimit : currentRotationLimit, 2.0f);
	bool isAtConstraintStart = FMath::IsNearlyEqual(cumulativeAngle, 0.0f, 2.0f);
	if (isAtConstraintLimit || isAtConstraintStart)
	{
		// If angular velocity change is high enough.
		if (angularVelocity > 5.0f)
		{
			// Calculate intensity of effects.
			float intensity = FMath::Clamp(angularVelocity / 500.0f, 0.0f, 1.0f);

			// If grabbed play haptic effect.
			if (handRef) handRef->PlayFeedback(handRef->GetEffectsContainer()->GetFeedbackEffect("DefaultCollision"), intensity);

			// Play audio if set and not playing the impact sound currently.
			if (imapctSoundEnabled && impactSound)
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), impactSound, rotatorAudio->GetComponentLocation(), intensity);
				imapctSoundEnabled = false;
			}
		}
	}
	// Otherwise if not at the constraint start and impact audio is playing
	else if (!imapctSoundEnabled) imapctSoundEnabled = true; 

	// Play sound effect if sounds are set. Set volume of sound from angular change over time...
	if (rotatingSound)
	{
		// Get volume to play dragging/friction audio from the current angular velocity. The higher the louder the sound.
		float volume = FMath::Clamp(angularVelocity / 60.0f, 0.0f, 1.0f);
		float interpolatedVolume = FMath::FInterpTo(rotatorAudio->VolumeMultiplier, volume, GetWorld()->GetDeltaSeconds(), 10.0f);

		// If the rotator audio is playing update the volume from current angular velocity.
		if (rotatorAudio->IsPlaying()) rotatorAudio->SetVolumeMultiplier(interpolatedVolume);
		// Otherwise if audio is not playing start playing the audio.
		else
		{
			rotatorAudio->SetVolumeMultiplier(volume);
			rotatorAudio->Play();
		}
	}
}

void ARotatableActor::UpdateRotatableLock()
{
	// If grabbed only lock if lockedWhileGrabbed is enabled.
	if (handRef && !lockWhileGrabbed) return;

	// If currently cannot lock check if we can disable it from current distance from lastUnlockAngle.
	if (cannotLock)
	{
		// Set can lock to true if hand is no longer grabbing.
		if (!FMath::IsNearlyEqual(cumulativeAngle, lastUnlockAngle, unlockingDistance) || !handRef)
		{
			cannotLock = false;
			lastCheckedRotation = cumulativeAngle;
		}
	}
	// Otherwise look for closest locking angle and lock at that angle.
	else
	{
		// For each locking point check 
		float closestRotationFound = BIG_NUMBER;
		bool pointFound = false;
		for (float point : lockingPoints)
		{
			// If the last checked rotation to the current rotation has passed the current point and is smaller relative to the last checked rotation, lock at said point.
			bool hasPassedLock = lastCheckedRotation < cumulativeAngle ? UKismetMathLibrary::InRange_FloatFloat(point, lastCheckedRotation, cumulativeAngle) : // Last is smaller
																		 UKismetMathLibrary::InRange_FloatFloat(point, cumulativeAngle, lastCheckedRotation);  // Last is bigger
			if (hasPassedLock && point < closestRotationFound)
			{
				closestRotationFound = point;
				pointFound = true;
			}
		}

		// Lock to point if one was found. 
		if (pointFound) Lock(closestRotationFound);

		// Save last rotation checked.
		lastCheckedRotation = cumulativeAngle;
	}
}

void ARotatableActor::Lock(float lockingAngle)
{
	if (lockable)
	{
		// If lock haptic effect is enable and not null play on hand then release. Also play sound.
		if (handRef)
		{
			if (lockHapticEffect) handRef->PlayFeedback(lockHapticEffect, 1.0f);
			handRef->ReleaseGrabbedActor();
		}

		// Disable physics and set all angles to locked angle.
		if (rotatorAudio->IsPlaying()) rotatorAudio->FadeOut(0.2f, 0.0f);
		rotator->SetSimulatePhysics(false);
		FTimerDelegate timerDel;
		timerDel.BindUFunction(this, FName("InterpolateToLockedRotation"), lockingAngle);
		GetWorld()->GetTimerManager().ClearTimer(lockingTimer);
		GetWorld()->GetTimerManager().SetTimer(lockingTimer, timerDel, 0.01f, true);
		lockedAngle = lockingAngle;

		// If this lock cannot be grabbed while locked prevent this interactable from being grabbed.
		if (!grabWhileLocked) interactableSettings.canInteract = false;

		// Play locking sound. Only if there is a locking sound.
		if (lockSound)
		{
			float lockVolume = FMath::Clamp(FMath::Abs(angularVelocity) / 220.0f, 0.4f, 1.5f);
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), lockSound, rotatorAudio->GetComponentLocation(), lockVolume);
		}

		// Log.
		UE_LOG(LogRotatable, Warning, TEXT("The Rotatable %s was locked at rotation %f."), *GetName(), lockingAngle);

		// Now locked.
		locked = true;
		OnRotatableLock.Broadcast(lockingAngle);
	}
}

void ARotatableActor::Unlock()
{
	// Unlock this rotatable.
	if (lockable && locked)
	{
		rotator->SetSimulatePhysics(true);
		GetWorld()->GetTimerManager().ClearTimer(lockingTimer);
		if (!grabWhileLocked) interactableSettings.canInteract = true;
		lastUnlockAngle = lockedAngle;
		firstRun = true;
		cumulativeAngle = lockedAngle;
		actualCumulativeAngle = lockedAngle;
		currentYawAngle = UVRFunctionLibrary::GetRelativeRotationFromWorld(rotator->GetComponentRotation(), pivot->GetComponentTransform()).Yaw;
		lastYawAngle = currentYawAngle;
		cannotLock = true;
		locked = false;

		// Log.
		UE_LOG(LogRotatable, Warning, TEXT("The Rotatable %s was unlocked."), *GetName());
	}
}

void ARotatableActor::InterpolateToLockedRotation(float lockedRotation)
{
	// Interpolate to the locked rotation.
	float interolatingYawRotation = FMath::FInterpTo(cumulativeAngle, lockedRotation, GetWorld()->GetDeltaSeconds(), 15.0f);
	cumulativeAngle = interolatingYawRotation;
	actualCumulativeAngle = cumulativeAngle;
	FRotator newRotation = FRotator(0.0f, interolatingYawRotation, 0.0f);
	FRotator worldRotation = pivot->GetComponentTransform().TransformRotation(newRotation.Quaternion()).Rotator();
	rotator->SetWorldRotation(worldRotation);
	if (interolatingYawRotation == lockedRotation) GetWorld()->GetTimerManager().ClearTimer(lockingTimer);
}

void ARotatableActor::SetRotatableRotation(float newRotation, bool useTimeine, bool lockAtNewRotation)
{
	if (flipped ? newRotation >= rotationLimit && newRotation < 0.0f : newRotation >= 0.0f && newRotation <= rotationLimit)
	{
		// If in hand release.
		if (handRef) handRef->ReleaseGrabbedActor();
		// If locked unlock.
		if (locked) Unlock();
		// Disable physics while returning.
		if (rotator->IsSimulatingPhysics()) rotator->SetSimulatePhysics(false);

		// Return.
		lockOnSetRotation = lockAtNewRotation;
		if (useTimeine && returnTimeline)
		{
			// Return to the new rotation using the timeline and the return curve.
			isReturning = true;
			firstRun = true;
			returningRotation = newRotation;
			initialReturnRotation = cumulativeAngle;

			// Don't allow hand to interact if locked after return.
			if (lockOnSetRotation && !grabWhileLocked) interactableSettings.canInteract = false;

			// Start timeline.
			returnTimeline->PlayFromStart();
		}
		else
		{
			// Set rotation instantly.
			cumulativeAngle = newRotation;
			actualCumulativeAngle = cumulativeAngle;
			UpdateRotation();

			// If enabled lock at new rotation.
			if (lockOnSetRotation)
			{
				lockable = true;
				Lock(newRotation);
			}
			else
			{
				rotator->SetSimulatePhysics(true);
				firstRun = true;
			}
		}
	}
	else UE_LOG(LogRotatable, Warning, TEXT("Cannot return to the rotation %f as it is outside of the rotabal actor %s's rotation bounds."), newRotation, *GetName());
}

void ARotatableActor::Returning(float val)
{
	// Save the last rotation change velocity for locking impact sound.
	float currentAngleChange = 0.0f;
	if (!firstRun) currentAngleChange = currentYawAngle - lastYawAngle;
	else firstRun = false;
	lastYawAngle = currentYawAngle;
	angularVelocity = FMath::Abs(currentAngleChange) / GetWorld()->GetDeltaSeconds();

	// Slowly return to the angle set using the returnCurve.
	float newCumulativeAngle = FMath::Lerp(initialReturnRotation, returningRotation, val);
	cumulativeAngle = newCumulativeAngle;
	actualCumulativeAngle = cumulativeAngle;
	UpdateRotation();
}

void ARotatableActor::ReturningEnd()
{
	// Enable physics after return has ended.
	isReturning = false;
	rotator->SetSimulatePhysics(true);
	firstRun = true;

	// If enabled lock at new rotation.
	if (lockOnSetRotation)
	{
		lockable = true;
		Lock(returningRotation);
	}
}

void ARotatableActor::UpdateRotation()
{
	// Convert the local rotation into world rotation and apply to the rotatable along the local yaw axis. Also clamp this rotation if need be.
	FRotator updatedWorldRotation;

	// If limited to a range and values have been updated convert cumulative angle back to world rotation for mesh.
	FRotator currentRotation;
	if (limitedToRange)
	{
		// Convert the cumulative angle back into word rotation format.
		float actualAngle = UVRFunctionLibrary::GetAngleFromCumulativeAngle(cumulativeAngle);
		currentRelativeAngle = actualAngle;
		currentRotation = FRotator(0.0f, actualAngle, 0.0f);

		// Get the final world rotation from converted cumulative angle.
		updatedWorldRotation = pivot->GetComponentTransform().TransformRotation(currentRotation.Quaternion()).Rotator();
	}
	else
	{
		// Otherwise just use the current yaw angle...
		currentRotation = FRotator(0.0f, currentYawAngle, 0.0f);
		currentRelativeAngle = currentYawAngle;
		updatedWorldRotation = UVRFunctionLibrary::GetWorldRotationFromRelative(currentRotation, pivot->GetComponentTransform());
	}

	// Use correct mode of detecting collisions and Apply the final clamped rotation...
	switch (rotateMode)
	{
	case ERotateMode::PhysicsRotation:
	case ERotateMode::StaticRotation:
	case ERotateMode::TwistRotation:
		rotator->SetWorldRotation(updatedWorldRotation, false, nullptr, ETeleportType::TeleportPhysics);
		break;
	case ERotateMode::StaticRotationCollision:
		rotator->SetWorldRotation(updatedWorldRotation, true, nullptr, ETeleportType::TeleportPhysics);
		break;
	}
}

void ARotatableActor::SpawnGrabLocation(UPrimitiveComponent* toAttatch, FVector location)
{
	// Create a scene component that will be used to track the current rotation using get rotation from vector.
	FName uniqueCompName = MakeUniqueObjectName(this, USceneComponent::StaticClass(), FName("grabScene"));
	grabLocation = NewObject<USceneComponent>(this, uniqueCompName);
	grabLocation->SetMobility(EComponentMobility::Movable);
	grabLocation->RegisterComponent();

	// Finally attach the scene component to the controller so now we can track the relative roll depending on the angle grabbed.
	grabLocation->AttachToComponent(toAttatch, FAttachmentTransformRules::KeepWorldTransform);

	// Set its world location to this rotatable location with a 100 unit right offset.
	grabLocation->SetWorldLocation(location);
}

void ARotatableActor::GrabPressed_Implementation(AVRHand* hand)
{
	// If locked or returning, reset before grabbing.
	if (returnTimeline && returnTimeline->IsPlaying()) returnTimeline->Stop();
	if (locked) Unlock();

	// Save the hand reference while being grabbed.
	handRef = hand;

	// Call the on mesh grabbed delegate for blueprint use.
	OnMeshGrabbed.Broadcast(handRef);

	// Grab using the correct methods.
	switch (rotateMode)
	{
	case ERotateMode::TwistRotation:
		// Setup the grab locations for twisting the rotatable.
		SpawnGrabLocation(handRef->grabCollider, rotator->GetComponentLocation() + (rotator->GetRightVector() * 100.0f));
		twistingHandOffset = pivot->GetComponentTransform().InverseTransformPositionNoScale(handRef->grabCollider->GetComponentLocation());
		// Update grab distance variables.
		handStartLocation = pivot->GetComponentTransform().InverseTransformPositionNoScale(grabLocation->GetComponentLocation());
		break;
	case ERotateMode::StaticRotation:
	case ERotateMode::StaticRotationCollision:
		// Setup the grab positions where the hand has grabbed the rotatable.
		SpawnGrabLocation(rotator, handRef->grabCollider->GetComponentLocation());
		// Update grab distance variables.
		handStartLocation = pivot->GetComponentTransform().InverseTransformPositionNoScale(grabLocation->GetComponentLocation());
		break;
	case ERotateMode::PhysicsRotation:
		// Setup the grab positions where the hand has grabbed the rotatable.
		SpawnGrabLocation(rotator, handRef->grabCollider->GetComponentLocation());
		// Grab using only the hands physics handle...
		handRef->grabHandle->CreateJointAndFollowLocation(rotator, handRef->grabCollider, NAME_None,
			handRef->grabCollider->GetComponentLocation(), interactableSettings.grabHandleData);
		break;
	}

	// Update grab distance start location.
	handStartLocation = pivot->GetComponentTransform().InverseTransformPositionNoScale(grabLocation->GetComponentLocation());

	// Save the current rotation  of the mesh so it can be compared later.
	meshStartRotation = rotator->GetComponentRotation();

	// Disable physics while rotating if in the static modes...
	if (rotateMode != ERotateMode::PhysicsRotation) rotator->SetSimulatePhysics(false);
}

void ARotatableActor::GrabReleased_Implementation(AVRHand* hand)
{
	// Run the grab released delegate.
	OnMeshReleased.Broadcast(handRef);

	// Reset this rotatable back to its default state.
	switch (rotateMode)
	{
	case ERotateMode::TwistRotation:
	case ERotateMode::StaticRotation:
	case ERotateMode::StaticRotationCollision:
		// Reset physics if it was enabled. NOTE: Re-apply its angular velocity also.
		if (simulatePhysics)
		{
			rotator->SetSimulatePhysics(true);
			rotator->SetPhysicsLinearVelocity(handRef->handVelocity, false);
			rotator->SetAllPhysicsAngularVelocityInDegrees(handRef->handAngularVelocity, false);
		}
	break;
	case ERotateMode::PhysicsRotation:
		// Release this rotatable from the hands physics handle.
		handRef->grabHandle->DestroyJoint();
	break;
	}

	// Remove the grab location.
	if (grabLocation) grabLocation->DestroyComponent();
	
	// Reset grabbed variables.
	handRef = nullptr;
	firstRun = true;
	actualCumulativeAngle = cumulativeAngle;
}

void ARotatableActor::GrabbedWhileLocked_Implementation()
{
}

void ARotatableActor::Dragging_Implementation(float deltaTime)
{

}

void ARotatableActor::Overlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::Overlapping_Implementation(hand);
}

void ARotatableActor::EndOverlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::EndOverlapping_Implementation(hand);
}

FHandInterfaceSettings ARotatableActor::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void ARotatableActor::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}
