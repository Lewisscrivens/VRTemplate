// All code is free to manipulate and use as is.

#include "RenderTargetBoard.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Kismet/KismetRenderingLibrary.h"

ARenderTargetBoard::ARenderTargetBoard()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create board mesh component.
	boardMesh = CreateDefaultSubobject<UStaticMeshComponent>("BoardMesh");
	boardMesh->SetCollisionProfileName("Interactable");
	boardMesh->SetNotifyRigidBodyCollision(true);

	// Setup variable defaults.
	renderTargetSize = FVector2D(512.0f, 512.0f);
	boardType = "Board";
}

void ARenderTargetBoard::BeginPlay()
{
	Super::BeginPlay();
	
	// Setup the material instances.
	boardMeshMaterialInst = UMaterialInstanceDynamic::Create(boardMeshMaterial, this);
	boardMesh->SetMaterial(0, boardMeshMaterial);
	inputMaterialInst = UMaterialInstanceDynamic::Create(inputMaterial, this);
	removalMaterialInstance = UMaterialInstanceDynamic::Create(removalMaterial, this);

	// Create and setup this boards render targets.
	blackRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), renderTargetSize.X, renderTargetSize.Y);
	redRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), renderTargetSize.X, renderTargetSize.Y);
	blueRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), renderTargetSize.X, renderTargetSize.Y);
	removalRenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(GetWorld(), UCanvasRenderTarget2D::StaticClass(), renderTargetSize.X, renderTargetSize.Y);
	boardMeshMaterialInst->SetTextureParameterValue("MaskBlack", blackRenderTarget);
	boardMeshMaterialInst->SetTextureParameterValue("MaskRed", redRenderTarget);
	boardMeshMaterialInst->SetTextureParameterValue("MaskBlue", blueRenderTarget);
}

void ARenderTargetBoard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//...
}

void ARenderTargetBoard::DrawOnBoard(FVector2D uvLocation, EMarkerColor color, float size)
{
	// Convert to vector.
	FVector uvVectorLoc = FVector(uvLocation.X, uvLocation.Y, 0.0f);

	// Set the location on the texture.
	inputMaterialInst->SetVectorParameterValue("DrawLocation", uvVectorLoc);
	inputMaterialInst->SetScalarParameterValue("DrawSize", size);

	// Draw onto the correct render target depending on the color.
	switch (color)
	{
	case EMarkerColor::Black:
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(GetWorld(), blackRenderTarget, inputMaterialInst);
	break;
	case EMarkerColor::Red:
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(GetWorld(), redRenderTarget, inputMaterialInst);
	break;
	case EMarkerColor::Blue:
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(GetWorld(), blueRenderTarget, inputMaterialInst);
	break;
	}
}

void ARenderTargetBoard::RemoveFromBoard(FVector2D uvLocation, float size)
{
	// Convert to vector.
	FVector uvVectorLoc = FVector(uvLocation.X, uvLocation.Y, 0.0f);

	// Set the location on the texture.
	removalMaterialInstance->SetVectorParameterValue("DrawLocation", uvVectorLoc);
	removalMaterialInstance->SetScalarParameterValue("DrawSize", size);

	// Draw onto the removal render target.
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(GetWorld(), removalRenderTarget, removalMaterialInstance);
}

void ARenderTargetBoard::ClearBoard()
{
	// Reset all render targets to black.
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), blackRenderTarget, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), redRenderTarget, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), blueRenderTarget, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(GetWorld(), removalRenderTarget, FLinearColor::Black);
}

