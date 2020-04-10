// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Globals.h"
#include "Player/VRPhysicsHandleComponent.h" // For the FPhysicsHandleData include.
#include "HandsInterface.generated.h"

/** Declare any delegates that will be re-used across classes, just include this class. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrabbed, class AVRHand*, handRef);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGrabbedComponent, class AVRHand*, handRef, class UPrimitiveComponent*, component);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGrabbedLocked);

/** Define this classes log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogHandsInterface, Log, All);

/** Declare classes used. */
class AVRHand;
class UActorComponent;
class UPrimitiveComponent;

/** Interface settings structure, used to hold any interface variables that will be changed and used in the hand class.
 * Makes changing this interfaces variables in BP possible... */
USTRUCT(BlueprintType)
struct FHandInterfaceSettings
{
	GENERATED_BODY()

public:

	/** If this component is interacted with by a physics handle use these physics handle values. NOTE: This is disabled by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		FPhysicsHandleData grabHandleData;
	/** Distance that the hand can be away from an interacting component. For example the component will be released from the hand if the distance becomes greater than this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		float releaseDistance;
	/** Distance that the hand must get away from the intractable before the rumble function is called. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		float handMinRumbleDistance;
	/** Current Distance between the hand and intractable when first grabbed used to determine when to rumble the hand on too much distance.  */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "HandInterface")
		float handDistance;
	/** Should the hand class check the interactables hand grab distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool canRelease;
	/** Two handed grab mode, don't release component from current grabbing hand when grabbed by another hand. NOTE: Best for weapons etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool twoHandedGrabbing;
	/** Locks the component to the hand so that the grip pressed event will release the component and not releasing the trigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool lockedToHand;
	/** Enable and disable highlight material functionality thats implemented in the hands interface class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool hightlightInteractable;
	/** Used to stop this interface from being interacted with. Useful for disabling things temporarily. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HandInterface")
		bool canInteract;	

	/** Constructor for this struct. Defaults to default interface values... */
	FHandInterfaceSettings(FPhysicsHandleData handleData = FPhysicsHandleData(), float releaseDist = 50.0f, float handMinRumbleDist = 1.0f, bool release = true, 
		bool twoHanded = false, bool lockedHand = false, bool highlight = true, bool interactEnabled = true, float currentHandDist = 0.0f)
	{
		this->grabHandleData = handleData;
		this->releaseDistance = releaseDist;
		this->handMinRumbleDistance = handMinRumbleDist;
		this->canRelease = release;
		this->twoHandedGrabbing = twoHanded;
		this->lockedToHand = lockedHand;
		this->hightlightInteractable = highlight;
		this->canInteract = interactEnabled;
		this->handDistance = currentHandDist;
	}

	/** Convert the values of this struct into a printable message/string for debugging. */
	FString ToString()
	{
		FString handleDataString = FString::Printf(TEXT("Handle Data = %s \n Release Distance = %f \n Hand Min Rumble Distance = %f \n Current Hand Grab Distance = %f \n Can Release = %s \n Locked to hand = %s \n Should Highlight = %s \n Can Interact = %s"),
			*grabHandleData.ToString(),
			releaseDistance,
			handMinRumbleDistance,
			handDistance,
			SBOOL(canRelease),
			SBOOL(lockedToHand),
			SBOOL(hightlightInteractable),
			SBOOL(canInteract));
			
		return handleDataString;
	}
};

//=================================
// Interface
//=================================

/** Holder of interface class... */
UINTERFACE(Blueprintable, BlueprintType, MinimalAPI)
class UHandsInterface : public UInterface
{
	GENERATED_BODY()
};

/** Interface class to hold C++ and Blueprint versions of intractable functions.
 * NOTE: Must implement the getter and setter method along with a local variable for the interactables FHandInterfaceSettings as it cannot be stored in the interface... */
class VRTEMPLATE_API IHandsInterface
{
	GENERATED_BODY()

private:

	bool overlapping = false; /** Keeps track of overlapping or not overlapping with any hands. */
	class TArray<AVRHand*> overlappingHands; /** Keep track of what hands are currently overlapping and what hands have ended the overlap. */
	TArray<UActorComponent*> foundChildren; /** Find any added children in BP. */
	TArray<UPrimitiveComponent*> foundComponents; /** Add any changed depth components to this array so they can be reset later. */

public:

	/** Implementable Functions for this interface. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void GrabPressed(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void GrabReleased(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void GrabbedWhileLocked();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void ReleasedWhileLocked();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void GripPressed(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void GripReleased();
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Dragging(float deltaTime);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Interact(bool pressed);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Overlapping(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void EndOverlapping(AVRHand* hand);
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
	void Teleported();

 	/** Get and set functions to allow changes from blueprint. */
 	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)		
 	FHandInterfaceSettings GetInterfaceSettings();
 	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = Hands)
 	void SetInterfaceSettings(FHandInterfaceSettings newInterfaceSettings);

	/** Setup functions for other classes overriding this interfaces functions */

	/** Ran when trigger is pressed all the way down. */
	virtual void GrabPressed_Implementation(AVRHand* hand);

	/** Ran when the trigger is released. */
	virtual void GrabReleased_Implementation(AVRHand* hand);

	/** Ran if the grabbable is locked to the hand and the trigger is pressed all the way down. */
	virtual void GrabbedWhileLocked_Implementation();

	/** Ran if the hand is locked to an interactable and the trigger is released. */
	virtual void ReleasedWhileLocked_Implementation();

	/** Ran when the controller is squeezed. */
	virtual void GripPressed_Implementation(AVRHand* hand);

	/** Ran when the controller is un-squeezed. */
	virtual void GripReleased_Implementation();

	/** Ticking function that is ran while an interactable is grabbed. */
	virtual void Dragging_Implementation(float deltaTime);

	/** Ran when the thumb button is pressed while something is being held. For example the trigger on the Valve index controller being pressed down. */
	virtual void Interact_Implementation(bool pressed);

	/** Ran on an interactable when the hand has selected it as the overlappingGrabbable to grab when grab is pressed.
	 * NOTE: Handles highlighting of interactables that are grabbable within the world. Be sure to call the super if overridden. */
	virtual void Overlapping_Implementation(AVRHand* hand);

	/** Ran on an interactable when the hand has selected it as the overlappingGrabbable to grab when grab is pressed is removed.
	 * NOTE: Handles un-highlighting of interactables that are grabbable within the world. Be sure to call the super if overridden. */
	virtual void EndOverlapping_Implementation(AVRHand* hand);

	/** Ran on an interactable when the hand is teleported. */
	virtual void Teleported_Implementation();

 	/**  Get and set functions to allow changes to an interactable. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation();
 	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings);
};
