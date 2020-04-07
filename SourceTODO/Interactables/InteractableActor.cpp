// Fill out your copyright notice in the Description page of Project Settings.

#include "Interactable.h"

DEFINE_LOG_CATEGORY(LogInteractable);

AInteractable::AInteractable()
{
	PrimaryActorTick.bCanEverTick = true;

	debugSettings = false;

	// Setup default interface variables.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.handMinRumbleDistance = 10.0f;
}

void AInteractable::BeginPlay()
{
	Super::BeginPlay();

}

void AInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if DEVELOPMENT
	// Print interface settings.
	if (debugSettings) UE_LOG(LogInteractable, Warning, TEXT("%s"), *interactableSettings.ToString());
#endif
}

void AInteractable::GrabPressed_Implementation(AVRHand* hand)
{
	GrabPressedBP(hand);
}

void AInteractable::GrabReleased_Implementation(AVRHand* hand)
{
	GrabReleasedBP(hand);
}

void AInteractable::Dragging_Implementation(float deltaTime)
{
	DraggingBP(deltaTime);
}

void AInteractable::Overlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::Overlapping_Implementation(hand);
	OverlappingBP(hand);
}

void AInteractable::GripPressed_Implementation(AVRHand* hand)
{
	GripPressed(hand);
}

void AInteractable::GripReleased_Implementation()
{
	GripReleased();
}

void AInteractable::Interact_Implementation(bool pressed)
{
	OnInteract.Broadcast(pressed);
	Interact(pressed);
}
void AInteractable::EndOverlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::EndOverlapping_Implementation(hand);
	EndOverlappingBP(hand);
}

void AInteractable::Teleported_Implementation()
{
	TeleportedBP();
}

FHandInterfaceSettings AInteractable::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void AInteractable::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}

void AInteractable::GrabbedWhileLocked_Implementation()
{
	GrabbedWhileLockedBP();
}

void AInteractable::ReleasedWhileLocked_Implementation()
{
	ReleasedWhileLockedBP();
}

