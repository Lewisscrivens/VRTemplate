// Fill out your copyright notice in the Description page of Project Settings.

#include "Interactables/InteractableActor.h"

DEFINE_LOG_CATEGORY(LogInteractable);

AInteractableActor::AInteractableActor()
{
	PrimaryActorTick.bCanEverTick = true;

	debugSettings = false;

	// Setup default interface variables.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.handMinRumbleDistance = 10.0f;
}

void AInteractableActor::BeginPlay()
{
	Super::BeginPlay();

	//...
}

void AInteractableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if DEVELOPMENT
	// Print interface settings.
	if (debugSettings) UE_LOG(LogInteractable, Warning, TEXT("%s"), *interactableSettings.ToString());
#endif
}

void AInteractableActor::GrabPressed_Implementation(AVRHand* hand)
{
	GrabPressedBP(hand);
}

void AInteractableActor::GrabReleased_Implementation(AVRHand* hand)
{
	GrabReleasedBP(hand);
}

void AInteractableActor::Dragging_Implementation(float deltaTime)
{
	DraggingBP(deltaTime);
}

void AInteractableActor::Overlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::Overlapping_Implementation(hand);
	OverlappingBP(hand);
}

void AInteractableActor::GripPressed_Implementation(AVRHand* hand)
{
	GripPressed(hand);
}

void AInteractableActor::GripReleased_Implementation()
{
	GripReleased();
}

void AInteractableActor::Interact_Implementation(bool pressed)
{
	OnInteract.Broadcast(pressed);
	Interact(pressed);
}
void AInteractableActor::EndOverlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::EndOverlapping_Implementation(hand);
	EndOverlappingBP(hand);
}

void AInteractableActor::Teleported_Implementation()
{
	TeleportedBP();
}

FHandInterfaceSettings AInteractableActor::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void AInteractableActor::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}

void AInteractableActor::GrabbedWhileLocked_Implementation()
{
	GrabbedWhileLockedBP();
}

void AInteractableActor::ReleasedWhileLocked_Implementation()
{
	ReleasedWhileLockedBP();
}

