// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Project/VRFunctionLibrary.h"
#include "Player/HandsInterface.h"
#include "Globals.h"
#include "GrabbableSkelMesh.generated.h"

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogGrabbableSkelComp, Log, All);

/* Declare classes used. */
class AVRHand;
class UAnimationAsset;
class UPrimitiveComponent;
class UVRPhysicsHandleComponent;
class USoundBase;
class UHapticFeedbackEffect_Base;

/* Grabbable skeletal mesh which will control grabbing bones and teleporting with a physics handle grabbed component. */
UCLASS(ClassGroup = (Custom), config = Engine, editinlinenew, Blueprintable, BlueprintType)
class VRTEMPLATE_API UGrabbableSkelMesh : public USkeletalMeshComponent, public IHandsInterface
{
	GENERATED_BODY()

public:

	/* Snapped animation to play. When snapped into a snappable component. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Grabbable")
	UAnimationAsset* snappedAnimation;

	/* Storage value of the player controller. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Grabbable")
	AVRHand* handRef;

	/* Other hand reference to second hand to grab this component if twoHandedGrabbing is enabled in the interface settings. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Grabbable")
	AVRHand* otherHandRef;

	/* Storage of the current bone to grab for this actor component. NOTE: If NAME_None then it will find the closest bone and grab from their.
	 * NOTE: If two handed grab mode is enabled this will only work for the first hand to grab this component. Second hand will grab the closest bone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FName boneToGrab;

	/* Storage of the bone to snap this component from, when overlapping with a snappable actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FName boneToSnap;

	/* Should the physics joint when created in physics handle modes, be placed at the center of the mesh or at the grabbing controller position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool centerPhysicsJoint;

	/* Increase inertia tensor scale of the current grabbed body to the value in intertiaBoneValues. 
	 * NOTE: Prevents soft constraint mode of the physics handle not working correctly due smaller bones not keeping hand position correctly. 
	 * By default bones are set to 2.2 inertia scale if not in array. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool adjustInertiaFromArray;

	/* Check for collisions and switch between soft and stiff constraint modes depending on result. 
	 * NOTE: Disables hand grab distance as it just flys out of the hand on collision due to physX calculation error. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Collisions")
	bool checkCollision;

	/* The time it takes to return to the hand via lerp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Collisions")
	float timeToLerp;

	/* Used for snap to hand so the rotation for each object can be adjusted to an offset. NOTE: If snapToHand is false then this will not be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FRotator snapToHandRotationOffset;

	/* Used for snap to hand so the location for each object can be adjusted to an offset. NOTE: If snapToHand is false then this will not be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FVector snapToHandLocationOffset;

	/* The haptic feedback intensity multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
	float hapticIntensityMultiplier;

	/* The haptic feedback collision effect to play override. If null it will use the default collision feedback in the AVRHands grabbing this grabbable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
	UHapticFeedbackEffect_Base* collisionFeedbackOverride;

	/* Sound to play on collision if null it will use the AVRHands default collision sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
	USoundBase* impactSoundOverride;

	/* Show any debugging information for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool debug;

	/* The interfaces interactable settings for how to interact with VR controllers/hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FHandInterfaceSettings interactableSettings;

	//////////////////////////
	//	  Grab Pointers     //
	//////////////////////////


	/* Holds ignored actors for when performing traces in the collision check functions. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	TArray<AActor*> ignored;

	/* Stored impact sound pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	USoundBase* impactSound;

	/* Stored collision haptic feedback pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	UHapticFeedbackEffect_Base* collisionFeedback;

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/* Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbedComponent OnMeshGrabbed;

	/* Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbedComponent OnMeshReleased;

private:

	FName otherBoneToGrab;/* Saved other bone name for two handed mode. */
	FVector originalInteriaScale; /* Saved value for the original physics assets inertia value. */
	FVector originalRelativePickupOffset; /* Store the original relative pickup offset to the hand for re-attachment after collisions. */
	FRotator originalRelativePickupRotation; /* Store the original relative pickup rotation offset from the hands rotation to the current grabbed bones rotation in the world. */
	FVector worldPickupOffset; /* Save the current world pickup location offset for collision check. */
	FRotator worldRotationOffset; /* Current desired world rotation offset. */
	FVector originalIntertia;/* Saved original inertia of the current grabbed body so it can be reset when released. */
	FTransform originalBoneOffset; /* The original offset of the joint target location relative to the bone. */
	FTimerHandle lastRumbleHandle;/* Ran after the rumble delay of the playing haptic feedback event if there is one. */

	float lastHitTime, lastImpactSoundTime, lastRumbleIntensity, lastZ;// Hit event time events.
	float lastHandGrabDistance; /* Timer variables to ensure that the grabbable has not hit anything for more than a given time. */
	float originalMass; /* Saved value for the original mass of the current boneToGrab from this components physics asset. */
	float lerpStartTime; /* The game time that the lerp should end. */
	bool grabbed; /* Is the grabbable in a hand. */
	bool grabFromClosestBone; /* Is grab from closest bone enabled or not. */
	bool softHandle; /* soft constraint enabled when colliding with other objects. */
	bool lerping; /* Is lerping from last collided position back to the hand grab world offset and rotation offset. */
	

protected:

	/* Level Start. */
	virtual void BeginPlay() override;

	/* ENABLE SOFTER HANDLING FOR WHEN COLLIDING... */
	void ToggleSoftPhysicsHandle(bool on);

	/* Lerp the current grabbed bone position to the grabbed bone offset location from the hand after a collision. */
	void LerpingBack(float deltaTime);

	/* Used to start and stop/initiate the lerp of this component back to the grabbed offset location/rotation. */
	void ToggleLerping(bool on);

public:

	/* Constructor. */
	UGrabbableSkelMesh();

	/* Hit event for this skeletal component. */
	UFUNCTION(BlueprintCallable)
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/* Timer event to reset the last rumble intensity after a sound or haptic effect has finished playing. */
	UFUNCTION(Category = "Timer")
	void ResetLastRumbleIntensity();

	/* Called to attach the grabbable to the hand using physics handle. */
	void PickupPhysicsHandle(AVRHand* hand);

	/* Called to detach the grabbable from the hands collision physics handle. */
	void DropPhysicsHandle(AVRHand* hand);

	/* Updates the component body's inertia tensor scale, mass values etc. when grabbed. This is all reset in grab release. */
	FName UpdateComponentsClosestBody(AVRHand* hand);

	/* Returns whether the mesh is currently in a hand */
	bool IsMeshGrabbed();

	/* Get the current grabbed transform. */
	UFUNCTION(BlueprintCallable)
	FTransform GetGrabbedTransform();

	/* Was this grabbable skeletal mesh recently hit. */
	UFUNCTION(BlueprintCallable)
	bool GetRecentlyHit();

	/* Implementation of the hands interface. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;
	virtual void Teleported_Implementation() override;

	/*  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};
