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

	// Setup class defaults.
	inputType = EBoardInputType::input;
	inputColor = EMarkerColor::Black;
	boardType = "Board";
	updateRate = 0.02f;
	inputSize = 0.05f;
	currentBoard = nullptr;
	traceDistance = 10.0f;
	debugTrace = false;
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

	// Setup timer and pause it until it is needed to be ran.
	FTimerDelegate timerDel;
	timerDel.BindUFunction(this, "UpdateInput");
	GetWorldTimerManager().SetTimer(inputUpdateTimer, timerDel, updateRate, true, 0.0f);
	GetWorldTimerManager().PauseTimer(inputUpdateTimer);
}

void ARenderTargetInput::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//...
}

void ARenderTargetInput::UpdateInput()
{
	// If grabbed.
	if (handRefInfo.handRef)
	{
		// Calculate current end location.
		FVector endPoint = grabbableMesh->GetComponentLocation() + (grabbableMesh->GetUpVector() * -traceDistance);

		// Only draw if the input has moved enough.
		if (!lastTraceLocation.Equals(endPoint, inputSize))
		{
			// Get UV location if a render target board can be found from the input trace.
			FVector2D UVLoc;
			if (InputTrace(UVLoc))
			{
				// If first hit draw onto the board.
				if (firstHit)
				{
					currentBoard->DrawOnBoard(UVLoc, inputColor, inputSize);
				}
				else
				{
					// Find number of times to draw between current and last to prevent jagged lines.
					float difference = FMath::Abs((UVLoc - lastUVLocation).Size());
					int lastIndex = (int)(difference / inputSize) * 3.0f;
					for (int i = 1; i < lastIndex; i++)
					{
						float lerpingAlpha = (i * inputSize) / difference;
						FVector2D lerpingUVLoc = FMath::Lerp(lastUVLocation, UVLoc, lerpingAlpha);
						currentBoard->DrawOnBoard(lerpingUVLoc, inputColor, inputSize);
					}
				}

				// Save information for next check.
				lastUVLocation = UVLoc;
				lastTraceLocation = endPoint;
				firstHit = false;
				return;
			}
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
	GetWorldTimerManager().UnPauseTimer(inputUpdateTimer);
}

void ARenderTargetInput::GrabReleased_Implementation(AVRHand* hand)
{
	AGrabbableActor::GrabReleased_Implementation(hand);

	// Pause the input check. 
	GetWorldTimerManager().PauseTimer(inputUpdateTimer);
}

