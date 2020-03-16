// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/HandsInterface.h"
#include "VR/VRFunctionLibrary.h"
#include "Globals.h"
#include "RotatableActor.generated.h"

/* Declare this classes logging category. */
DECLARE_LOG_CATEGORY_EXTERN(LogRotatable, Log, All);

/* Locking delegate. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRotatableLock, float, angle);

/* Declare classed used. */
class USceneComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class UArrowComponent;
class UPhysicsConstraintComponent;
class UHapticFeedbackEffect_Base;
class USimpleTimeline;
class UCurveFloat;
class AVRHand;
class AVRPawn;
class USoundBase;
class UAudioComponent;

/* Different rotational grabbing methods. */
UENUM(BlueprintType)
enum class ERotateMode : uint8
{
	StaticRotationCollision UMETA(DisplayName = "StaticRotationCollision", ToolTip = "When grabbed rotate using trigonometry calculations and sweep on setRotation to prevent overlaps."),
	StaticRotation UMETA(DisplayName = "StaticRotation", ToolTip = "When grabbed rotate using trigonometry calculations without checking for overlap events."),
	PhysicsRotation UMETA(DisplayName = "PhysicsRotation", ToolTip = "Grab and rotate using the physics handle, NOTE: will auto enable simulatePhysics variable on begin play."),
	TwistRotation UMETA(DisplayName = "TwistRotation", ToolTip = "When grabbed twisting the controller in the relative grab location will twist/rotate this rotatable. (Using trigonometry calculations)")
};

/* Different constrained positions, used to visualize what state the constraint is in. */
UENUM(BlueprintType)
enum class EConstraintState : uint8
{
	Bellow180,
	Start,
	Middle,
	End
};

/* Mixture between physics constraint and static rotation to allow angles greater than 180 degrees.
 * NOTE: Pivot in place needs to have collision enabled with everything ignored for it to work correctly when created physics bodies for this constrained rotatable.
 * NOTE: Grabbable rotating component must be a child of the rotatable box component and have a "Grabbable" tag.
 * NOTE: This class is best suited to things that need a rotation limit above 180 degrees and need physical collisions like a door etc. */
UCLASS()
class NINETOFIVE_API ARotatableActor: public AActor, public IHandsInterface
{
	GENERATED_BODY()
	
public:	

	/* The component to rotate. NOTE: Do not change the pitch or roll rotation as it will be reset. */
	UPROPERTY(BlueprintReadOnly)
	UBoxComponent* rotator;

	/* Arrow to point to the start rotation of this rotatable. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UArrowComponent* rotationStart;

	/* Arrow to point in the rotation axis direction that the mesh is current rotating around... */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UArrowComponent* rotationAxis;

	/* Physics constraint to keep collisions active and rotate.  */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UPhysicsConstraintComponent* pivot;

	/* Rotator audio component to play locking and dragging sounds.  */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UAudioComponent* rotatorAudio;

	/* Reference to the hand currently grabbing this component. Also can be used as a bool to check if this is in a hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Rotatable")
	AVRHand* handRef;

	/* What rotation mode is this rotatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
	ERotateMode rotateMode;

	/* Is the physics constraint active on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
	bool simulatePhysics;

	/* Should release actor when rotation over becomes too great. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
	bool releaseOnOverRotation;

	/* Curve to drive the timeline when returning to certain rotations smoothly. NOTE: Used in the SetRotatableRotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Returning")
	UCurveFloat* returnCurve;

	/* Haptic effect to play on hand when locking while grabbed. NOTE: Will not play haptic effect if this in null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|HapticEffects", meta = (EditCondition = "lockable"))
	UHapticFeedbackEffect_Base* lockHapticEffect;

	/* Haptic effect to play on hand when grabbed and rotating this component every hapticRotationDelay. NOTE: Will not play haptic effect if this in null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|HapticEffects")
	UHapticFeedbackEffect_Base* rotatingHapticEffect;

	/* Play rotating haptic effect if not null every hapticRotationDelay degrees. NOTE: If set to 0.0f it will play the haptic effect constantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|HapticEffects", meta = (UIMin = "0.1", ClampMin = "0.1"))
	float hapticRotationDelay;

	/* Multiply the intensity of the current haptic effect by the intensity multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|HapticEffects", meta = (UIMin = "0.01", ClampMin = "0.01"))
	float hapticIntensityMultiplier;

	/* Sound to play when rotating this rotatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Sounds")
	USoundBase* rotatingSound;

	/* Sound to play when locking this rotatable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Sounds", meta = (EditCondition = "lockable"))
	USoundBase* lockSound;

	/* Sound to play when rotatable hits its constraint limits. NOTE: Default collision haptic feedback is used from the AVRHand grabbing this component. Sound will not play if null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Sounds")
	USoundBase* impactSound;

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

	/* How close to a locking point before locking into rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable", UIMin = "0.0", ClampMin = "0.0"))
	float lockingDistance;

	/* How far in the yaw axis after an unlock before this rotatable can be locked again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable", UIMin = "0.0", ClampMin = "0.0"))
	float unlockingDistance;

	/* Rotatable locking points. NOTE: If you want the constraint to lock at (x) degrees add a float to the array set to (x)f. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable|Locking", meta = (EditCondition = "lockable"))
	TArray<float> lockingPoints;

	/* The start rotation of the rotatable. Updates in real time within editor when changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
	float startRotation;

	/* The max rotation limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
	float rotationLimit;

	/* How much rotation over the limit is needed to release from the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable", meta = (EditCondition = "releaseOnOverRotation"))
	float overRotationLimit;

	/* Friction when rotating, uses angular motors on the pivot constraint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable", meta = (EditCondition = "simulatePhysics", UIMin = "0.0", ClampMin = "0.0"))
	float friction;

	/* Current frames yaw angle relative to the pivot calculated from the cumulative angle and applied via world space to the rotator components. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|RotatableValues")
	float currentRelativeAngle;

	/* The current amount of times that the rotator has done a full rotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|RotatableValues")
	float cumulativeAngle;

	/* The current amount of times that the rotator has done a full rotation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rotatable|RotatableValues")
	int revolutionCount;

	/* Is the rotatbale still returning to a specified angle. */
	UPROPERTY(BlueprintReadOnly, Category = "Rotatable")
	bool isReturning;

	/* Shows cumulativeAngle, revolutionCount and draws the current expected grabbed position as a blue point in the scene.
	 * Also draws the current grabbed position as green... NOTE: Twist mode has a red point which represents the hand position for rotation calculations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
	bool debug;

	/* The interface settings for hand interaction with this interactable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rotatable")
	FHandInterfaceSettings interactableSettings;

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/* Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshGrabbed;

	/* Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshReleased;

	/* Rotatable has been locked to a point in the lockingPoints array. */
	UPROPERTY(BlueprintAssignable)
	FOnRotatableLock OnRotatableLock;

private:

	FVector handStartLocation; /* Save the start locations to help calculate offsets. */
	FVector twistingHandOffset; /* Original distance of the hand to the grabbed rotatable. */
	FRotator meshStartRotation; /* Save the start rotation to help calculate offsets. */
	FRotator meshOriginalRelative; /* Get original relative rotation of the mesh. */

	float lockedAngle; /* The angle this rotatable was locked at. */
	float currentYawAngle;;/* Current frames yaw angle. */
	float lastYawAngle;/* last frames yaw angle. */
	float actualCumulativeAngle;/* Actual cumulative angle the hand has rotated, used to keep track of when to rotate with the hand. */
	float currentRotationLimit;/* Absolute rotationLimit. (Always positive.) */
	float lastUnlockAngle; /* Last angle to unlock at. Used for determining if the user is rotating away from a lock so do not lock when grabbed. */
	float returningRotation; /* The rotation to return to. */
	float initialReturnRotation; /* Initial cumulative angle to lerp from when returning to a specified angle. */
	float lastHapticFeedbackRotation; /* The last rotation haptic feedback was performed on. */
	float angularVelocity; /* The current angular velocity of the rotator when not simulating physics. */
	float lastCheckedRotation; /* The last cumulative angle that was checked by the update rotatable locking function. */

	bool firstGrab;
	bool firstRun;
	bool flipped; /* Is the range negative. */
	bool limitedToRange; /* Is the rotatable limited within a range. */
	bool cannotLock; /* Variable to prevent lockable from locking after it was unlocked. */
	bool lockOnSetRotation; /* Should lock when rotation has been set. */
	bool imapctSoundEnabled;

	USceneComponent* grabLocation; /* A scene component that is spawned and attatched to the hand with an offset right of the twistable relatively. */
	EConstraintState constrainedState; /* Current state of the constraint, this is used to keep track of how the constrained rotatable should act at different rotations. */
	FTimerHandle lockingTimer; /* Locking interpolation timer. */
	USimpleTimeline* returnTimeline; /* Simple timeline class to hold all timeline functions etc. */

protected:

	/* Level start. */
	virtual void BeginPlay() override;

	/* Update the constraint mode from the current cumulative angle. */
	void UpdateConstraintMode();

	/* Change the constraints current state, used to allow cumulative rotations while using the physics constraint. (As it is limited to 360 and has many other issues.)
	 * @Param state, state of constraint (ENUM) to swap to.	*/
	void UpdateConstraint(EConstraintState state);

	/* Update the constraints reference position for the swing axis. NOTE: Should be half of the currentLimit.
	 * @Param constraintAngle, the angle in the yaw-axis of the mesh that the constraints reference should be set to. */
	void UpdateConstraintRefference(float constraintAngle);

	/* If grabbed this function will update the grabbed angle (currentYawAngle) in the yaw using original grab offsets and trigonometry.
	 * Otherwise it will return the current local rotation which is calculated from the world and this actors original world transform. */
	void UpdateGrabbedRotation();

	/* Update the current distance between this rotatable current offset compared to the original grab offset  */
	void UpdateHandGrabDistance();

	/* Updates the rotational values used in update rotation for keeping rotation offset from hand grabbed location, also updates the constraint
	 * from the cumulative angle. Also takes care of haptic effects and rotatable sounds while being dragged etc.
	 * NOTE: Ran while grabbed OR in this actors tick function when movement is detected. */
	void UpdateRotatable(float DeltaTime);

	/* Update the rotatables audio events and haptic feedback events if grabbed while rotating or impacting the constraint bounds. */
	void UpdateAudioAndHaptics();

	/* Update the locking functionality if enabled for this rotatable. Ran from UpdateRotatable. */
	void UpdateRotatableLock();

	/* Update the current angle in the yaw axis from the cumulative angle calculated in UpdateRotatable in this actors Ticking function. */
	void UpdateRotation();

	/* Spawn the grabbed location at the given location and attach to the given component to keep track of hand movement...
	 * @Param toAttach, the component to attach the spawned scene component to.
	 * @Param location, the location in world space to spawn the scene component. */
	void SpawnGrabLocation(UPrimitiveComponent* toAttach, FVector location);

public:

	/* Constructor. */
	ARotatableActor();

	/* Updates the current rotational values and the physical rotation that uses those values. NOTE: Only ran when not grabbed. */
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	/* Post edit change property for updating this class from any default settings changed while within editor. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/* Callback function for the timeline.
	 * @Param newRotation, The new rotation to set.
	 * @Param useTimeine, Adjust rotation using a timeline and curve or instantly set the rotation. 
	 * @Param lockAtNewRotation, Should lock at new rotation. Enables lockable variable if true.
	 * NOTE: If lockAtNewRotation is true and grabWhileLocked is false this rotatable will not be grabbable while returning and locked. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable")
	void SetRotatableRotation(float newRotation, bool useTimeine = false, bool lockAtNewRotation = false);

	/* Callback function for the timeline.
	 * @Param val, current curve float value to drive rotation speeds while lerping. */
	UFUNCTION(Category = "Rotatable|Returning")
	void Returning(float val);

	/* End function for the timeline, called when the Returning function is over or ended. */
	UFUNCTION(Category = "Rotatable|Returning")
	void ReturningEnd();

	/* Rotatable locked rotation interpolation function. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable|Locking")
	void InterpolateToLockedRotation(float lockedRotation);

	/* Lock this rotatable if it is not already locked. 
	 * @Param lockingAngle, The angle to lock the rotatable at. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable|Locking")
	void Lock(float lockingAngle);

	/* Unlock this rotatable if it is locked. */
	UFUNCTION(BlueprintCallable, Category = "Rotatable|Locking")
	void Unlock();

	/* Implementation of the hands interface. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void GrabbedWhileLocked_Implementation() override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;

	/*  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};
