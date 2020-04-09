// All code is free to manipulate and use as is.

#pragma once
#include "CoreMinimal.h"
#include "Globals.h"
#include "GameFramework/Actor.h"
#include "RenderTargetBoard.generated.h"

/* Define used classes. */
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UCanvasRenderTarget2D;
class UMaterialInterface;

/* A class which allows the given boardMesh to be drawn on like a piece of paper or white board from the RenderTargetInput class. */
UCLASS()
class VRTEMPLATE_API ARenderTargetBoard : public AActor
{
	GENERATED_BODY()
	
public:

	/* The board mesh to compare world hit locations with UV locations to allow drawing onto a RT mask. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Board")
	UStaticMeshComponent* boardMesh;

	/* Material instance for the boardMesh to allow updating of the material from the input and removal render targets... */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Board")
	UMaterialInstanceDynamic* boardMeshMaterialInst;

	/* The material instance created for the input material for drawing onto the inputRenderTarget. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Board")
	UMaterialInstanceDynamic* inputMaterialInst;

	/* The material instance created for the removal material for drawing onto the removalRenderTarget. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Board")
	UMaterialInstanceDynamic* removalMaterialInstance;

	/* The created render target set in the boardMesh's material instance for input of the marker blue. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Board")
	UCanvasRenderTarget2D* blueRenderTarget;

	/* The created render target set in the boardMesh's material instance for removal of the marker. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Board")
	UCanvasRenderTarget2D* removalRenderTarget;

	/* Material to create boardMesh material instance from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	UMaterialInterface* boardMeshMaterial;

	/* Material to create input material instance from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	UMaterialInterface* inputMaterial;

	/* Material to create removal material instance from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	UMaterialInterface* removalMaterial;
	
	/* The size of the render targets to use. NOTE: Changes resolution of marker input/removal.
	 * NOTE: Increase the more the bigger your board mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	FVector2D renderTargetSize;

	/* Checked in a RenderTargetInput object to see if its this board that it supports. Needs to be set the same in all input and removal object for them to work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	FName boardType;

protected:
	
	/* Level start. */
	virtual void BeginPlay() override;

public:	

	/* Constructor. */
	ARenderTargetBoard();
	
	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/* Draw on the board into the inputRenderTarget. 
	 * NOTE: Called from RenderTargetInput class when touching the board with an input. */
	void DrawOnBoard(FVector2D uvLocation, float size);

	/* Remove from the board and draw into the removalRenderTarget.
	 * NOTE: Called from RenderTargetInput class when touching the board with a removal. */
	void RemoveFromBoard(FVector2D uvLocation, float size);

	/* Clear all render targets on the board. */
	UFUNCTION(BlueprintCallable, Category = "Board")
	void ClearBoard();
};