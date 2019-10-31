// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.h"
#include "Project/Globals.h"
#include <PxSimpleTypes.h>
#include "VRPhysicsHandleComponent.generated.h"

/* Declare log type for the VR Physics Handle class. */
DECLARE_LOG_CATEGORY_EXTERN(LogVRHandle, Log, All);

/* Declare classes used within this H file. */
namespace physx
{
	class PxD6Joint;
	class PxRigidDynamic;
}
class UPrimitiveComponent;

/* VR Physics Handle data, holds all of this classes functionality variables. Defaults values are tested with 1kg grabbable assets. 
 * NOTE: Values may need to be higher to have effect on lighter components less than 1KG and heavier may need weaker values to prevent collision issues.
 * NOTE: Mainly used to switch between different functionality using the UpdateJointValues function. */
USTRUCT(BlueprintType)
struct FPhysicsHandleData
{
	GENERATED_BODY()

public:

	/* Should the handle data be used. Only used for instances of this structure outside of this class checked and applied in the hand class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle")
		bool handleDataEnabled;
	/* The linear damping value of the joints linear drive. NOTE: Only used if the soft linear constraint is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		float linearDamping;
	/* The linear stiffness value of the joints linear drive. NOTE: Only used if the soft linear constraint is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		float linearStiffness;
	/* The angular damping value of the joints angular drive. NOTE: Only used if the soft angular constraint is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		float angularDamping;
	/* The angular stiffness value of the joints angular drive. NOTE: Only used if the soft angular constraint is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		float angularStiffness;
	/* The max amount of force the linear drive of the joint can apply. NOTE: Only used if the soft linear constraint is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		float maxLinearForce;
	/* The max amount of force the angular drive of the joint can apply. NOTE: Only used if the soft angular constraint is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		float maxAngularForce;
	/* The interpolation speed of the current target location to the new target location. NOTE: Only used if the interpolation is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		float interpSpeed;
	/* Should use soft angular constraint on the joint for this handle. NOTE: Changes angular constraint to be free and targets angular rotation with the angular drive of the joint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		bool softAngularConstraint;
	/* Should use soft linear constraint on the joint for this handle. NOTE: Changes linear constraint to be free and targets linear position with the linear drive of the joint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		bool softLinearConstraint;
	/* Should interpolate the target location. If true the targetTransform will interpolate from its current value at the interpSpeed, otherwise it will be set immediately. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		bool interpolate;
	/* Should update the handle automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle", meta = (EditCondition = "handleDataEnabled"))
		bool updateTargetLocation;

	/* Constructor for this struct. Defaults to default constraint values. Prioritize linear constrained movement over angular for more realistic collision tracking. */
	/* NOTE: If using something like a sword etc. Use a much lower angular stiffness than the linear stiffness to get that specific effect. */
	FPhysicsHandleData(bool dataEnabled = false, float linDamp = 200.0f, float angDamp = 200.0f, float linStiff = 35000.0f, float angStiff = 30000.0f, float speed = 50.0f,
		bool softAgnConstraint = true, bool softlinConstraint = true, float maxForceLinear = 10000.0f, float maxForceAngular = 10000.0f, bool interpToTarget = true, bool updateHandle = true)
	{
		this->handleDataEnabled = dataEnabled;
		this->linearDamping = linDamp;
		this->angularDamping = angDamp;
		this->linearStiffness = linStiff;
		this->angularStiffness = angStiff;
		this->interpSpeed = speed;
		this->softAngularConstraint = softAgnConstraint;
		this->softLinearConstraint = softlinConstraint;
		this->maxLinearForce = maxForceLinear;
		this->maxAngularForce = maxForceAngular;
		this->interpolate = interpToTarget;
		this->updateTargetLocation = updateHandle;
	}

	/* Update the constraints drive values and force that can be applied linearly and angularly.
	 * @Param softLinear, soft linear constraint?
	 * @Param softAngular, soft angular constraint?
	 * @Param maxForceLinear, max amount of force that can be applied by the joint linearly.
	 * @Param maxForceAngular, max amount of force that can be applied by the joint angularly. 
	 * NOTE: This function will NOT update the current joint if one exists. Either use this and run reinitJoint or use the toggleDrive function. */
	void UpdateJointDrive(bool softLinear, bool softAngular, float maxForceLin = 10000.0f, float maxForceAng = 10000.0f)
	{
		this->softLinearConstraint = softLinear;
		this->softAngularConstraint = softAngular;

		this->maxLinearForce = maxForceLin;
		this->maxAngularForce = maxForceAng;
	}

	/* Convert and return this structure as a string. */
	FString ToString()
	{
		FString handleDataString = FString::Printf(TEXT("Data Enabled = %s \n Linear Damping = %f \n Angular Damping = %f \n Linear Stifness = %f \n Angular Stiffness = %f \n Interp Speed = %f \n Soft Angular Constraint = %s \n Soft Linear Constraint = %s \n Max Linear Force = %f \n Max Angular Force = %f \n Interpolate Target = %s \n Update Target Location = %s"),
			SBOOL(handleDataEnabled),
			linearDamping,
			angularDamping,
			linearStiffness,
			angularStiffness,
			interpSpeed,
			SBOOL(softAngularConstraint),
			SBOOL(softLinearConstraint),
			maxLinearForce,
			maxAngularForce,
			SBOOL(interpolate),
			SBOOL(updateTargetLocation));

		return handleDataString;
	}
};

/* Custom physics handle component modified to work better for use with VR. 
 * NOTE: If any changes are made to handleData, reinitJoint must be ran to apply said changes. */
UCLASS(ClassGroup = Physics, hidecategories = Object, meta = (BlueprintSpawnableComponent))
class VRTEMPLATE_API UVRPhysicsHandleComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:

	/* Default constraint structure containing all options of this constraint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle")
	FPhysicsHandleData handleData;

	/* Component that has been grabbed. Null if nothing is grabbed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PhysicsHandle")
	UPrimitiveComponent* grabbedComponent;

	/* Relative joint location to the grabbed component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PhysicsHandle")
	FTransform jointTransformGrabbable; 

	/* Name of the grabbed bone name if there is a grabbed component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PhysicsHandle")
	FName grabbedBoneName;

	/* Should use the target component to update the target location. (Needs to be done in tick.) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PhysicsHandle")
	bool updateTargetRotation;

	/* Print visual debug points. 
	 * Blue Point = The target location of the joint. 
	 * Also logs when the joint values are updated and what values it is updated with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysicsHandle")
	bool debug;

	float constraintLinearMaxForce; /* If linear soft constraint is enabled, this is the max amount of force that can be added to get to the constraints target location. */
	float constraintAgularMaxForce; /* If angular soft constraint is enabled, this is the max amount of force that can be added to get to the constraints target rotation. */
	FTransform targetTransform; /* Target location of the KinActor. */
	FTransform currentTransform; /* Current location of the KinActor. */
	bool grabOffset; /* Is the current grab offset enabled from the target component to the grabbed location. */

public:

	/* Frame. Start of physics. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// BP //
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocation to be ran to update the current joints location...
	 * NOTE: Equivalent of normal physics handles GrabComponentAtLocation function.
	 * @Param comp, The component to be constrained to the handle location. 
	 * @Param target, The target component to be followed relative to the grabLocation.
	 * @Param boneName, The bone of given component to be constrained. 
	 * @Param jointLocation, The reference location in the world to create the joint at. 
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle", meta = (AutoCreateRefTerm = "interactableData"))
	void K2_CreateJointAndFollowLocationTarget(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, FVector jointLocation,
		FPhysicsHandleData interactableData);

	// CPP // 
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocation to be ran to update the current joints location...
	 * NOTE: Equivalent of normal physics handles GrabComponentAtLocation function.
	 * @Param comp, The component to be constrained to the handle location.
	 * @Param target, The target component to be followed relative to the grabLocation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param jointLocation, The reference location in the world to create the joint at.
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	void CreateJointAndFollowLocation(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, FVector jointLocation,
		FPhysicsHandleData interactableData = FPhysicsHandleData());

	// BP //
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocation to be ran to update the current joints location...
	 * NOTE: You will need to handle the target of the joint in a tick function as it is not done automatically when no target component is passed into this function.
	 * @Param comp, The component to be constrained to the handle location.
	 * @Param target, The target component to be followed relative to the grabLocation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param jointLocation, The reference location in the world to create the joint at.
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle", meta = (AutoCreateRefTerm = "interactableData"))
	void K2_CreateJointAndFollowLocation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FPhysicsHandleData interactableData);

	// CPP // 
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocation to be ran to update the current joints location...
	 * NOTE: You will need to handle the target of the joint in a tick function as it is not done automatically when no target component is passed into this function.
	 * @Param comp, The component to be constrained to the handle location.
	 * @Param target, The target component to be followed relative to the grabLocation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param jointLocation, The reference location in the world to create the joint at.
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	void CreateJointAndFollowLocation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FPhysicsHandleData interactableData = FPhysicsHandleData());

	// BP //
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocationAndRotation to be ran to update the current joints location & rotation...
	 * NOTE: Equivalent of normal physics handles GrabComponentAtLocationWithRotation function.
	 * @Param comp, The component to be constrained to the handle location & rotation.
	 * @Param target, The target component to be followed relative to the grabLocation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param jointLocation, The reference location in the world to create the joint at.
	 * @Param jointOrientation, The reference rotation in the world to create the joint at. 
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle", meta = (AutoCreateRefTerm = "interactableData"))
	void K2_CreateJointAndFollowLocationWithRotationTarget(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, FVector jointLocation, 
		FRotator jointOrientation, FPhysicsHandleData interactableData);

	// CPP //
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocationAndRotation to be ran to update the current joints location & rotation...
	 * NOTE: Equivalent of normal physics handles GrabComponentAtLocationWithRotation function.
	 * @Param comp, The component to be constrained to the handle location & rotation.
	 * @Param target, The target component to be followed relative to the grabLocation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param jointLocation, The reference location in the world to create the joint at.
	 * @Param jointOrientation, The reference rotation in the world to create the joint at.
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	void CreateJointAndFollowLocationWithRotation(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, FVector jointLocation,
		FRotator jointOrientation, FPhysicsHandleData interactableData = FPhysicsHandleData());

	// BP //
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocationAndRotation to be ran to update the current joints location & rotation...
	 * NOTE: You will need to handle the target of the joint in a tick function as it is not done automatically when no target component is passed into this function.
	 * @Param comp, The component to be constrained to the handle location & rotation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param jointLocation, The reference location in the world to create the joint at.
	 * @Param jointOrientation, The reference rotation in the world to create the joint at.
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle", meta = (AutoCreateRefTerm = "interactableData"))
	void K2_CreateJointAndFollowLocationWithRotation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FRotator jointOrientation,
		FPhysicsHandleData interactableData);

	// CPP //
	/* Create a joint between the physics handle and the given component. Requires SetTargetLocationAndRotation to be ran to update the current joints location & rotation...
	 * NOTE: You will need to handle the target of the joint in a tick function as it is not done automatically when no target component is passed into this function.
	 * @Param comp, The component to be constrained to the handle location & rotation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param jointLocation, The reference location in the world to create the joint at.
	 * @Param jointOrientation, The reference rotation in the world to create the joint at.
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	void CreateJointAndFollowLocationWithRotation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FRotator jointOrientation, 
		FPhysicsHandleData interactableData = FPhysicsHandleData());

	/* Destroy the joint created when grabbed. NOTE: Equivalent of normal physics handles releaseComponent function. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void DestroyJoint();

	/* Returns the current expected location of the joint in world space relative to the target component. 
	 * NOTE: Only works if grabbed with a target component in mind. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	FTransform GetTargetLocation();

	/* Returns the target offset from the target component. Will be 0 if the grab location was the same as the target components location for example. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	FTransform GetTargetOffset();

	/* Enable or disable the current joints drive.
	 * @Param linearDrive, Enable/Disable the linear drive of the current joint of this handle.
	 * @Param angularDrive, Enable/Disable the angular drive of the current joint of this handle.*/
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void ToggleDrive(bool linearDrive, bool angularDrive);

	/* Update all of the constraints values.
	 * @Param newData, A physics handle data structure that holds all values within this class. NOTE: Declared in the globals.h file. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void UpdateJointValues(FPhysicsHandleData newData);

	/* Reset the joint back to its original handleData (Physics handle variables) values that were originally initialized on begin play. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void ResetJoint();

	/* Used to update the target location if its not automatically being updated.
	 * @Param newTargetTransform, The new Location/Rotation of the target of this joint in world space.
	 * @Param updateTransformInstantly, should the target transform of the joint be updated when target is set. (If false it is updated next frame). NOTE: False by default. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void SetTarget(FTransform newTargetTransform, bool updateTransformInstantly = false);

	/* Reinitialize the joint and drive values if something is grabbed from the current handleData. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void ReinitJoint();

	/* Toggle rotation constraint on or off after it has been grabbed with rotation while its still grabbed/constrained. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void ToggleRotationConstraint(bool on);

	/* Returns the current rotation constraint value. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	bool IsRotationConstrained();

	/* Updates the target rotation of this joint/constraint/physicsHandle. NOTE: Disable updateTargetRotation before using this in a tick function. */
	UFUNCTION(BlueprintCallable, Category = "Physics|Components|VRPhysicsHandle")
	void UpdateHandleTargetRotation(FRotator updatedRotation);

protected:
	
	physx::PxD6Joint* joint; /* Pointer to PhysX joint created when a component is grabbed. */
	physx::PxRigidDynamic* targetActor; /* Pointer to target actor created in the create joint function to constrain a given component to. */
	UPrimitiveComponent* targetComponent; /* Pointer to the tracked component so the positions can be updated within this class. */
	FTransform targetOffset; /* Relative offset transform from the target component that the constraint was initialized / positioned. */
	bool rotationConstraint; /* Is the rotation constraint currently active. */
	FPhysicsHandleData originalData; /* Original physics handle data of this class, in case its replaced on creating the constraint. */

	/* Unregister this component. */
	void OnUnregister();

	/* Level start. Used to save the original data. */
	virtual void BeginPlay() override;
	
	/* Function to create a  physics target actor and a joint between itself and the given component.
	 * NOTE: The targetActors location must be updated to move the constraint. Also ONLY use CreateJointAndFollowLocation OR CreateJointAndFollowLocationWithRotation.
	 * @Param comp, The component to be constrained to the handle location & rotation.
	 * @Param target, The target component to be followed relative to the grabLocation.
	 * @Param boneName, The bone of given component to be constrained.
	 * @Param grabLocation, The reference location in the world to create the joint at.
	 * @Param grabOrientation, The reference rotation in the world to create the joint at.
	 * @Param constrainRotation, Should the joint update any rotational values when constrained.
	 * @Param interactableData, A pointer to the FPhysicsHandleData structure to be used when grabbing comp, NOTE: handleDataEnabled needs to be true. */
	void CreateJoint(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, const FVector& grabLocation, const FRotator& grabOrientation,
		bool constrainRotation = false, FPhysicsHandleData interactableData = FPhysicsHandleData());

	/* Update the transform transform of the joint if one currently exists.
	 * @Param updatedTransform, The new updated location/rotation for the transform. */
	void UpdateHandleTransform(const FTransform& updatedTransform);
};
