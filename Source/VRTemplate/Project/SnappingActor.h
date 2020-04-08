// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Player/VRPhysicsHandleComponent.h"
#include "Globals.h"
#include "SnappingActor.generated.h"

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogSnappingActor, Log, All);

/* Define this actors delegate for when something is snapped to this actor. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSnapppedGrabbableActor, AGrabbableActor*, grabbableActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSnappped, UPrimitiveComponent*, componentSnapped, FVector, snappingLocation, FRotator, snappingRotation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnsnapped, UPrimitiveComponent*, componentSnapped);

/* Declare classes used. */
class UBoxComponent;
class AGrabbableActor;
class UMaterialInterface;
class UGrabbableSkelMesh;
class UMeshComponent;
class AVRHand;
class UPrimitiveComponent;
class USoundBase;
class UHapticFeedbackEffect_Base;

/* Enum for different snapping modes. */
UENUM(BlueprintType)
enum class ESnappingMode : uint8
{
	Instant UMETA(DisplayName = "Instant", ToolTip = "Grabbables will instantly snap into position."),
	Interpolate UMETA(DisplayName = "Interpolate", ToolTip = "Grabbables will interpolate into position at the interpSpeed."),
	InstantOnRelease UMETA(DisplayName = "InstantOnRelease", ToolTip = "Grabbables will instantly snap into position when let go within the box snapping component."),
	InterpolateOnRelease UMETA(DisplayName = "InterpolateOnRelease", ToolTip = "Grabbables will interpolate into position at the interpSpeed when let go within the box snapping component."),
	PhysicsOnRelease UMETA(DisplayName = "PhysicsOnRelease", ToolTip = "Grabbables will interpolate into a physics handle on release."),
};

/* Different interpolating modes. */
UENUM(BlueprintType)
enum class EInterpMode : uint8
{
	Disabled, /* The interpolation is disabled in tick. */
	Interpolate, /* Interpolate the preview comp to the center of the snapping component with any offset. */
	Returning, /* Interpolate the preview comp back to the grabbed position after overlap is ended. */
	InterpolateOverlapping, /* Interpolate the overlapping object to the center of the snapping component with any offset. */
};

/* Enum for differentiating between different preview mesh setups. */
UENUM(BlueprintType)
enum class EPreviewMeshSetup : uint8
{
	GrabbableActor,
	GrabbableSkelMesh,
};

/* Actor with a Box component that will snap any grabbable that overlaps into its center position + the location and rotation 
 * offset applied to the instance of this class. */
UCLASS()
class VRTEMPLATE_API ASnappingActor : public AActor
{
	GENERATED_BODY()

public:

	/* Box component to detect overlaps with snappable objects. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* snapBox;

	/* Current snapping mode of this snapping component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	ESnappingMode snapMode;

	/* Use this material on the preview mesh of the overlapping grabbable. NOTE: Only used if it is not null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	UMaterialInterface* previewMaterial;

	/* Instantly release from handle on overlap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	bool snatch;

	/* Handle data for the physics handle snapping mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	FPhysicsHandleData physicsHandleSettings;

	/* Should rotate while snapped in place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Rotation")
	bool rotateAroundYaw;

	/* Speed in which to rotate the grabbable snapped into this snappable component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Rotation")
	float rotationSpeed;

	/* How long it takes the preview/mesh to interpolate/lerp into snapping position and back to the hand if need be. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	float timeToInterp;

	/* Already has component snapped within the component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snappable")
	bool full;

	/* The location offset from this box components center location to snap the grabbable to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Offset")
	FVector locationOffset;

	/* The rotation offset from this box components rotation to snap the grabbable to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable|Offset")
	FRotator rotationOffset;

	/* A grabbable actor or actor with a grabbable skeletal mesh component to be snapped to this component on begin play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	AActor* actorToSnap;

	/* This snappable snapping tag which will be checked for overlapping grabbables or grabbable skeletal components. 
	 * NOTE: If NAME_None then every grabbable and grabbable skeletal mesh component will snap to this snapping actor.
	 * NOTE: For grabbableActor add tag to actor, for grabbableSkelMesh add tag to the component.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snappable")
	FName snappingTag;

	/* The preview mesh of whatever is currently overlapping. */
	UPROPERTY(BlueprintReadOnly, Category = "Snappable")
	UMeshComponent* previewComponent;

	/* The component to interpolate to or from the hand. */
	UPROPERTY(BlueprintReadOnly, Category = "Snappable")
	UMeshComponent* componentToInterpolate; 

	/* Grabbable actor that is currently overlapping with this component. */
	UPROPERTY(BlueprintReadOnly, Category = "Snappable")
	AGrabbableActor* overlappingGrabbable;

	/* Grabbable Skeletal Mesh Component that is currently overlapping this component. */
	UPROPERTY(BlueprintReadOnly, Category = "Snappable")
	UGrabbableSkelMesh* overlappingGrabbableSkel;

	/* Pointer to a physics handle component used in the physics modes for snapping. */
	UPROPERTY(BlueprintReadOnly, Category = "Snappable")
	UVRPhysicsHandleComponent* currentHandle;

	////////////////
	/* Delegates. */
	////////////////

	/* Called when something is connected. */
	UPROPERTY(BlueprintAssignable, Category = "Snapping")
	FOnSnapppedGrabbableActor OnSnapConnectGrabbable;

	/* Called when something is connected. */
	UPROPERTY(BlueprintAssignable, Category = "Snapping")
	FOnSnappped OnSnapConnect;

	/* Called when something is disconnected when grabbed. */
	UPROPERTY(BlueprintAssignable, Category = "Snapping")
	FOnUnsnapped OnSnapDisconnect;

private:
	 
	EPreviewMeshSetup compSetup; /* What component type to preview. NOTE: Less expensive than casting multiple times. */
	EInterpMode interpMode; /* The current interpolation mode. */
	FVector originalIntertia; /* Original inertia scale for bone grabbed in physics snap mode. */
	FVector lerpLocation;/* Current lerping location. */
	FRotator lerpRotation;/* Current lerping rotation. */
	FTransform interpStartTransform;/* The starting location and rotation of the lerp. */	
	float interpolationStartTime;/* The starting game time of the lerp. */
	bool componentSnapped; /* Is there a snapped component? */
	bool updateAnim; /* Should update animation of the skeletal mesh that is being snapped to this actor. */

public:

	/* Constructor. */
	ASnappingActor();

	/* Frame. */
	virtual void Tick(float DeltaTime) override;

#if WITH_EDITOR
	/* Handles when certain values in this class can be edited. */
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif

	/* Function binded to current overlapping grabbable.
	 * @Param hand, The hand that grabbed the current overlapping grabbable.
	 * @Param compReleased, The component that was grabbed by the hand. */
	UFUNCTION(Category = "Collision")
	void OnGrabbablePressed(AVRHand* hand, UPrimitiveComponent* compReleased);

	/* Function binded to current overlapping grabbable.
	 * @Param hand, The hand that released the current overlapping grabbable.
	 * @Param compReleased, The component that was released by the hand. */
	UFUNCTION(Category = "Collision")
	void OnGrabbableRealeased(AVRHand* hand, UPrimitiveComponent* compReleased);

	/* Start interpolating the preview mesh or the mesh released/overlapped depending on the mode of interpolation.
	 * @Param mode, The mode of interpolation to put into motion. */
	UFUNCTION(Category = "Collision")
	void StartInterpolation(EInterpMode mode);

	/* Force a component to snap into the snapping actor.
	* @Param snappingActor, The actor to snap to this snappable actor. */
	UFUNCTION(BlueprintCallable)
	void ForceSnap(AActor* snappingActor);

	/* Force release of a snapped object. */
	UFUNCTION(BlueprintCallable)
	void ForceRelease();

protected:

	/* Level start. */
	virtual void BeginPlay() override;

	/* Overlapping Event binded to this classes box component. */
	UFUNCTION(Category = "Collision")
	void OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/* Overlapping Ending Event binded to this classes box component. */
	UFUNCTION(Category = "Collision")
	void OverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/* Function to interpolate a given component into this components location + the offset location & rotation.
	 * @Param rootComponent, the root component that is being interpolated by this function. */
	UFUNCTION(Category = "Visuals")
	void Interpolate(float deltaTime);

	/* Spawn a copy of the rootComponent and its children and set this as the preview mesh and make sure the material is set to previewMaterial. 
	 * @Param comp, The root component of the previewComponent/mesh. 
	 * @Param setupType, Enum to differentiate skeletal mesh setup from static mesh. */
	bool SetupPreviewMesh(UPrimitiveComponent* comp, EPreviewMeshSetup setupType);

	/* Reset the preview mesh back to null and destroy any duplicated children preview meshes. */
	void ResetPreviewMesh();

	/* Create a physics handle and grab the compToAttach.
	 * @Param compToAttatch, The component to attach to the handles joint. 
	 * @Param setupType, The type of setup to use to grab the component. */
	void CreateAndAttatchPhysicsHandle(UPrimitiveComponent* compToAttatch, EPreviewMeshSetup setupType);

	/* Function to destroy the physics handle joint attatched to any given component. */
	void DestroyPhysicsHandle();
};
