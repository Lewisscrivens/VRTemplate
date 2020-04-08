// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Player/HandsInterface.h"
#include "SlidableStaticMesh.generated.h"

/* Define this components log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogSlidableMesh, Log, All);

/* Selection of relative axis to slide this component in. */
UENUM(BlueprintType)
enum class ESlideAxis : uint8
{
	X UMETA(DisplayName = "X", ToolTip = "Slide in the relative x-axis to this components parent."),
	Y UMETA(DisplayName = "Y", ToolTip = "Slide in the relative y-axis to this components parent."),
	Z UMETA(DisplayName = "Z", ToolTip = "Slide in the relative z-axis to this components parent."),
};

/* Declare used classes. */
class AVRHand;
class UHapticFeedbackEffect_Base;
class USoundBase;

/* A simple version of a slidable static mesh actor that can be used for things that do not need collisions, like sliders on panel of electronics,
 * or things like inserting a floppy disc etc.
*  NOTE: Will slide in its relative selected axis to its parent component/actor. */
UCLASS(ClassGroup = (Custom), Blueprintable, BlueprintType, PerObjectConfig, EditInlineNew)
class VRTEMPLATE_API USlidableStaticMesh : public UStaticMeshComponent, public IHandsInterface
{
	GENERATED_BODY()
	
public:

	/* Reference to the hand currently grabbing this component. Also can be used as a bool to check if this is in a hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Slidable")
	AVRHand* handRef;

	/* The current relative axis for this sliding component to slide in when grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	ESlideAxis currentAxis;

	/* The current slidable limit that the slidable will move in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	float slideLimit;

	/* The slidables start location in the specified axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	float startLocation;

	/* Is the constraint limit centered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	bool centerLimit;

	/* Should release the slidable when it reaches it maxLimit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	bool releaseOnLimit;

	/* Original relative transform to calculate boundaries of slidable movement. */
	UPROPERTY(BlueprintReadWrite, Category = "Slidable")
	FTransform originalRelativeTransform;

	/* The interface settings for hand interaction with this intractable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	FHandInterfaceSettings interactableSettings;

	//////////////////////////
	//	  Grab delegates    //
	//////////////////////////

	/* Mesh grabbed by hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshGrabbed;

	/* Mesh released from hand. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshReleased;

	/* Mesh released on limit. */
	UPROPERTY(BlueprintAssignable)
	FGrabbed OnMeshReleasedOnLimit;

	/* Current position along the constraint for this slidable static mesh. */
	UPROPERTY(BlueprintReadOnly, Category = "Slidable")
	float currentPosition;

private:

	FVector originalGrabLocation; /* The original relative grab offset from the hand to the slidable to prevent snapping on grab. */
	float maxRelativeLoc, minRelativeLoc; /* The min and max relative location to use depending on settings, Calculated on begin play. */
	bool interpolating; /* Interpolation enabled/disabled. */
	float interpolationSpeed; /* The speed to interpolate at. */
	float relativeInterpolationPos;	/* The relative location along the selected sliding axis to interpolate to if interpolateOnRelease it true. */

protected:

	/* Level start. */
	virtual void BeginPlay() override;

public:

	/* Constructor. */
	USlidableStaticMesh();

	/* Frame. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

#if WITH_EDITOR
	/* Post edit change. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent & PropertyChangedEvent) override;
#endif

	/* Updates the min and max limits depending on what the current slidable options are. */
	void UpdateConstraintBounds();

	/* Update this slidables position relative to the original relative grab offset within the relative limits of the selected sliding axis. */
	void UpdateSlidable();

	/* Returns the closes vector relative location along the clamped axis limits from the input variable position. */
	UFUNCTION(BlueprintCallable, Category = "Slidable")
	FVector ClampPosition(FVector position);

	/* Set a slidable position along the current axis.
	 * @Param positionAlongAxis, The position along the current axis to go to in Local space.
	 * @Param interpolate, weather or not to interpolate to the slidable position. 
	 * @Param interpSpeed, The speed in which to interpolate the position at. */
	UFUNCTION(BlueprintCallable, Category = "Slidable")
	void SetSlidablePosition(float positionAlongAxis, bool interpolate = false, float interpSpeed = 8.0f);

	/* Implementation of the hands interface. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;
	virtual void Overlapping_Implementation(AVRHand* hand) override;
	virtual void EndOverlapping_Implementation(AVRHand* hand) override;

	/*  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};
