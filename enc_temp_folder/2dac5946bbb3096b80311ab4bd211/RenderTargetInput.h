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

/* Render target board input object for casting onto the Render Target Board's mesh to allow drawing functionality. So for example this would be a pencil or a marker.
 * NOTE: Currently only blue is supported as it is set manually in the material this could be changed but will require a more complex board material for transitioning between 
 *       different colored markers... Didn't add this as the class is only an example of how this can be done. */
UCLASS()
class VRTEMPLATE_API ARenderTargetInput : public AGrabbableActor
{
	GENERATED_BODY()

public:

	/* The input type of this render target input class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	EBoardInputType inputType;

	/* Checked when hitting a RenderTargetBoard to see if its supported for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FName boardType;

	/* The current update rate per second of the trace function. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float updateRate;

	/* Input size of the marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float inputSize;

	/* The distance to trace from the grabbable mesh down along the Z-Axis to look for a RenderTargetBoard object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (UIMin = "0.0", ClampMin = "0.0"))
	float traceDistance;

	/* Should show class debug information like the trace while grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool debugTrace;

	/* The current colliding board. */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	ARenderTargetBoard* currentBoard;

private:

	bool firstHit; /* Is the hit returned from the input trace the first hit. */
	FVector lastTraceLocation; /* Last input traces hits endLocation component world location. */
	FVector2D lastUVLocation; /* The last uv location from last input trace. */
	FTimerHandle updateTimer; /* The timer handle to update the trace function. */

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

	/* Override grab and release functions to pause and un-pause the update trace function in this classes tick function. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
};
