// All code is free to manipulate and use as is.

#include "RenderTargetInput.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

ARenderTargetInput::ARenderTargetInput()
{
	PrimaryActorTick.bCanEverTick = true;

	// Setup the tracePoint scene component.
	tracePoint = CreateDefaultSubobject<USceneComponent>("TracePoint");
	tracePoint->SetMobility(EComponentMobility::Movable);

	// Setup class defaults.
	inputType = EBoardInputType::input;
	inputColor = EMarkerColor::Black;
	boardType = "Board";
	updateRate = 0.02f;
	inputSize = 0.05f;
	currentBoard = nullptr;
	traceDistance = 1.0f;
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
		// Only draw if the input has moved enough.
		if (!lastTraceLocation.Equals(tracePoint->GetComponentLocation(), inputSize))
		{
			// Get UV location if a render target board can be found from the input trace.
			FVector2D UVLoc;
			if (InputTrace(UVLoc))
			{
				// If first hit draw onto the board.
				if (firstHit) currentBoard->DrawOnBoard(UVLoc, inputColor, inputSize);
				// Otherwise draw between the last and current hit to prevent jagged lines.
				else
				{
					// Find number of times to draw between current and last.
					float difference = (UVLoc - lastUVLocation).Size();
					int lastIndex = (int)(difference / inputSize);
					for (int i = 1; i < lastIndex; i++)
					{
						float lerpingAlpha = (i * inputSize) / difference;
						FVector2D lerpingUVLoc = FMath::Lerp(lastUVLocation, UVLoc, lerpingAlpha);
						currentBoard->DrawOnBoard(lerpingUVLoc, inputColor, inputSize);
					}
				}

				// Save information for next check.
				lastUVLocation = UVLoc;
				lastTraceLocation = tracePoint->GetComponentLocation();
				firstHit = false;
			}
			// Reset and exit if no uv location can be found.
			else firstHit = true;
		}
	}
	// Reset everything if released.
	else firstHit = true;
}

bool ARenderTargetInput::InputTrace(FVector2D& hitUVLoc)
{
	// Perform line trace looking for a RenderTargetBoard actor.
	FHitResult hit;
	FVector startLocation = tracePoint->GetComponentLocation();
	FVector endLocation = startLocation + (tracePoint->GetForwardVector() * traceDistance);
	FCollisionObjectQueryParams objectParams;
	objectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams params;
	params.AddIgnoredActor(this);
	params.bTraceComplex = true;// Needed to return information about UV hit location.
	GetWorld()->LineTraceSingleByObjectType(hit, startLocation, endLocation, objectParams, params);

	// Show debug lines for testing.
	if (debugTrace)
	{
		if (hit.bBlockingHit)
		{
			DrawDebugLine(GetWorld(), hit.TraceStart, hit.Location, FColor::Green, false, 0.02f, 0.0f, 1.0f);
			DrawDebugPoint(GetWorld(), hit.Location, 3.0f, FColor::Red, false, 0.02f, 0.0f);
		}
		else
		{
			DrawDebugLine(GetWorld(), hit.TraceStart, hit.TraceEnd, FColor::Red, false, 0.02f, 0.0f, 1.0f);
			DrawDebugPoint(GetWorld(), hit.TraceEnd, 3.0f, FColor::Red, false, 0.02f, 0.0f);
		}
	}

	// If the render target board was hit set it.
	if (hit.bBlockingHit && hit.GetActor())
	{
		if (ARenderTargetBoard* foundBoard = Cast<ARenderTargetBoard>(hit.GetActor()))
		{
			currentBoard = foundBoard;
			UGameplayStatics::FindCollisionUV(hit, 0, hitUVLoc);
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

