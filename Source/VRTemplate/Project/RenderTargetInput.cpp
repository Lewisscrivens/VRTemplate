// All code is free to manipulate and use as is.

#include "RenderTargetInput.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/SceneComponent.h"
#include "Interactables/GrabbableActor.h"
#include "Components/StaticMeshComponent.h"
#include "Player/VRHand.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

ARenderTargetInput::ARenderTargetInput()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PostUpdateWork;

	// Setup class defaults.
	inputType = EBoardInputType::input;
	boardType = "Board";
	inputSize = 0.05f;
	updateRate = 0.02f;
	currentBoard = nullptr;
	traceDistance = 10.0f;
	debugTrace = false;
	traceEnabled = true;
}

void ARenderTargetInput::BeginPlay()
{
	Super::BeginPlay();

	// Destroy if theres no grabbable mesh setup.
	if (!grabbableMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARenderTargetInput class destroyed named %s because there was no grabbable component setup."), *GetName());
		Destroy();
		return;
	}

	// Setup update timer and pause it till grabbed.
	FTimerDelegate timerDel;
	timerDel.BindUFunction(this, "UpdateInput");
	GetWorldTimerManager().SetTimer(updateTimer, timerDel, updateRate, true, 0.0f);
	GetWorldTimerManager().PauseTimer(updateTimer);
}

void ARenderTargetInput::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//...
}

void ARenderTargetInput::UpdateInput()
{
	// If grabbed.
	if (handRefInfo.handRef && traceEnabled)
	{
		// Calculate current end location.
		FVector endPoint = grabbableMesh->GetComponentLocation() + (grabbableMesh->GetUpVector() * -traceDistance);

		// Get UV location if a render target board can be found from the input trace.
		FVector2D UVLoc;
		if (InputTrace(UVLoc))
		{
			// If first hit draw onto the board.
			if (firstHit)
			{
				if (inputType == EBoardInputType::input) currentBoard->DrawOnBoard(UVLoc, inputSize);
				else currentBoard->RemoveFromBoard(UVLoc, inputSize);
			}
			else
			{
				// Find number of times to draw between current and last to prevent jagged lines.
				float UVDistance = FVector2D::Distance(UVLoc, lastUVLocation); 
				float size = inputSize * FMath::Clamp((UVDistance / 0.05f), 0.2f, 1.0f);
				int lastIndex = (int)(UVDistance / size);
				for (int i = 1; i < lastIndex; i++)
				{
					float lerpingAlpha = (i * size) / UVDistance;
					FVector2D lerpingUVLoc = FMath::Lerp(lastUVLocation, UVLoc, lerpingAlpha);
					if (inputType == EBoardInputType::input) currentBoard->DrawOnBoard(lerpingUVLoc, inputSize);
					else currentBoard->RemoveFromBoard(UVLoc, inputSize);
				}
			}

			// Rumble the hand while drawing on the board.
			if (handRefInfo.handRef) handRefInfo.handRef->PlayFeedback(nullptr, 1.0f, true);

			// Save information for next check.
			lastUVLocation = UVLoc;
			lastTraceLocation = endPoint;
			firstHit = false;
			return;
		}
	}

	// Must have left the board.
	firstHit = true;
}

bool ARenderTargetInput::InputTrace(FVector2D& hitUVLoc)
{
	// Perform line trace looking for a RenderTargetBoard actor.
	FHitResult hit;
	FVector startLocation = grabbableMesh->GetComponentLocation();
	FVector endLocation = startLocation + (grabbableMesh->GetUpVector() * -traceDistance);
	TArray<TEnumAsByte<EObjectTypeQuery>> objectTypes;
	objectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldStatic));
	TArray<AActor*> ignored;
	ignored.Add(this);
	ignored.Add(handRefInfo.handRef);
	UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), startLocation, endLocation, objectTypes, true, ignored, EDrawDebugTrace::None, hit, true);

	// Show debug lines for testing.
	if (debugTrace)
	{
		if (hit.bBlockingHit)
		{
			DrawDebugLine(GetWorld(), startLocation, hit.Location, FColor::Green, false, 0.1f, 0.0f, 1.0f);
			DrawDebugPoint(GetWorld(), hit.Location, 0.5f, FColor::Red, false, 0.1f, 0.0f);
		}
		else
		{
		    DrawDebugLine(GetWorld(), startLocation, endLocation, FColor::Red, false, 0.1f, 0.0f, 1.0f);
			DrawDebugPoint(GetWorld(), endLocation, 0.5f, FColor::Red, false, 0.1f, 0.0f);
		}
	}

	// If the render target board was hit set it.
	if (hit.bBlockingHit && hit.GetActor())
	{
		if (ARenderTargetBoard* foundBoard = Cast<ARenderTargetBoard>(hit.GetActor()))
		{
			currentBoard = foundBoard;
			FVector2D foundUV;
			bool found = UGameplayStatics::FindCollisionUV(hit, 0, foundUV);
			hitUVLoc = foundUV;// For debugging.
			return true;
		}
	}

	// Failed.
	return false;
}

void ARenderTargetInput::GrabPressed_Implementation(AVRHand* hand)
{
	AGrabbableActor::GrabPressed_Implementation(hand);

	// Un-pause the input check.
	GetWorldTimerManager().UnPauseTimer(updateTimer);
}

void ARenderTargetInput::GrabReleased_Implementation(AVRHand* hand)
{
	AGrabbableActor::GrabReleased_Implementation(hand);

	// Pause the input check. 
	GetWorldTimerManager().PauseTimer(updateTimer);
}

