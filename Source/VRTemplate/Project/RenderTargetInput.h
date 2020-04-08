// All code is free to manipulate and use as is.

#pragma once
#include "CoreMinimal.h"
#include "Globals.h"
#include "Interactables/GrabbableActor.h"
#include "RenderTargetInput.generated.h"

/* Render target board input object for casting onto the Render Target Board's mesh to allow drawing functionality. So for example this would be a pencil or a marker. */
UCLASS()
class VRTEMPLATE_API ARenderTargetInput : public AGrabbableActor
{
	GENERATED_BODY()

public:

	/* Constructor. */
	ARenderTargetInput();
};
