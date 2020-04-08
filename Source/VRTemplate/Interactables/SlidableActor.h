// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/HandsInterface.h"
#include "Project/VRFunctionLibrary.h"
#include "Globals.h"
#include "SlidableActor.generated.h"

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogSlidableActor, Log, All);

/* Declare classes used. */
class USceneComponent;
class UPrimitiveComponent;
class UArrowComponent;
class UPhysicsConstraintComponent;
class USimpleTimeline;
class UCurveFloat;
class AVRHand;
class UHapticFeedbackEffect_Base;
class USoundBase;
class UAudioComponent;

/* Different slidable modes.*/
UENUM(BlueprintType)
enum class ESlidableMode : uint8
{
	GrabStatic UMETA(DisplayName = "GrabStatic", ToolTip = "Disable physics while grabbed and ignore collisions."),
	GrabStaticCollision UMETA(DisplayName = "GrabStaticCollision", ToolTip = "Disable physics while grabbed and used sweep to prevent overlapping constraints."),
	GrabPhysics UMETA(DisplayName = "GrabPhysics", ToolTip = "Grab with physics handle on the hand, less accurate but better when full physical interactions are needed."),
};

/* Main slidable  class without an initialized slidingMesh component. Used to create virtual reality drawers etc. */
UCLASS(Blueprintable)
class VRTEMPLATE_API ASlidableActor : public AActor, public IHandsInterface
{
	GENERATED_BODY()

public:

	/* Root component of the slidable. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneComponent* root;

	/* Constrained sliding component. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UPrimitiveComponent* slidingMesh;

	/* Slidables audio component to play locking and dragging sounds.  */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UAudioComponent* slidableAudio;

	/* Arrow to point in the X-axis direction. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UArrowComponent* slidableXDirection;

	/* Physics constraint to keep collisions active. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UPhysicsConstraintComponent* pivot;

	/* Reference to the hand currently grabbing this component. Also can be used as a bool to check if this is in a hand. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Slidable")
	AVRHand* handRef;

	/* The current Slidable mode, mainly used to describe how this Slidable is grabbed and interactables with collisions and other physics objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	ESlidableMode currentSlidableMode;

	/* Component name to grab on this slidable actor. If NAME_None grab the slidingMesh component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|GrabPhysics")
	FName compToGrab;

	/* The bone to grab the slidable by. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	FName boneToGrab;

	/* Is the physics constraint active on the mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	bool simulatePhysics;

	/* The linear damping value but it can really be seen as friction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	float friction;

	/* Amount of velocity to keep after bouncing off something like the end of the constraint for example. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	float restitution;

	/* The max translation limit for each axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	FVector sliderLimit;

	/* Current slider position relative to the actor position. Needed as simulating physics breaks parent relative locations. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Slidable")
	FVector sliderRelativePosition;

	/* Position the constraint (pivot components) reference position at the pivot components location. (Acts more like regular constraint this way) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	bool centerConstraint;

	/* Reference position offset made visible to blueprint for finding constraint origin point from actors root location. */
	UPROPERTY(BlueprintReadOnly, Category = "Slidable|Constraint")
	FVector refferenceOffset;

	/* Ignored Actors... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|Constraint")
	TArray<AActor*> ignoredActors;

	/* Sound to play when sliding this slidable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|Sounds", meta = (EditCondition = "lockable"))
	USoundBase* slidingSound;

	/* Sound to play when slidable hits its constraint limits. NOTE: Default collision haptic feedback is used from the AVRHand grabbing this component. Sound will not play if null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|Sounds")
	USoundBase* impactSound;

	/* Intensity multiplier for the sound effect on this slidable actor on an impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|HapticEffects", meta = (UIMin = "0.1", ClampMin = "0.1"))
	float impactSoundIntensity;

	/* Haptic effect to play on hand when grabbed and rotating this component every hapticRotationDelay. NOTE: Will not play haptic effect if this in null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|HapticEffects")
	UHapticFeedbackEffect_Base* slidingHapticEffect;

	/* Haptic effect override for impacts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|HapticEffects")
	UHapticFeedbackEffect_Base* impactHapticEffect;

	/* Play sliding haptic effect/Sound if not null every hapticSlideDelay units. NOTE: If set to 0.0f it will play the haptic effect constantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|HapticEffects", meta = (UIMin = "0.1", ClampMin = "0.1"))
	float hapticSlideDelay;

	/* Intensity multiplier for the haptic effect on this slidable actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|HapticEffects", meta = (UIMin = "0.1", ClampMin = "0.1"))
	float hapticIntensity;

	/* Debug boolean variable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable|Constraint")
	bool debug;

	/* The interfaces interactable settings for how to interact with VR controllers/hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	FHandInterfaceSettings interactableSettings;

	/* Target component used to check offset from handRef etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Slidable")
	UPrimitiveComponent* targetComponent; 

	/* Used for physics grabbing, if there is a different component that requires grabbing. */
	UPROPERTY(BlueprintReadOnly, Category = "Slidable")
	UPrimitiveComponent* componentToGrab;

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/* Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshGrabbed;

	/* Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshReleased;

	/* Grabbed while locked to the hand, useful for triggering further events while grabbed. */
	UPROPERTY(BlueprintAssignable)
	FGrabbedLocked OnGrabbedWhileLocked;

	/* Released while locked to the hand, useful for triggering further events while grabbed. */
	UPROPERTY(BlueprintAssignable)
	FGrabbedLocked OnReleasedWhileLocked;

private:

	FTransform originalTransform;/* Original actor transform saved at begin play. */
	FVector originalGrabOffset; /* Original grab offset of the hand to the Slidable mesh to prevent jumping when this is grabbed. */
	FVector currentVelocity; /* velocity while grabbed to be re-applied on release. Also last frames position used to calculate the velocity. */
	FVector constraintOffset;
	FVector currentPosition, lastPosition; /* Variables to keep track of the current velocity. */
	FVector lastHapticFeedbackPosition; /* The last position haptic feedback was performed on. */

	bool limitedToRange; /* Is this interactable limited/constrained to a range. */
	bool xLimited, yLimited, zLimited; /* Boolean values for weather a certain axis is active in the constraint... */
	bool imapctSoundEnabled;
	int activeAxis; /* Used to determine if there is only a single axis. */

protected:

	/* The absolute slider limit on both the X and Y axis. */
	FVector currentSliderLimit; 

	/* Level start. */
	virtual void BeginPlay() override;

	/* Update the positive limit for given values. */
	void UpdateLimit(float originalLimit, float& posLimit);

	/* Return the correct reference offset for the given axis, either return half or minus half. */
	float GetRefferenceOffset(float axis);

	/* Setup this constraints values to the mesh... */
	void SetupConstraint();

	/* Return booleans of if the slidableMesh is within its constrained X, Y and Z axis limits. */
	void InRange(bool& inRangeXPointer, bool& inRangeYPointer, bool& inRangeZPointer);

#if DEVELOPMENT
	/* Visualise the constraints bounds. */
	void ShowConstraintBounds();
#endif

public:

	/* Constructor. */
	ASlidableActor();

	/* Used to update the current values for the constraints position if linear velocity is greater than 0. */
	virtual void Tick(float DeltaTime) override;

	/* Checks the mesh is within the bounds of the constraint at all times. If not the mesh is moved back to the closest constrained position. */
	void CheckConstraintBounds();

	/* Clamp the given position within the constraint. */
	FVector ClampPosition(FVector position);

	/* Update the slidables position, starts by running the UpdateValues function to get hand positions etc. */
	void UpdateSlidable(float DeltaTime);

	/* Update the audio for this slidable when sliding or impacting the constrained limits. */
	UFUNCTION(Category = "Audio")
	void UpdateAudioAndHaptics();

	/* Implementation of the hands interface. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;
	virtual void GrabbedWhileLocked_Implementation() override;
	virtual void ReleasedWhileLocked_Implementation() override;

	/*  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};
