// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/HandsInterface.h"
#include "Globals.h"
#include "GrabbableActor.generated.h"

/** Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogGrabbable, Log, All);

/** Declare classes used. */
class UStaticMeshComponent;
class UVRPhysicsHandleComponent;
class AVRHand;
class USoundBase;
class UAudioComponent;
class UHapticFeedbackEffect_Base;

/** Declare the Physics and collisions state changed delegate. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPhysicalStateChanged, bool, physicsOn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCollisionChanged, ECollisionResponse, newCollisionResponse);

/** Grabbing modes. */
UENUM(BlueprintType)
enum class EGrabMode : uint8
{
	AttatchTo UMETA(DisplayName = "AttatchTo", ToolTip = "Uses only attatch to and does not detect collision."),
	PhysicsHandle UMETA(DisplayName = "PhysicsHandle", ToolTip = "Grab grabbableMesh with the physics handle."),
	AttatchToWithPhysics UMETA(DisplayName = "AttatchToWithPhysics", ToolTip = "Grabs with attach to mode initially, while colliding grab mode is switched to physics handle (NOTE: Uses Grabbable trace channel)."),
};

/** Two handed second grabbing modes. */
UENUM(BlueprintType)
enum class ESecondGrabMode : uint8
{
	PhysicsHandle UMETA(DisplayName = "PhysicsHandle", ToolTip = "Uses only physics handle constantly while grabbed with more than one hand to assume where the component would be."),
	TrackRotation UMETA(DisplayName = "TrackRotation", ToolTip = "Uses the second hand to grab this actor as a target rotation to face. NOTE: Good for weapons etc."),
};

/** Different return to hand after collision modes. */
UENUM(BlueprintType)
enum class EReturnMode : uint8
{
	Default UMETA(DisplayName = "Default", ToolTip = "When the current grab offset is not overlapping anything the grabbable will lerp back to the hand."),
	ClearPathToHand UMETA(DisplayName = "ClearPathToHand", ToolTip = "Will return to the hand when current grab offset has no overlaps and there is a clear path to lerp back to the hand."),
};

/** Collision checking type. */
UENUM(BlueprintType)
enum class EOverlapType : uint8
{
	Simple UMETA(DisplayName = "Simple", ToolTip = "The collision function will use the actors bounds to get if there is any overlaps. NOTE: Better for performance."),
	Complex UMETA(DisplayName = "Complex", ToolTip = "The collision function will use each individual components bounds to get if there is any overlap."),
};

/** Struct to hold a hands initial and current grabbing information/variables. */
USTRUCT(BlueprintType)
struct FGrabInformation
{
	GENERATED_BODY()

public:

	/** Storage variable for the hand when grabbing a grabbableActor. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	AVRHand* handRef;

	/** Component used to target location/rotation while grabbed. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	UPrimitiveComponent* targetComponent; 

	/** Store the original relative pickup rotation for re-attachment after collisions. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	FRotator originalPickupRelativeRotation; 

	/** Store the original relative pickup offset to the hand for re-attachment after collisions. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	FVector originalRelativePickupOffset; 

	/** The original grabbed location of the grabbable. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	FVector originalWorldGrabbedLocation; 

	/** Save the current world pickup location offset for collision check. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	FVector worldPickupOffset; 

	/** Save the current world pickup rotation offset for collision check. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	FRotator worldRotationOffset; 

	/** Default constructor. */
	FGrabInformation()
	{
		Reset();
	}

	/** Reset this structs variables. */
	void Reset()
	{
		handRef = nullptr;
		targetComponent = nullptr;
		originalPickupRelativeRotation = FRotator::ZeroRotator;
		originalRelativePickupOffset = FVector::ZeroVector;
		originalWorldGrabbedLocation = FVector::ZeroVector;
		worldPickupOffset = FVector::ZeroVector;
		worldRotationOffset = FRotator::ZeroRotator;
	}
};

/** Make a actor grabbable using this class. NOTE: components will have to use the mesh as the root component to be able to be part
 * of the grabbable actor... 
 * TODO: Improve target rotation mode for two handed grab modes. */
UCLASS()
class VRTEMPLATE_API AGrabbableActor : public AActor, public IHandsInterface
{
	GENERATED_BODY()

public:

	/*** Grabbable mesh root component. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
		UStaticMeshComponent* grabbableMesh;

	/** Component to play audio when this grabbable impacts other objects on hit. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
		UAudioComponent* grabbableAudio;

	/** Struct to hold information on the hand grabbing this component like grab offsets, handRefference, target locations etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
		FGrabInformation handRefInfo;

	/** Struct to hold a second hands information if two handed grabbing is enabled on this actor. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
		FGrabInformation otherHandRefInfo;

	/** Storage value for current grab mode of this grabbable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		EGrabMode grabMode;

	/** Storage value for current grab mode of this grabbable for the second hand to grab it. NOTE: Requires two handed grabbing to be enabled in the interactable settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		ESecondGrabMode secondHandGrabMode;

	/** Storage value for current return mode of this grabbable. 
	 * Note: Only used if grab mode is AttatchToWithPhysics grab mode, otherwise it will always use ReturnWithCollision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		EReturnMode lerpMode;

	/** Collision type to use when in PhysicsHandleStiff mode or AttatchToWithPhysics grabMode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		EOverlapType collisionType;

	/** Friction material to use while grabbed to prevent grabbable edge colliders catching on flat surfaces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics")
		UPhysicalMaterial* physicsMaterialWhileGrabbed;

	/** The haptic feedback intensity multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
		float hapticIntensityMultiplier;

	/** The haptic feedback collision effect to play override. If null it will use the default collision feedback in the AVRHands grabbing this grabbable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
		UHapticFeedbackEffect_Base* collisionFeedbackOverride;

	/** Sound to play on collision if null it will use the AVRHands default collision sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Haptic Effects")
		USoundBase* impactSoundOverride;

	/** Time it takes for component to return to the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		float timeToLerp; 

	/** Accuracy/Size of the sweep trace that checks if there is a clear path to the hand. (current grabbed component scale * sweepAccuracy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics")
		float sweepAccuracy;

	/** The second hands rotation offset for when twoHandedGrabbing is enabled and the grabbable is grabbed with two hands while in the secondGrabMode targetRotation. NOTE: Use-full for offsetting different models for weapons etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		FRotator secondHandRotationOffset;

	/** Used for snap to hand so the rotation for each object can be adjusted to an offset. NOTE: If snapToHand is false then this will not be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		FRotator snapToHandRotationOffset;

	/** Used for snap to hand so the location for each object can be adjusted to an offset. NOTE: If snapToHand is false then this will not be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		FVector snapToHandLocationOffset;

	/** Physics material while grabbed enabled or disabled. NOTE: Replaces physics material override back to null after release. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		bool physicsMaterialWhileGrabbedEnabled;

	/** Snap the grabbed object to the current hand location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		bool snapToHand;

	/** Consider the weight of the object when throwing it by decreasing velocity based on mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics")
		bool considerMassWhenThrown;

	/** Can be used to change the mass on grab to prevent it effecting the physics handles functionality.
	 * NOTE: Changes mass of this grabbable when grabbed to massWhenGrabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics")
		bool changeMassOnGrab;

	/** Mass to use on the grabbableMesh while grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable|Physics", meta = (EditCondition = "changeMassOnGrab"))
		float massWhenGrabbed;

	/** The current frames velocity of  the grabbable. NOTE: Used instead of physics velocity as the grabbable isn't simulating physics when not colliding with other objects. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grabbable|Physics")
		float currentFrameVelocity;

	/** The current frames velocity change when compared to the last frame. NOTE: Useful for calculating impact force etc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grabbable|Physics")
		float currentVelocityChange;

	/** Show debug information. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		bool debug;

	/** Cancel the grabbing of this component. */
	UPROPERTY(BlueprintReadWrite, Category = "Grabbable")
		bool cancelGrab;

	/** The interfaces interactable settings for how to interact with VR controllers/hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		FHandInterfaceSettings interactableSettings;


	//////////////////////////
	//	     Pointers       //
	//////////////////////////

	/** Array of components to check for collisions/overlap events. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	TArray<UPrimitiveComponent*> collidableMeshes;

	/** Holds ignored actors for when performing traces in the collision check functions. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	TArray<AActor*> ignoredActors;

	/** The original physics material of this grabbable before grabbed. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	UPhysicalMaterial* originalPhysicalMAT;

	/** Stored impact sound pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	USoundBase* impactSound;

	/** Stored collision haptic feedback pointer. */
	UPROPERTY(BlueprintReadOnly, Category = "Grabbable")
	UHapticFeedbackEffect_Base* collisionFeedback;

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/** Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbedComponent OnMeshGrabbed;

	/** Mesh grabbed by hand function has ended when this has ran. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshGrabbedEnd;

	/** Mesh grabbed by hand function has ended when this has ran. */
	UPROPERTY(BlueprintAssignable)
	FPhysicalStateChanged OnPhysicsStateChanged;

	/** Mesh grabbed by hand function has ended when this has ran. */
	UPROPERTY(BlueprintAssignable)
	FCollisionChanged OnCollisionChanged;

	/** Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbedComponent OnMeshReleased;

private:
	 
	FTransform secondHandOriginalTransform, secondHandGrabbableRot; /** Original second hand grabbed transform of the grabbableMesh. */
	FTimerHandle lastRumbleHandle;/** Timer handle to allow another rumble within this class when over. */
	float lastImpactSoundTime, lastRumbleIntensity;
	float lastFrameVelocity; /** Last frames velocity to help calculate velocity change over time. */
	float lastHandGrabDistance; /** distance the hand was away from this actor last frame.  */
	float lerpStartTime; /** Start time of the lerp. */
	float lastZ;
	bool lerping; /** Lerp back to the hands intended grabbing position. */
	bool attatched, physicsAttatched;/** Is the grabbable current attatched to the scene component or the physics handle. */
	bool driveEnabled; /** Is the physics drive currently enabled. */	

protected:

	/** Level Start */
	virtual void BeginPlay() override;

#if WITH_EDITOR
	/** Handles when certain values in this class can be edited. */
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	/** Function for returning to attach to mode when using mixed physics mode. */
	void LerpingBack(float deltaTime);

	/** Toggles the lerping value and saves the lerpEndTime if turned on. */
	void ToggleLerping(bool on);

	/** Update the current grab information for the current world offset in location and rotation. */
	void UpdateGrabInformation();

private:

	/** Binded event to this actors hit response delegate. */
	UFUNCTION(Category = "Collision")
	void OnHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	/** Event to reset the rumble intensity last set on hit. */
	UFUNCTION(Category = "Collision")
	void ResetLastRumbleIntensity();	

public:

	/** Constructor. */
	AGrabbableActor();
	
	/** Frame. */
	virtual void Tick(float DeltaTime) override;

	/** Called to attach the grabbable to the hand using attach to. */
	void PickupAttatchTo();

	/** Called to attach the grabbable to the hand using physics handle.
	 * NOTE: grab information (hand) to use to grab this grabbable with its physics handle. */
	void PickupPhysicsHandle(FGrabInformation grabInfo);

	/** Called to detach the grabbable from the hand. */
	void DropAttatchTo();

	/** Called to detach the grabbable from the hands collision physics handle. */
	void DropPhysicsHandle(FGrabInformation grabInfo);

	/** Check for colliding components. 
	 * @Param complex, Should each individual components bounds be checked for collisions OR the entire actor bounds to save on performance. */
	bool GetColliding();

	/** Check if the actor is grabbed. */
	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsActorGrabbed();

	/** Check if the actor is grabbed by two hands. */
	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsActorGrabbedTwoHanded();

	/** Sweep the current actor to the current hand location and check if it hits anything.
	 * @Return FHitResult of the result from the component sweep. */
	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	FHitResult SweepActor();

	/** Get the current grabbed transform. */
	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	FTransform GetGrabbedTransform();

	/** Implementation of the required interface functions. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;
	virtual void Teleported_Implementation() override;

	/**  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};
