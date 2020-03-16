// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/HandsInterface.h"
#include "Globals.h"
#include "Interactable.generated.h"

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogInteractable, Log, All);

/* Declare interact delegate. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInteractEvent, bool, pressed);

/* Declare classes used. */
class AVRHand;

/* Class with implemented interface ready for blueprint use, Needed due to the IHandsInterface class being null when called
 * from a BlueprintActor with said interface implemented. */
UCLASS()
class NINETOFIVE_API AInteractable : public AActor, public IHandsInterface
{
	GENERATED_BODY()

public:

	/* Enable the debugging message for printing this interactables current settings every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interatable")
	bool debugSettings;

	/* The interfaces interactable settings for how to interact with VR controllers/hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FHandInterfaceSettings interactableSettings;

	/* Cast to delegate when interact is pressed. */
	UPROPERTY(BlueprintAssignable)
	FInteractEvent OnInteract;

protected:

	

	/* Level Start. */
	virtual void BeginPlay() override;

public:

	/* Constructor. */
	AInteractable();

	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/* Blueprint interface functions to be override in the BP....
	 * NOTE: This is needed due to the interface being C++ it doesn't work correctly in BP... */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void GrabPressedBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void GrabReleasedBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void DraggingBP(float deltaTime);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void GripPressed(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void GripReleased();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void Interact(bool pressed);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void OverlappingBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void EndOverlappingBP(AVRHand* hand);
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void GrabbedWhileLockedBP();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void ReleasedWhileLockedBP();
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = Blueprint)
		void TeleportedBP();

	/* Implementation of the interfaces functions... */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void GrabbedWhileLocked_Implementation() override;
	virtual void ReleasedWhileLocked_Implementation() override;
	virtual void GripPressed_Implementation(AVRHand* hand) override;
	virtual void GripReleased_Implementation() override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Interact_Implementation(bool pressed) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;
	virtual void Teleported_Implementation() override;

	/*  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};
