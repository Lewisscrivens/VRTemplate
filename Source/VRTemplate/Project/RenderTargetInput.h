// All code is free to manipulate and use as is.

#pragma once
#include "CoreMinimal.h"
#include "Globals.h"
#include "Interactables/GrabbableActor.h"
#include "Project/RenderTargetBoard.h"
#include "RenderTargetInput.generated.h"

/** Enum to check if its and input or removal. */
UENUM(BlueprintType)
enum class EBoardInputType : uint8 
{
	input,
	removal
};

/** Render target board input object for casting onto the Render Target Board's mesh to allow drawing functionality. So for example this would be a pencil or a marker.
 * NOTE: Would be much better to not use the grabbable actor class and make a separate collision class so that writing on the board is easier/feels more natural. For the sake
 *       of saving time I have used the grabbable actor but its a big improvement that could be made... */
UCLASS()
class VRTEMPLATE_API ARenderTargetInput : public AGrabbableActor
{
	GENERATED_BODY()

public:

	/** The input type of this render target input class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	EBoardInputType inputType;

	/** Checked when hitting a RenderTargetBoard to see if its supported for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FName boardType;

	/** The current update rate per second of the trace function. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float updateRate;

	/** Input size of the marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float inputSize;

	/** The distance to trace from the grabbable mesh down along the Z-Axis to look for a RenderTargetBoard object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (UIMin = "0.0", ClampMin = "0.0"))
	float traceDistance;

	/** This class is enabled? Needed for example in the marker the lid can be put on which would make this class functionality pointless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool traceEnabled;

	/** Should show class debug information like the trace while grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool debugTrace;

	/** The current colliding board. */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	ARenderTargetBoard* currentBoard;

private:

	bool firstHit; /** Is the hit returned from the input trace the first hit. */
	FVector lastTraceLocation; /** Last input traces hits endLocation component world location. */
	FVector2D lastUVLocation; /** The last uv location from last input trace. */
	FTimerHandle updateTimer; /** The timer handle to update the trace function. */

protected:

	/** Level start. */
	virtual void BeginPlay() override;

	/** Perform and input trace looking for a UV location on a RenderTargetInput class. */
	bool InputTrace(FVector2D& hitUVLoc);

public:

	/** Constructor. */
	ARenderTargetInput();

	/** Frame. */
	virtual void Tick(float DeltaTime) override;

	/** Update the input check onto a render target board. */
	UFUNCTION(Category = "Input")
	void UpdateInput();

	/** Override grab and release functions to pause and un-pause the update trace function in this classes tick function. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
};
