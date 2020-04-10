// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Globals.h"
#include "Interactables/SlidableStaticMesh.h"
#include "SnappingSlidableComponent.generated.h"

/** Define this components log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogSnapSlidingComp, Log, All);

/** Define this actors delegate for when something is snapped to this actor. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlidingCompSnapped, class AGrabbableActor*, grabbableActor);

/** Declare used variables types. */
class AVRHand;
class AVRPawn;
class AGrabbableActor;

/** Class designed to snap a grabbable actor to a sliding mesh that can be released to insert things into a given location.
 * NOTE: This class is good for creating floppy disc functionality or inserting coins etc. */
UCLASS(ClassGroup = (Custom), Blueprintable, BlueprintType, PerObjectConfig, EditInlineNew)
class VRTEMPLATE_API USnappingSlidableComponent : public UBoxComponent
{
	GENERATED_BODY()

public:

	/** The location offset from this box components center location to snap the grabbable to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Offset")
	FVector locationOffset;

	/** The rotation offset from this box components rotation to snap the grabbable to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Offset")
	FRotator rotationOffset;

	/** The tag required on a grabbable actor to be snapped into this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	FName snappingTag;

	/** The current relative axis for this sliding component to slide in when grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Slidable")
	ESlideAxis axisToSlide;

	/** The current slidable limit that the slidable will move in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Slidable")
	float slidingLimit;

	/** If lerp to limit is true then this is the time it takes to lerp to the limit position of the slidable static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Slidable", meta = (EditCondition = "lerpToLimitOnRelease"))
	float releasedLerpTime;

	/** The hand that just released the slidable or grabbable actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	AVRHand* handRegrab; 

	/** The intialised sliding static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	USlidableStaticMesh* slidingMesh; 

	/** The current grabbable snapped to the snapping component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slidable")
	AGrabbableActor* snappedGrabbable; 

	////////////////////////////
	//        Delegates       //
	////////////////////////////

	/** Called when something is snapped. */
	UPROPERTY(BlueprintAssignable, Category = "Snapping")
	FOnSlidingCompSnapped OnSnapConnect;

	/** Called when something is unsnapped. */
	UPROPERTY(BlueprintAssignable, Category = "Snapping")
	FOnSlidingCompSnapped OnSnapDisconnect;

private:

	FVector relativeSlidingLerpPos; /** The position for the slidable to lerp to once released. */
	FVector slidingStartLoc; /** The start location of the lerp from current slidable position into its limit. */
	FTransform originalGrabOffset; /** The original grab transform of the grabbable when snapped into place. */
	FTimerHandle updateTimer; /** Timer to check the state of the hand relative to this slidable interactable. */

	bool lerpSlidableToLimit; /** Should lerp the slidable to the limit in the tick function. */
	float interpolationStartTime; /** The start time of the interpolation. */
	

protected:

	/** Level start. */
	virtual void BeginPlay() override;

	/** Initialise the sliding component to connect to on snap connected. */
	void InitSlidingComponent();

	/** Called when the slidable is grabbed by a hand. */
	UFUNCTION(Category = "Snappable|Slidable")
	void OnSlidableGrabbed(AVRHand* hand);

	/** Called when the slidable is released from a hand. */
	UFUNCTION(Category = "Snappable|Slidable")
	void OnSlidableReleased(AVRHand* hand);

	/** Called when the snappedGrabbable is grabbed by a hand. */
	UFUNCTION(Category = "Snappable")
	void OnGrabbableGrabbed(AVRHand* hand, UPrimitiveComponent* compPressed);

public:

	/** Constructor. */
	USnappingSlidableComponent();

	/** Frame. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Function to update any changes in hand distance or releasing interpolation. */
	UFUNCTION(Category = "Update")
	void UpdateSlidableState();

	/** This components overlap begin event. */
	UFUNCTION(Category = "Collision")
	void OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Function used to force snap a component into this snapping component. */
	UFUNCTION(BlueprintCallable, Category = "Snappable")
	void ForceSnap(AGrabbableActor* actorToSnap);
	
	/** Function used to force snap release a grabbable from this component. */
	UFUNCTION(BlueprintCallable, Category = "Snappable")
	void ForceRelease();
};
