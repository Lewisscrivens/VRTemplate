// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Globals.h"
#include "Interactables/RotatableStaticMesh.h"
#include "SnappingRotatableComponent.generated.h"

/** Define this components log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogSnapRotComp, Log, All);

/** Define this actors delegate for when something is snapped to this actor. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRotatingCompSnapped, class AGrabbableActor*, grabbableActor);

/** Delegate definition for reaching the limit. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLimitReached);

/** Declare used variables types. */
class AVRHand;
class AVRPawn;
class AGrabbableActor;
class URotatableStaticMesh;

/** Class designed to snap a grabbable actor to a rotatable mesh that can be twisted to a given rotation to unlock things etc. For example its good for keys in locks. */
UCLASS(ClassGroup = (Custom), Blueprintable, BlueprintType, PerObjectConfig, EditInlineNew)
class VRTEMPLATE_API USnappingRotatableComponent : public UBoxComponent
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

	/** The hand that just released the rotatable or grabbable actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	AVRHand* handRegrab; 

	/** The intialised rotatable static mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	URotatableStaticMesh* rotatableMesh; 

	/** The current grabbable snapped to the snapping component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	AGrabbableActor* snappedGrabbable; 

	/** Lock at the rotatable's limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Twistable")
	bool lockOnLimit;

	/** The current rotational limit that the rotatable static mesh will move in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Twistable")
	float rotatingLimit;

	/** The distance between the component and the grabbing had before its removed from the lock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Twistable")
	float returningDistance;

	/** Sound to play when the rotatable mesh's limit is reached. If nullptr, no unlocking sound will be played. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Twistable")
	USoundBase* limitReachedSound;

	/** Haptic effect to use when the rotatable mesh's limit is reached. NOTE: If nullptr then no haptic event will be played. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Twistable")
	UHapticFeedbackEffect_Base* limitReachedHaptics;

	////////////////////////////
	//        Delegates       //
	////////////////////////////

	/** Called when the twistable has been inserted and twisted to the correct limit. */
	UPROPERTY(BlueprintAssignable, Category = "Snappable")
	FLimitReached limitReachedDel;

	/** Called when the twistable has been inserted and twisted to the correct limit. */
	UPROPERTY(BlueprintAssignable, Category = "Snappable")
	FLimitReached limitExitedDel;

	/** Called when something is snapped. */
	UPROPERTY(BlueprintAssignable, Category = "Snappable")
	FOnRotatingCompSnapped OnSnapConnect;

	/** Called when something is unsnapped. */
	UPROPERTY(BlueprintAssignable, Category = "Snappable")
	FOnRotatingCompSnapped OnSnapDisconnect;

private:

	bool limitReached; /** Has the limit reached delegate been called already? */
	FTransform originalGrabOffset; /** The original grab transform of the grabbable when snapped into place. */
	FTimerHandle updateTimer; /** Timer to check the state of the hand relative to this rotatable interactable. */
	
protected:

	/** Level start. */
	virtual void BeginPlay() override;

	/** Initialise the rotatable component to connect to on snap connected. */
	void InitRotatableComponent();

	/** Function called if in twisting snap mode and a twistable is snapped in then locked while snapped in.
	 * @Param angle, The angle that the rotatable mesh was locked. 
	 * @Param rotatable, The rotatable mesh that was locked. */
	UFUNCTION(Category = "Snappable|Slidable")
	void OnRotatableLocked(float angle, URotatableStaticMesh* rotatable);

	/** Called when the snappedGrabbable is grabbed by a hand. */
	UFUNCTION(Category = "Snappable")
	void OnGrabbableGrabbed(AVRHand* hand, UPrimitiveComponent* compPressed);

public:

	/** Constructor. */
	USnappingRotatableComponent();

	/** Frame. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Function to update any changes in hand rotation relative to the rotatable. */
	UFUNCTION(Category = "Update")
	void UpdateRotatableState();

	/** This components overlap begin event. */
	UFUNCTION(Category = "Collision")
	void OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Function used to force snap a component into this snapping component.
	 * NOTE: So components can be snapped on beginPlay. For example having a key start in a lock without someone having to put it there. */
	UFUNCTION(BlueprintCallable, Category = "Snappable")
	void ForceSnap(AGrabbableActor* actorToSnap);
	
	/** Function used to force snap release a grabbable from this component. */
	UFUNCTION(BlueprintCallable, Category = "Snappable")
	void ForceRelease();
};
