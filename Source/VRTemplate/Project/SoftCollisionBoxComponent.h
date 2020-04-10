// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "SoftCollisionBoxComponent.generated.h"

/** Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogSoftCollisionBox, Log, All);

/** Box collision component. Used to enable soft collision only from the hand class for any interactables that can be picked up.
 * NOTE: Use-full for preventing things being forces through collisions due to the handSkel having infinite mass and force as it does not simulate physics. */
UCLASS(ClassGroup = (Custom), config = Engine, editinlinenew, Blueprintable, BlueprintType)
class VRTEMPLATE_API USoftCollisionBoxComponent : public UBoxComponent
{
	GENERATED_BODY()
	
public:

	/** Debugging messages enabled for what collision responses were changed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box")
	bool debug;

	/** The original values of each overlapped and changed interactable collision value in respect to the handSkel/hand collision channel. */
	UPROPERTY(BlueprintReadOnly, Category = "Box")
	TMap<UPrimitiveComponent*, TEnumAsByte<ECollisionResponse>> originalValues;

public:

	/** Constructor. */
	USoftCollisionBoxComponent();

	/** Level start. */
	virtual void BeginPlay() override;

	/** Binded function to overlap begin for this component. */
	UFUNCTION(Category = "Collision")
	void BeginOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Binded function to overlap end for this component. */
	UFUNCTION(Category = "Collision")
	void EndOverlap(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
