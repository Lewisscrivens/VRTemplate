// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Player/HandsInterface.h"
#include "Project/VRFunctionLibrary.h"
#include "Globals.h"
#include "RotatableStaticMesh.generated.h"

/* Define this components log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogRotatableMesh, Log, All);

/* Locking delegate. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRotatableMeshLock, float, angle, URotatableStaticMesh*, rotatable);

/* Declare classes used. */
class AVRHand;
class USceneComponent;

/* This is a custom static mesh component class that can be grabbed from VR and rotated around a given axis, Currently only the yaw axis.
 * Attach any mesh to this scene component with the tag grabbable to be able to grab this component from children... 
 * NOTE: This class is best used for smaller interactables that do not need to be physically constrained. Like door handles etc. (Cannot have collision with other objects that affect its rotation)
 * NOTE: Ensure any colliding grabbable components attatched to this scene component have the collision channel and tag "Grabbable". */
UENUM(BlueprintType)
enum class EStaticRotation : uint8
{
	Static UMETA(DisplayName = "StaticRotation", ToolTip = "Rotation follows original grabbed position."),
	StaticCollision UMETA(DisplayName = "StaticRotationCollision", ToolTip = "Rotation follows original grabbed position using sweep taking collisions into account."),
	Twist UMETA(DisplayName = "TwistRotation", ToolTip = "Grab and rotate using the physics handle, NOTE: will auto enable simulatePhysics variable on begin play."),
};

/* Rotatable static mesh component created for implementing rotating parts on an actor. */
UCLASS(ClassGroup = (Custom), Blueprintable, BlueprintType, PerObjectConfig, EditInlineNew)
class VRTEMPLATE_API URotatableStaticMesh : public UStaticMeshComponent, public IHandsInterface
{
	GENERATED_BODY()

public:

	/* Reference to the hand currently grabbing this component. Also can be used as a bool to check if this is in a hand. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Rotatable")
		AVRHand* handRef;

	/* What rotation mode is this rotatable static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		EStaticRotation rotateMode;

	/* The rotation will only update on locking angle. NOTE: Use-full for knobs for selection etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		bool lockOnlyUpdate;

	/* The constraint will attempt to fake physics using hand release velocity and faked restitution/friction values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		bool fakePhysics;

	/* Center the rotational limit to plus and minus "rotationLimit / 2" in either direction. Default is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		bool centerRotationLimit;

	/* Use the maxOverRotation float to determine when to release from the hand... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotatable")
		bool releaseOnOverRotation;

	/* Shows cumulativeAngle, revolutionCount and draws the current expected grabbed position as a blue point in the scene.
	 * Also draws the current grabbed position as green... NOTE: Twist mode has a red point which represents the hand position for rotation calculations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		bool debug;

	/* The fakedPhysics restitution when hitting objects. How much velocity to keep when bouncing off the walls of the constraint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"))
		float restitution;

	/* The fakedPhysics damping variable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable", meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.2", UIMax = "0.2"))
		float friction;

	/* The amount of rotation to add to the rotation at a time. NOTE: If 0 this is not used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable", meta = (ClampMin = "0.0", UIMin = "0.0"))
		float grabRotationStep;

	/* The max rotation limit. NOTE: 0 means its free to rotate to any given limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		float rotationLimit;

	/* The start rotation of the rotatable. Updates in real time within editor when changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		float startRotation;

	/* Max angle you can go past the constraint before it is released from the hand. Used to prevent unusual functionality. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rotatable")
		float maxOverRotation;

	/* Current amount of rotation done by this rotatable static mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|CurrentValues")
		float cumulativeAngle;

	/* Current amount of revolutions calculated from the cumulative angle. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|CurrentValues")
		int revolutionCount;

	/* Haptic effect to play on hand when locking while grabbed. NOTE: Will not play haptic effect if this in null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		UHapticFeedbackEffect_Base* lockHapticEffect;

	/* Sound to play when locking this rotatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		USoundBase* lockSound;

	/* Enable the rotatable to lock into certain positions. NOTE: On lock is a delegate that indicates which point has been locked to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking")
		bool lockable;

	/* Is currently locked. If enabled on begin play the rotatable will be locked in its start rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool locked;

	/* Can lock while grabbed? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool lockWhileGrabbed;

	/* Grab while locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool grabWhileLocked;

	/* Release from the hand when locked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool releaseWhenLocked;

	/* Interpolate to the locked angle, otherwise set rotation instantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		bool interpolateToLock;

	/* How close to a locking point before locking into rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable", UIMin = "0.0", ClampMin = "0.0"))
		float lockingDistance;

	/* How far in the yaw axis after an unlock before this rotatable can be locked again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable", UIMin = "0.0", ClampMin = "0.0"))
		float unlockingDistance;

	/* Rotatable locking points. NOTE: If you want the constraint to lock at (x) degrees add a float to the array set to (x)f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
		TArray<float> lockingPoints;

	/* The interface settings for hand interaction with this interactable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
		FHandInterfaceSettings interactableSettings;

	/* Scene component reference. NOTE: Spawned in when grabbed to keep track of grabbed position/rotation. */
	UPROPERTY(BlueprintReadOnly, Category = "Rotatable")
	USceneComponent* grabScene; 

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/* Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshGrabbed;

	/* Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshReleased;

	/* Locking delegate. Called when locked function is ran. */
	UPROPERTY(BlueprintAssignable)
	FOnRotatableMeshLock OnRotatableLock;

private:

	bool flipped;/* Is the rotation range negative, changes the maths used in certain areas of this class. */
	bool firstRun;/* Boolean to change last yaw angle on angle change to current angle. (So there is no angle change calculated on grabbing...) */
	bool cannotLock; /* Disables locking after grabbed when locked in position so it doesn't instantly release from the hand and lock again. */
	bool isLimited; /* Is limited to a range. */
	
	float lastYawAngle;/* last frames yaw angle. */
	float actualCumulativeAngle;/* un-clamped cumulative angle. (The amount the hand has cumulatively rotated) */
	float currentYawAngle;/* Current frames angle updated from the UpdateGrabbedRotation function. */
	float currentAngleChange, angleChangeOnRelease; /* Difference in rotation between frames in the yaw axis. */
	float lastUnlockAngle, lastCheckedRotation; /* The last unlocked angle to check against when grabbed. */
	float currentRotationLimit;/* Absolute rotationLimit. (Positive rotationLimit) */
	float currentLockedRotation;/* The current locked rotation of the rotatable if it is locked. */

	FRotator originalRelativeRotation;/* Save the original rotation of the rotatable mesh used to add or subtract cumulative rotation from. */
	FVector handStartLocation, twistingHandOffset; /* Save the start locations to help calculate offsets. */
	FRotator meshStartRelative; /* Relative rotation of this component around the constrained axis. */
	FTimerHandle lockingTimer; /* Locking interpolation timer. */

protected:

	/* Level start. */
	virtual void BeginPlay() override;

#if WITH_EDITOR
	/* Post edit change. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;
#endif

	/* Check if the current cumulative angle is close enough to a locking position, if so lock the rotatable. */
	void UpdateRotatableLock();

public:

	/* Constructor. */
	URotatableStaticMesh();

	/* Updates the current rotational values and the physical rotation that uses those values. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/* If grabbed this function will update the grabbed angle (currentYawAngle) in the yaw using original grab offsets and trigonometry. */
	void UpdateGrabbedRotation();

	/* Updates the rotational values used in update rotation for keeping rotation offset from hand grabbed location */
	void UpdateRotatable(float DeltaTime);

	/* Updates and clamps both the cumulative angle and revolution count.
	 * @Param increaseAmount, the amount the cumulative angle has increased/decreased. */
	void IncreaseCumulativeAngle(float increaseAmount);

	/* iS THE VALUE WITHIN A RANGE. Taken from kismet math library. */
	bool InRange(float Value, float Min, float Max, bool InclusiveMin = true, bool InclusiveMax = true);

	/* Lock this rotatable static mesh at the specified locking angle. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable")
	void Lock(float lockingAngle);

	/* Unlock this rotatable static mesh. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable")
	void Unlock();

	/* Interpolation function for interpolating the rotation to a given rotation. */
	UFUNCTION(BlueprintCallable)
	void InterpolateToLockedRotation(float lockedRotation);

	/* Update the current angle in this components yaw axis. */
	void UpdateRotation(float DeltaTime = 0.0f);

	/* Apply physical rotation from last force of the hand on release, Handled in tick.... 
	 * Also handles restitution values. */
	void UpdatePhysicalRotation(float DeltaTime);

	/* Spawns a scene component to keep track of certain rotations/locations relative to the hand and this component.
	 * @Param connection, the primitive component to connect the newly spawned scene component to.
	 * @Param worldLocation, the world location to spawn and attach the scene component at... */
	void CreateSceneComp(USceneComponent* connection, FVector worldLocation);

	/* Return the correct transform to use as this components parent.... */
	FTransform GetParentTransform();

	/* Update the hands release distance variables. */
	void UpdateHandGrabDistance();

	/* Implementation of the hands interface. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;

	/*  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};