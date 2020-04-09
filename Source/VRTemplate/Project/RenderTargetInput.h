// All code is free to manipulate and use as is.

#pragma once
#include "CoreMinimal.h"
#include "Globals.h"
#include "Interactables/GrabbableActor.h"
#include "Project/RenderTargetBoard.h"
#include "RenderTargetInput.generated.h"

/* Enum to check if its and input or removal. */
UENUM(BlueprintType)
enum class EBoardInputType : uint8 
{
	input,
	removal
};

/* Render target board input object for casting onto the Render Target Board's mesh to allow drawing functionality. So for example this would be a pencil or a marker. */
UCLASS()
class VRTEMPLATE_API ARenderTargetInput : public AGrabbableActor
{
	GENERATED_BODY()

public:

	/* The input type of this render target input class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	EBoardInputType inputType;

	/* The input color, only used if the input type is not removal! */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	EMarkerColor inputColor;

	/* Checked when hitting a RenderTargetBoard to see if its supported for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FName boardType;

	/* The update rate of the render target input detecting board and running the draw/removal events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float updateRate;

	/* Input size of the marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	float inputSize;

	/* The distance to trace from the grabbable mesh down along the Z-Axis to look for a RenderTargetBoard object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board", meta = (UIMin = "0.0", ClampMin = "0.0"))
	float traceDistance;

	/* Should show class debug information like the trace while grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	bool debugTrace;

	/* The current colliding board. */
	UPROPERTY(BlueprintReadOnly, Category = "Board")
	ARenderTargetBoard* currentBoard;

private:

	FTimerHandle inputUpdateTimer; /* The timer handle for updating the input detection against a render target board. */
	bool firstHit; /* Is the hit returned from the input trace the first hit. */
	FVector lastTraceLocation; /* Last input traces hits endLocation component world location. */
	FVector2D lastUVLocation; /* The last uv location from last input trace. */

protected:

	/* Level start. */
	virtual void BeginPlay() override;

	/* Perform and input trace looking for a UV location on a RenderTargetInput class. */
	bool InputTrace(FVector2D& hitUVLoc);

public:

	/* Constructor. */
	ARenderTargetInput();

	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/* Update the input check onto a render target board. */
	UFUNCTION(Category = "Input")
	void UpdateInput();

	/* Override grab and release functions to pause and un-pause the input update function timer. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
};
