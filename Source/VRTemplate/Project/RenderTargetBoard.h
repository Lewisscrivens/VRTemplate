// All code is free to manipulate and use as is.

#pragma once
#include "CoreMinimal.h"
#include "Globals.h"
#include "GameFramework/Actor.h"
#include "RenderTargetBoard.generated.h"

/* A class which allows the given boardMesh to be drawn on like a piece of paper or white board from the RenderTargetInput class. */
UCLASS()
class VRTEMPLATE_API ARenderTargetBoard : public AActor
{
	GENERATED_BODY()
	
public:	
	
	/* Constructor. */
	ARenderTargetBoard();

protected:
	
	/* Level start. */
	virtual void BeginPlay() override;

public:	
	
	/* Frame. */
	virtual void Tick(float DeltaTime) override;

};
