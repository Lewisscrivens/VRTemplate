// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/HandsInterface.h"
#include "PeelableSplineActor.generated.h"

/* Declare log type for a peelable spline actor. */
DECLARE_LOG_CATEGORY_EXTERN(LogPeelable, Log, All);

/* Declare this components delegates. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPeelableEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPeelableSection, int, section, bool, up);

/* Include classes used. */
class USceneComponent;
class USplineComponent;
class USplineMeshComponent;
class USphereComponent;
class UStaticMesh;
class AVRHand;
class UHapticFeedbackEffect_Base;
class USoundBase;
class UMaterialInterface;

/* Enum to differ between different regeneration types for the spline. */
UENUM(BlueprintType)
enum class ERegenType : uint8
{
	Default,
	Grabbed,
};

/* An actor that can be grabbed and peeled away from its start state. NOTE: Use-full for tape, tearing card etc. 
*  NOTE: Ignore ECC_Destructible to ignore the splineMeshes. */
UCLASS()
class NINETOFIVE_API APeelableSplineActor : public AActor, public IHandsInterface
{
	GENERATED_BODY()

public:

	/* Root component to hold spline component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		USceneComponent* root;

	/* The spline component to hold the peelable spline meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		USplineComponent* peelableSpline;

	/* Sphere collision to act as the grabbable area of this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		USphereComponent* grabArea;

	/* The spline component to curve the held part of the already peeled spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		USplineComponent* grabCurveSpline;

	/* The spline mesh to use at the start of the spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		UStaticMesh* splineStartMesh;

	/* The spline mesh to use at the end of the spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		UStaticMesh* splineEndMesh;

	/* The spline mesh to repeat between the start and end mesh along the spline. NOTE: Requires 3 or more spline points on the peelable spline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		UStaticMesh* splineMiddleMesh;

	/* Material to be used for the spline meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Peelable")
		UMaterialInterface* splineMeshMaterial;

	/* Curve to apply the haptic feedback effect to the controller while grabbed.
	 * NOTE: Intensity is multiplied by the movement velocity hand. 
	 * NOTE: If null no haptic feedback is played. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		UHapticFeedbackEffect_Base* hapticCurve;

	/* Sound to play while grabbed and dragging peelable spline. 
	 * NOTE: Intensity and pitch of sound is multiplied by the movement velocity hand.
	 * NOTE: If null no sound is played. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		USoundBase* peelSound;

	/* Regenerate the spline using the splineMeshDistance and tapeSections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable|Spline Defaults")
		bool regenerateSpline;

	/* Point the end spline meshes attatched to the spline down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable|Spline Defaults")
		bool pointEndsDown;

	/* The default size in-between each spline point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable|Spline Defaults")
		float splineMeshDistance;

	/* The number of spline points to generate by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable|Spline Defaults")
		int tapeSections;

	/* Are debugging logs enabled on this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Peelable")
		bool debug;

	/* The interfaces interactable settings for how to interact with VR controllers/hands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
		FHandInterfaceSettings interactableSettings;

	//////////////////////////
	//	Peelable delegates  //
	//////////////////////////

	/* Peelable peeled another section. 
	 * @Int section, The index of the peelable spline point peeled up. 
	 * @Bool up,	 True: Was peeled up.	False: stuck back down. */
	UPROPERTY(BlueprintAssignable)
		FPeelableSection OnPeeled;

	/* Peelable grabbed. */
	UPROPERTY(BlueprintAssignable)
		FPeelableEvent OnPeelableGrabbed;

	/* Peelable released. */
	UPROPERTY(BlueprintAssignable)
		FPeelableEvent OnPeelableReleased;

	/* Called when peelable is fully peeled after being released from the hand. PostUpdate. */
	UPROPERTY(BlueprintAssignable)
		FPeelableEvent OnPeelableComplete;

private:

	AVRHand* handRef; /* Stored reference to the hand if this component is grabbed. */

	FTransform originalSplineTransform; /* Original relative transform of the spline to its parent. */
	FTransform controllerTransform; /* Current frames controller transform if handRef is not nullptr. */
	FVector originalGrabOffset; /* Original relative grab location offset of the hands controller to the spline mesh side being grabbed. */
	FVector worldGrabOffset; /* Current intended grab offset location for the spline mesh side being grabbed. */
	
	bool beenPeeled; /* Set to true once the component is peeled. */
	int detachedSplineEnd; /* Index up to what spline point. */
	int numOfOverlaps; /* Keep track of how many parts of the spline is being overlapped by the hand. */

protected:

	/* Stored references to the currently spawned spline meshes.
	 * NOTE: UPROPERTY prevents garbage collision during in editor modifications to this variable. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Peelable")
	TArray<USplineMeshComponent*> splineMeshes; 

	/* Level start. */
	virtual void BeginPlay() override;

#if WITH_EDITOR
	/* Property changed in this actor. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif

	/* Refresh all spline meshes and the spline itself.
	 * NOTE: If index is a valid index within the splineMeshes array this function will only update that single index. */
	void RefreshSplineMeshesFromSpline(int index = -1);

public:	

	/* Constructor. */
	APeelableSplineActor();

	/* Frame. */
	virtual void Tick(float DeltaTime) override;

	/* Regenerate a given spline point back to its default position after a full regenerate spline point from defaults.
	 * @Param indexToReset, index of spline point to be regenerated to default. */
	UFUNCTION(BlueprintCallable, Category = "Peelable")
	void RegenerateSplinePoint(int indexToReset);

	/* Regenerate the spline component from the default values in the "Spline Defaults" subcategory of this peelable actor.
	 * NOTE: Also binded to the editor widget RegenerateSpline button.
	 * NOTE: Also ran when released from hand when not fully peeled. 
	 * @Param regenType, 
	 * ERegenType::Default, Regenerate the spline peelable into its default state.
	 * ERegenType::Grabbed, Regenerate the spline peelable between the hand, current peel state and the end of the spline. */
	UFUNCTION(BlueprintCallable, Category = "Peelable")
	void RegenerateSplineFromDefaults(ERegenType regenType);

	/* Constructor called when anything is changed on the spline mesh or added etc. */
	virtual void OnConstruction(const FTransform& Transform) override;

	/* Get the collection of currently spawned spline meshes. */
	UFUNCTION(BlueprintCallable, Category = "Peelable")
	TArray<USplineMeshComponent*>& GetGeneratedSplineMeshes();

	/* When hand starts overlap on any spline mesh. */
	UFUNCTION(Category = "Peelable|Collision")
	void OverlapDetected(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/* When hand ends overlap on any spline mesh. */
	UFUNCTION(Category = "Peelable|Collision")
	void OverlapEnded(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/* Implementation of the hands interface. */
	virtual void GrabPressed_Implementation(AVRHand* hand) override;
	virtual void GrabReleased_Implementation(AVRHand* hand) override;
	virtual void Dragging_Implementation(float deltaTime) override;

	/*  Get and set functions to allow changes from blueprint. */
	virtual FHandInterfaceSettings GetInterfaceSettings_Implementation() override;
	virtual void SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings) override;
};
