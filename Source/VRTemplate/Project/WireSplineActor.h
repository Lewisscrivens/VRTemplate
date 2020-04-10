// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Globals.h"
#include "WireSplineActor.generated.h"

/** Declare log type for a peelable spline actor. */
DECLARE_LOG_CATEGORY_EXTERN(LogWireSpline, Log, All);

/** Include used classes. */
class USceneComponent;
class USplineComponent;
class UStaticMesh;
class UPrimitiveComponent;
class USplineMeshComponent;
class UCapsuleComponent;
class UPhysicsConstraintComponent;
class UPhysicalMaterial;

/** A Spline component that can generate a given mesh along itself, also supports physics simulated splines at runtime if enabled. 
 * TODO: Make tangents target from next body rotations also so the wire visually bends in a more realistic way. 
 * NOTE The reason the start and end locations are updated manually is because it could be attached to something that is switching between
 *      physics enabled and disabled which would break any other means of connection.
*  NOTE: Supports connection to slidable components this class was originally made to act as a wire for a mouse but may be of some use in other cable generation areas... */
UCLASS()
class VRTEMPLATE_API AWireSplineActor : public AActor
{
	GENERATED_BODY()
	
public:

	/** Root component to hold spline component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	USceneComponent* root;

	/** The spline to place the wire spline meshes along. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	USplineComponent* wireSpline;

	/** The wire static mesh to use to make the spline mesh components along this spline wire actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	UStaticMesh* wireMesh;

	/** Physics material applied to physics bodies generated if physics is enabled on this wire actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	UPhysicalMaterial* wirePhysicsMaterial;

	/** Generate physics from spline points on begin play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	bool generatePhysics;

	/** Primitive component to attach start point to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	AActor* startConnection;

	/** Primitive component to attach end point to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	AActor* endConnection;

	/** Stiffness of the wire. Swing velocity to target on each constraint when generating the physics bodies for this wire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	float wireStiffness;

	/** The angular swing1 and swing2 limit to use for all of the spawned constraints when generating the physics bodies for this wire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	float angularConstraintLimit;

	/** The default length/distance in-between each spline point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	float splineMeshLength;

	/** The number of spline meshes to be generated. NOTE: Spline points to generate = splineMeshNo + 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	int splineMeshNo;

	/** Regenerate the spline using the defined properties. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	bool regenerateSplineFully;

	/** Regenerate the spline using the defined properties without changing the current spline shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	bool regenerateSplineKeepShape;

	/** Enable debug logging for this class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WireSpline")
	bool debug;

	/** Storage of the generated spline meshes for this wire component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireSpline")
	TArray<USplineMeshComponent*> generatedSplineMeshes;

	/** Physics shapes generated. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireSpline")
	TArray<UCapsuleComponent*> generatedPhysicsBodies;

	/** Storage of the generated constraints between each generated spline mesh physics bodies. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WireSpline")
	TArray<UPhysicsConstraintComponent*> generatedConstraints;

	/** Storage of the scene component attached to the start location for this wire.. */
	UPROPERTY(BlueprintReadOnly, Category = "WireSpline")
	USceneComponent* startAttachScene;

	/** Storage of the scene component attached to the end location for this wire.. */
	UPROPERTY(BlueprintReadOnly, Category = "WireSpline")
	USceneComponent* endAttachScene;

private:

	/** Was the spline successfully generated before begin play. */
	bool splineSuccesfullyGenerated; 

	/** The start and end attachment locations relative to there parent. 
	 * Note: Used to update locations of start and end scene points. */
	FTransform startAttachmentOffset, endAttachmentOffset;

public:

	/** Constructor. */
	AWireSplineActor();

	/** Frame. */
	virtual void Tick(float DeltaTime) override;

	/** Whenever the component is moved or a value is changed run this function. */
	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	/** Level start. */
	virtual void BeginPlay() override;

#if WITH_EDITOR
	/** Property changed in this component. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif

	/** Regenerate the spline from its current properties. */
	void RegenerateSpline();

	/** Regenerate the spline from its current properties without changing its shape. */
	void RegenerateSplineKeepShape();

	/** Generate and store the spline meshes between each spline point. */
	void GenerateSplineMeshes();

	/** Update spline meshes start and end locations as-well and the visual rendering of the components in the generatedWireMeshes array. */
	void UpdateSplineMeshes();

	/** Generate physics bodies and constraints for each spline point. */
	bool GeneratePhysicsBodies();

	/** Update the spline point locations from the generated physics bodies. */
	void UpdateSplineLocationsFromPhysicsBodies();
};
