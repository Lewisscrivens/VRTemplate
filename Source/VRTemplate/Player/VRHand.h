// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "MotionControllerComponent.h"
#include "GameFramework/Actor.h"
#include "Player/HandsInterface.h"
#include "Globals.h"
#include "VRHand.generated.h"

/** Declare log type for the hand class. */
DECLARE_LOG_CATEGORY_EXTERN(LogHand, Log, All);

/** Declare classes used in the H file. */
class AVRPawn;
class USoundBase;
class UAudioComponent;
class APlayerController;
class USceneComponent;
class UBoxComponent;
class UMotionControllerComponent;
class UVRPhysicsHandleComponent;
class USkeletalMeshComponent;
class UHapticFeedbackEffect_Base;
class UEffectsContainer;
class UWidgetInteractionComponent;
class USphereComponent;
class UWidgetComponent;

/** Controller type enum for selecting the offset of each hand. */
UENUM(BlueprintType)
enum class EVRController : uint8
{
	Index,
	Vive,
	Oculus
};

/** NOTE: Just flipping a mesh on an axis to create a left and right hand from the said mesh will break its physics asset in version UE4.23
 * NOTE: HandSkel collision used for interacting with grabbable etc. Constrained components must use physicsCollider to prevent constraint breakage. */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class VRTEMPLATE_API AVRHand : public AActor
{
	GENERATED_BODY()

public:

	/** Scene component to hold the controller. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneComponent* scene;

	/** Motion controller. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UMotionControllerComponent* controller;

	/** Scene component to hold the hand skel and colliders. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneComponent* handRoot;

	/** Pointer to the hand skeletal mesh component from the player controller pawn. Set in BP. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	USkeletalMeshComponent* handSkel;

	/** Movement controller direction. (Point forward on X-axis in direction of hand and position to spawn teleporting spline etc.) */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USceneComponent* movementTarget;

	/** Sphere component to detect overlaps with widgets in the 3D world to fire the widgetInteractor events correctly. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	USphereComponent* widgetOverlap;

	/** Widget interaction component to allow interaction with 3D ui via touching it with the index finger on either hand. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UWidgetInteractionComponent* widgetInteractor;

	/** Collier to handle any physics collisions with constrained components to prevent breakage due to infinite hand force. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UBoxComponent* physicsCollider;

	/** Pointer to the grab colliders from the player controller pawn. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UBoxComponent* grabCollider;

	/** VR Physics handle component to handle grabbed actors collision against other colliders. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UVRPhysicsHandleComponent* grabHandle;

	/** VR Physics handle component to handle the hands physics collider to collide with constraint components. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UVRPhysicsHandleComponent* physicsHandle;

	/** Audio component to use for impact sounds or any other sounds from the hands current location.
	 * NOTE: Used over play sound at location as I need to know when the sound has stopped playing. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite)
	UAudioComponent* handAudio;

	/** Pointer to the main player class. Initialized in the player class. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Hand")
	AVRPawn* player;

	/** Pointer to the other hand for grabbing objects from hands. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadWrite, Category = "Hand")
	AVRHand* otherHand;

	/** Enum for what this current hand is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	EControllerHand handEnum;

	/** The current type of controller to set the hand class up with. 
	 * NOTE: Use SetControllerType to change this value during runtime. Or run SetupControllerOffsets after changing it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	EVRController controllerType;

	/** Pointer to store any overlapping objects that can be grabbed and contain the hands interface. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand")
	UObject* objectToGrab;

	/** Pointer to the object in the hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand")
	UObject* objectInHand;

	/** Extent of the physics Collider when the hand is closed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand")
	FVector pcClosedExtent;

	/** Position of the physics Collider when the hand is closed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand")
	FVector pcClosedPositiion;

	/** Do the hands disappear when grabbing things? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hand")
	bool hideOnGrab;

	/** Is the player grabbing? */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	bool grabbing;

	/** Is the player gripping? */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	bool gripping;

	/** Current velocity of the hand calculated in the tick function. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	FVector handVelocity;

	/** Current angular velocity of the hand calculated in the tick function. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	FVector handAngularVelocity;

	/** Current trigger value used for animating the hands etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	float trigger;

	/** Current thumb stick values for this hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	FVector2D thumbstick;
	
	/** Is the hand active. */
	UPROPERTY(BlueprintReadOnly, Category = "Hand|CurrentValues")
	bool active;

	/** Is the controller currently being tracked. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hand")
	bool foundController; 

	/** Enable any debug messages for this class.
	 * NOTE: Only used when DEVELOPMENT = 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hand")
	bool debug;

	/** Is the hand currently locked to an interactable. NOTE: Needs to grip to release from hand... */
	UPROPERTY(BlueprintReadOnly, Category = "Hand")
	bool handIsLocked; 

protected:

	/** Level start. */
	virtual void BeginPlay() override;

private:

	APlayerController* owningController; /** The owning player controller of this hand class. */
	FVector lastHandPosition, currentHandPosition;/** Last and current hand positions used for calculating force/velocity etc. */
	FQuat lastHandRotation, currentHandRotation;
	FTransform originalHandTransform;/** Saved original hand transform at the end of initialization. */	
	FVector pcOriginalOffset; /** Physics collider original open offset. */
	FVector pcOpenExtent; /** Physics collider original open extent. */
	FTimerHandle colTimerHandle, controllerColTimerHandle; /** Timer handle to store a reference to the Collider timer that loops the function CollisionDelay to check for overlapping collision. */

	int distanceFrameCount; /** How many frames has the hand been too far away from the grabbed object. */
	float currentHapticIntesity; /** The current playing haptic effects intensity for this hand classes controller. */
	bool collisionEnabled; /** Collision is enabled or disabled for this hand, disabled on begin play until the controller is tracked. */
	bool lastFrameOverlap; /** Did we overlap something in the last frame. */
	bool devModeEnabled; /** Local bool to check if dev mode is enabled. */

private:

	/** Loops every 0.1 seconds checking if the handSkel component is currently overlapping physics etc. If not re-enable collision, until then loop and check. */
	UFUNCTION(Category = "Collision")
	void CollisionDelay();

	/** Checks distance to collision to determine weather to temporarily disable it or teleport it back to the hand if blocked behind object. */
	void UpdatePhysicalCollision(float deltaTime);

	/** Check for overlapping actors with the grab Collider. (Runs Overlapping begin and end in hands interface on any actors with said interface) */
	void CheckForOverlappingActors();

	/** Function to find the first intractable interface going from the component up through the parents to the actor.
	 * @Param comp, Component to look through itself and its children components for the interface.
	 * @Return UObject pointer to the game object that owns the interface. */
	UObject* LookForInterface(USceneComponent* comp);

	/** When the distance between the hand and the grabbed component becomes too great it is released from the hand. */
	void CheckInteractablesDistance();

	/** Update the hand animation variables. */
	void UpdateAnimationInstance();

public:

	/** Constructor */
	AVRHand();

	/** Frame */
	virtual void Tick(float DeltaTime) override;

	/** Widget interactor begin overlap event. */
	UFUNCTION(Category = "Collision")
	void WidgetInteractorOverlapBegin(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Initialise variables given from the AVRPawn, Also acts as this classes begin play.
	 * @Param oppositeHand, Pointer to the other hand.
	 * @Param playerRef, Pointer to the VRPawn class. 
	 * @Param dev, Is developer mode activated. */
	void SetupHand(AVRHand * oppositeHand, AVRPawn* playerRef, bool dev);

	/** Function to change the type of controller this hand is.
	 * NOTE: Also handles setting up the controller offsets. */
	UFUNCTION(BlueprintCallable, Category = "Hand")
	void SetControllerType(EVRController type);

	/** Function to setup the current controller offset from the currentController type selected in this class. */
	UFUNCTION(BlueprintCallable, Category = "Hand")
	void SetupControllerOffset();

	/** Update the tracked state and collisions of this controller. */
	void UpdateControllerTrackedState();

	/** Grab the actorToGrab if it is not null. */
	void Grab();

	/** Force grab an object. NOTE: Good for swapping grabbed objects in code.
	 * @Param objectToForceGrab, The object to grab by this hand.
	 * NOTE: Will only work if the user is already grabbing trigger. */
	UFUNCTION(BlueprintCallable, Category = "Hand")
	void ForceGrab(UObject* objectToForceGrab);

	/** Drop the actorInHand if it is not null. NOTE: Only call this function from input.  */
	void Drop();

	/** Run interact function on anything that is grabbed in the hand. */
	void Interact(bool pressed);

	/** Used to release actors from the hand. Drop is only called by input. */
	UFUNCTION(BlueprintCallable, Category = "Hand")
	void ReleaseGrabbedActor();

	/** Grip is pressed/released. Currently only used for animation in BP. */
	void Grip(bool pressed);

	/** Function to run the teleport event after teleportation in the VRMovement class. */
	void TeleportHand();

	/** Toggle collision of all components for the hand to ignore other actor collisions.
	 * @Param open, open = activate collision and !open = ignore collisions. */
	void ActivateCollision(bool open, float openDelay = -1.0f);

	/** Reset the given VR physics handle to its default properties. 
	 * @Param reset handle back to its default values. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void ResetHandle(UVRPhysicsHandleComponent* handleToReset);

	/** Play an audio/Sound base from the current handAudio component at the hands location.
	 * @Param sound, The sound to play at the current controller location. 
	 * @Param volume, The volume to play the sound at. 
	 * @Param pitch, The pitch to play the sound at.
	 * @Param replace, Should this function replace the current playing sound. 
	 * @Return true or false on whether or not a sound was played. 
	 * NOTE: Running play sound with a null sound reference will play the default impact sound. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	bool PlaySound(USoundBase* sound = nullptr, float volume = 1.0f, float pitch = 1.0f, bool replace = false);


	/** Function to stop the current sound playing through the handAudio audio component.
	 * @Param fade, fade the sound out or stop completely. 
	 * @Param fadeTime, Time to fade the sound out. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void StopSound(bool fade = false, float timeToFade = 0.5f);

	/** Play the given feedback for the pawn.
	 * @Param feedback, the feedback effect to use, if left null this function will use the defaultFeedback in the pawn class.
	 * @Param intensity, the intensity of the effect to play.
	 * @Param replace, Should replace the current haptic effect playing? If there is one... 
	 * @NOTE  If replace is false it will only replace a haptic feedback effect if the new intensity is greater than the current playing one. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	bool PlayFeedback(UHapticFeedbackEffect_Base* feedback = nullptr, float intensity = 1.0f, bool replace = false);

	/** Returns the effects container from the pawn class. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	UEffectsContainer* GetEffects();

	/** Get the current haptic intensity if a haptic effect is playing.
	 * @Return 0 if no haptic effect is playing, otherwise return the current haptic effects intensity. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	float GetCurrentFeedbackIntensity();

	/** @Return true if this hand classes controller is currently playing a haptic effect. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	bool IsPlayingFeedback();

	/** Disables all hand functionality for the current hand, used in developer mode mainly for disabling hands temporarily.
	 * @Param disable, disable = disable the hand and !disable = enable the hand. */
	UFUNCTION(BlueprintCallable, Category = "Hands")
	void Disable(bool disable);
};