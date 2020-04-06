// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/VRPhysicsHandleComponent.h"
#include "EngineDefines.h"
#include "PhysxUserData.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsPublic.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "PhysXIncludes.h"
#include "DrawDebugHelpers.h"

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif 

DEFINE_LOG_CATEGORY(LogVRHandle);

UVRPhysicsHandleComponent::UVRPhysicsHandleComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 	bAutoActivate = true;
 	PrimaryComponentTick.bCanEverTick = true;
 	PrimaryComponentTick.TickGroup = TG_PrePhysics;
 
 	// Default setup.
 	handleData = FPhysicsHandleData();
 	rotationConstraint = false;
 	updateTargetRotation = true;
 	grabOffset = true;
 	grabbedComponent = nullptr;
 	targetComponent = nullptr;
 	grabbedBoneName = NAME_None;
}

void UVRPhysicsHandleComponent::OnUnregister()
{
 	if (grabbedComponent)
 	{
 		DestroyJoint();
 	}
 
 #if WITH_PHYSX
 	if (joint)
 	{
 		check(targetActor);
 
 		// use correct scene
 		PxScene* jointScene = joint->getScene();
 		if (jointScene)
 		{
 			jointScene->lockWrite();
 
 			// destroy joint
 			joint->release();
 			joint = NULL;
 
 			// Destroy temporary actor.
 			targetActor->release();
 			targetActor = NULL;
 
 			jointScene->unlockWrite();
 		}
 	}
 #endif // WITH_PHYSX

	Super::OnUnregister();
}

void UVRPhysicsHandleComponent::BeginPlay()
{
	Super::BeginPlay();
 
 	// Save the original handle data.
 	originalData = handleData;
}

void UVRPhysicsHandleComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
 
 	// If the targetComponent is valid update the target transform to the new target location.
 	if (handleData.updateTargetLocation && targetComponent)
 	{
 		if (grabOffset)
 		{
 			targetTransform.SetLocation(targetComponent->GetComponentTransform().TransformPositionNoScale(targetOffset.GetLocation()));
 			if (updateTargetRotation) targetTransform.SetRotation(targetComponent->GetComponentTransform().TransformRotation(targetOffset.GetRotation()));
 		}
 		else
 		{
 			targetTransform.SetLocation(targetComponent->GetComponentLocation());
 			if (updateTargetRotation) targetTransform.SetRotation(targetComponent->GetComponentRotation().Quaternion());
 		}
 	}
 
 	// If interpolation has been enabled perform blend between the current transform and the target at the given interpolation speed.
 	if (handleData.interpolate)
 	{
 		float alpha = FMath::Clamp(DeltaTime * handleData.interpSpeed, 0.0f, 1.0f);
 		FTransform normalisedCurrent = currentTransform;
 		FTransform normalisedTarget = targetTransform;
 		normalisedCurrent.NormalizeRotation();
 		normalisedTarget.NormalizeRotation();
 		currentTransform.Blend(normalisedCurrent, normalisedTarget, alpha);
 	}
 	// Otherwise if not interpolating just set new target transform instantly.
 	else
 	{
 		currentTransform = targetTransform;
 	}
 
 	// Update the transform of the physics handle.
 	UpdateHandleTransform(currentTransform);
}

void UVRPhysicsHandleComponent::K2_CreateJointAndFollowLocationTarget(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, 
	FVector jointLocation, FPhysicsHandleData interactableData)
{
	CreateJointAndFollowLocation(comp, target, boneName, jointLocation, interactableData);
}

void UVRPhysicsHandleComponent::CreateJointAndFollowLocation(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, 
	FVector jointLocation, FPhysicsHandleData interactableData)
{
	CreateJoint(comp, target, boneName, jointLocation, FRotator::ZeroRotator, false, interactableData);
}

void UVRPhysicsHandleComponent::K2_CreateJointAndFollowLocation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FPhysicsHandleData interactableData)
{
	CreateJointAndFollowLocation(comp, boneName, jointLocation, interactableData);
}

void UVRPhysicsHandleComponent::CreateJointAndFollowLocation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FPhysicsHandleData interactableData)
{
	CreateJoint(comp, nullptr, boneName, jointLocation, FRotator::ZeroRotator, false, interactableData);
}

void UVRPhysicsHandleComponent::K2_CreateJointAndFollowLocationWithRotationTarget(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName,
	FVector jointLocation, FRotator jointOrientation, FPhysicsHandleData interactableData)
{
	CreateJointAndFollowLocationWithRotation(comp, target, boneName, jointLocation, jointOrientation, interactableData);
}

void UVRPhysicsHandleComponent::CreateJointAndFollowLocationWithRotation(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, 
	FVector jointLocation, FRotator jointOrientation, FPhysicsHandleData interactableData)
{
	CreateJoint(comp, target, boneName, jointLocation, jointOrientation, true, interactableData);
} 

void UVRPhysicsHandleComponent::K2_CreateJointAndFollowLocationWithRotation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FRotator jointOrientation, FPhysicsHandleData interactableData)
{
	CreateJointAndFollowLocationWithRotation(comp, boneName, jointLocation, jointOrientation, interactableData);
}

void UVRPhysicsHandleComponent::CreateJointAndFollowLocationWithRotation(UPrimitiveComponent* comp, FName boneName, FVector jointLocation, FRotator jointOrientation, FPhysicsHandleData interactableData)
{
	CreateJoint(comp, nullptr, boneName, jointLocation, jointOrientation, true, interactableData);
}

void UVRPhysicsHandleComponent::CreateJoint(UPrimitiveComponent* comp, UPrimitiveComponent* target, FName boneName, const FVector& grabLocation, 
	const FRotator& grabOrientation, bool constrainRotation, FPhysicsHandleData interactableData)
{
 	// If we are already holding something - drop it first.
  	if (grabbedComponent != NULL) DestroyJoint();
 	// If the component is null throw log and return.
 	CHECK_RETURN(LogVRHandle, !comp, "The VR Physics Handle %s, cannot create a joint as the given component is null.", *GetName());
 
 #if WITH_PHYSX && PHYSICS_INTERFACE_PHYSX
 	// Get the PxRigidDynamic that we want to grab.
 	FBodyInstance* BodyInstance = comp->GetBodyInstance(boneName);
 	if (!BodyInstance)
 	{
 		return;
 	}

	// Get actor handle.
	const FPhysicsActorHandle& ActorHandle = BodyInstance->GetPhysicsActorHandle();
 	FPhysicsCommand::ExecuteWrite(ActorHandle, [&](const FPhysicsActorHandle& Actor)
 	{
 		if (PxRigidActor* phsyActor = FPhysicsInterface::GetPxRigidActor_AssumesLocked(Actor))
 		{
 			PxScene* scene = phsyActor->getScene();
 
 			// If a valid handle has been passed into this function use it as this joints data.
 			if (interactableData.handleDataEnabled) handleData = interactableData;
 
 			// Create the transform location of the joint to be created.
 			PxTransform grabbedActorPose = phsyActor->getGlobalPose();
 			PxTransform jointTransform(U2PVector(grabLocation), U2PQuat(grabOrientation.Quaternion()));
 
 			// Ensure the target and current transform are the same on initial creation of this joint.
 			targetTransform = currentTransform = P2UTransform(jointTransform);
 
 			// If we don't already have a handle make one now.
 			if (!joint)
 			{
 				// Create kinematic actor we are going to create joint with. This will be moved around with calls to SetLocation/SetRotation.
 				PxRigidDynamic* newTarget = scene->getPhysics().createRigidDynamic(jointTransform);
 				newTarget->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
 				newTarget->setMass(1.0f);
 				newTarget->setMassSpaceInertiaTensor(PxVec3(1.0f, 1.0f, 1.0f));
 				newTarget->userData = NULL;
 
 				// Add to Scene and Update data reference.
 				scene->addActor(*newTarget);
  				targetActor = newTarget;
 
 				// Create the joint with the new joint transform.
 				PxD6Joint* newJoint = PxD6JointCreate(scene->getPhysics(), targetActor, PxTransform(PxIdentity), phsyActor, grabbedActorPose.transformInv(jointTransform));
 				if (!newJoint)
 				{
 					joint = 0;
 				}
 				else
 				{
 					// Setup the joint properties.
 					newJoint->userData = NULL;
 					joint = newJoint;
 					rotationConstraint = constrainRotation;
 					ReinitJoint();
 				}
 			}
 		}
 	});
 
 #endif // WITH_PHYSX
 
 	// Save variables to keep track of grabbed state.
 	grabbedComponent = comp;
 	grabbedBoneName = boneName;
 
 	// Save the offset of the joint from the grabbed component.
 	FTransform grabbedCompTransform = grabbedComponent->GetComponentTransform();
 	jointTransformGrabbable.SetLocation(grabbedCompTransform.InverseTransformPositionNoScale(grabLocation));
 	jointTransformGrabbable.SetRotation(grabbedCompTransform.InverseTransformRotation(grabOrientation.Quaternion()));
 	
 	// If there is a target component get the relative transform and follow this in this components tick function.
 	if (target)
 	{
 		// Save the target comp and Find the relative offset from the target component to be updated in tick.
 		targetComponent = target;
 		FVector targetLocOffset = targetComponent->GetComponentTransform().InverseTransformPositionNoScale(grabLocation);
 		FRotator targetRotOffset = targetComponent->GetComponentTransform().InverseTransformRotation(grabOrientation.Quaternion()).Rotator();
 		targetOffset = FTransform(targetRotOffset, targetLocOffset, targetTransform.GetScale3D());
 	}
 	// Otherwise disable update target location.
 	else handleData.updateTargetLocation = false;
}

void UVRPhysicsHandleComponent::DestroyJoint()
{
 	// Destroy all physX actors.
 #if WITH_PHYSX
 	if (grabbedComponent)
 	{
 		if (joint)
 		{
 			check(targetActor);
 
 			// use correct scene
 			PxScene* jointScene = joint->getScene();
 			if (jointScene)
 			{
 				SCOPED_SCENE_WRITE_LOCK(jointScene);
 				// Destroy joint.
 				joint->release();
 
 				// Destroy temporary actor.
 				targetActor->release();
 
 			}
 			targetActor = NULL;
 			joint = NULL;
 		}
 
 		// Reset any grabbed pointers/variables also.
 		handleData = originalData;
 		grabbedComponent->WakeRigidBody(grabbedBoneName);
 		grabbedComponent = NULL;
 		grabbedBoneName = NAME_None;
 
 		// Reset transforms.
 		jointTransformGrabbable = FTransform();
 		currentTransform = FTransform();
 		targetTransform = FTransform();
 	}
 #endif
}

FTransform UVRPhysicsHandleComponent::GetTargetLocation()
{
	// Update target to where it would be.
	FTransform newTransform;
	if (targetComponent)
	{
		// Get the correct offset.
		if (grabOffset)
		{
			newTransform.SetLocation(targetComponent->GetComponentTransform().TransformPositionNoScale(targetOffset.GetLocation()));
			newTransform.SetRotation(targetComponent->GetComponentTransform().TransformRotation(targetOffset.GetRotation()));
		}
		else
		{
			newTransform.SetLocation(targetComponent->GetComponentLocation());
			newTransform.SetRotation(targetComponent->GetComponentRotation().Quaternion());
		}

		// Copy scale. (Doesn't really do anything...)
		newTransform.SetScale3D(currentTransform.GetScale3D());
	}

	return newTransform;
}

FTransform UVRPhysicsHandleComponent::GetTargetOffset()
{
	return targetOffset;
}

void UVRPhysicsHandleComponent::SetTarget(FTransform newTargetTransform, bool updateTransformInstantly)
{
 	CHECK_RETURN(LogVRHandle, handleData.updateTargetLocation, "The Physics Handle %s, Cannot update target location as its currently being updated in the tick function.", *GetName());
 	
 	// Update target transform of the current joint.
 	targetTransform = newTargetTransform;
 	
 	// Update the transform now.
 	if (updateTransformInstantly)
 	{
 		currentTransform = targetTransform;
 		UpdateHandleTransform(currentTransform);
 	}
}

void UVRPhysicsHandleComponent::UpdateHandleTransform(const FTransform& updatedTransform)
{
 	if (!targetActor)
 	{
 		return;
 	}
 
 #if WITH_PHYSX
 	PxScene* targetScene = targetActor->getScene();
 	SCOPED_SCENE_WRITE_LOCK(targetScene);
 
 	// Has the new transform location been changed enough to apply.
 	PxVec3 newTargetLoc = U2PVector(updatedTransform.GetTranslation());
 	PxVec3 currentTargetLoc = targetActor->getGlobalPose().p;
 	bool changedPos = true;
 	if ((newTargetLoc - currentTargetLoc).magnitudeSquared() <= 0.01f * 0.01f)
 	{ 
 		newTargetLoc = currentTargetLoc;
 		changedPos = false;
 	}
 
 	// Has the new transform rotation been changed enough to apply orientation change.
 	PxQuat newTargetOrientation = U2PQuat(updatedTransform.GetRotation());
 	PxQuat currentTargetOrientation = targetActor->getGlobalPose().q;
 	bool changedRot = true;
 	if ((FMath::Abs(newTargetOrientation.dot(currentTargetOrientation)) > (1.0f - SMALL_NUMBER)))
 	{
 		newTargetOrientation = currentTargetOrientation;
 		changedRot = false;
 	}
 
 	// If the location or rotation has been changed apply new kinematic target to the PhysX Actor.
 	if (changedPos || changedRot)
 	{
 		targetActor->setKinematicTarget(PxTransform(newTargetLoc, newTargetOrientation));
 	}
 
 #if WITH_EDITOR
 	// Log the new hand target location as a blue point.
 	if (debug) DrawDebugPoint(GetWorld(), updatedTransform.GetTranslation(), 5.0f, FColor::Blue, true, 0.1f, 0.0f);
 #endif // END EDITOR
 #endif // END PhysX
}

void UVRPhysicsHandleComponent::ToggleDrive(bool linearDrive, bool angularDrive)
{
	// Toggle on or off the current angular/linear drive.
	handleData.softLinearConstraint = linearDrive;
	handleData.softAngularConstraint = angularDrive;

	// Apply changes if joint already exists.
	ReinitJoint();
}

void UVRPhysicsHandleComponent::UpdateJointValues(FPhysicsHandleData newData)
{
	// Update the handle data.
	handleData = newData;

	// Apply changes to the constraint.
	ReinitJoint();
}

void UVRPhysicsHandleComponent::ResetJoint()
{
	UpdateJointValues(originalData);
}

void UVRPhysicsHandleComponent::ReinitJoint()
{
 	// Re-Initialise the constraints joint setup and drives.
 #if WITH_PHYSX
 	if (joint != nullptr)
 	{
 		// Re-initialize the joint created when grabbed. (Setup weather the constraint is soft or stiff)
 		PxD6Motion::Enum const LocationMotionType = handleData.softLinearConstraint ? PxD6Motion::eFREE : PxD6Motion::eLOCKED;
 		PxD6Motion::Enum const RotationMotionType = (handleData.softAngularConstraint || !rotationConstraint) ? PxD6Motion::eFREE : PxD6Motion::eLOCKED;
 
 		// Linear motion for handle.
 		joint->setMotion(PxD6Axis::eX, LocationMotionType);
 		joint->setMotion(PxD6Axis::eY, LocationMotionType);
 		joint->setMotion(PxD6Axis::eZ, LocationMotionType);
 		joint->setDrivePosition(PxTransform(PxVec3(0, 0, 0)));
 
 		// Angular motion for the handle.
 		joint->setMotion(PxD6Axis::eTWIST, RotationMotionType);
 		joint->setMotion(PxD6Axis::eSWING1, RotationMotionType);
 		joint->setMotion(PxD6Axis::eSWING2, RotationMotionType);
 
 		// Setup Linear drives for this physics handle if created.
 		if (handleData.softLinearConstraint)
 		{
 			joint->setDrive(PxD6Drive::eX, PxD6JointDrive(handleData.linearStiffness, handleData.linearDamping, handleData.maxLinearForce, PxD6JointDriveFlag::eACCELERATION));
 			joint->setDrive(PxD6Drive::eY, PxD6JointDrive(handleData.linearStiffness, handleData.linearDamping, handleData.maxLinearForce, PxD6JointDriveFlag::eACCELERATION));
 			joint->setDrive(PxD6Drive::eZ, PxD6JointDrive(handleData.linearStiffness, handleData.linearDamping, handleData.maxLinearForce, PxD6JointDriveFlag::eACCELERATION));
 		}
 		// Reset linear drives.
 		else
 		{
 			joint->setDrive(PxD6Drive::eX, PxD6JointDrive(0.0f, 0.0f, 0.0f, false));
 			joint->setDrive(PxD6Drive::eY, PxD6JointDrive(0.0f, 0.0f, 0.0f, false));
 			joint->setDrive(PxD6Drive::eZ, PxD6JointDrive(0.0f, 0.0f, 0.0f, false));
 		}
 
 		// Setup Angular drives for this physics handle if created.
 		if (rotationConstraint)
 		{
 			if (handleData.softAngularConstraint)
 			{
 				joint->setDrive(PxD6Drive::eSLERP, PxD6JointDrive(handleData.angularStiffness, handleData.angularDamping, handleData.maxAngularForce, PxD6JointDriveFlag::eACCELERATION));
 			}
 			// Reset angular drive.
 			else
 			{
 				joint->setDrive(PxD6Drive::eSLERP, PxD6JointDrive(0.0f, 0.0f, 0.0f, false));
 			}
 		}
 		
 #if WITH_EDITOR
 		// Log the new handle values.
 		if (debug) UE_LOG(LogVRHandle, Log, TEXT("\n \n %s \n"), *handleData.ToString());
 #endif // END Editor
 	}
 #endif // END PhysX
}

void UVRPhysicsHandleComponent::UpdateHandleTargetRotation(FRotator updatedRotation)
{
	targetTransform.SetRotation(updatedRotation.Quaternion());
}

void UVRPhysicsHandleComponent::ToggleRotationConstraint(bool on)
{
	// Update rotation constraint and re-initalise the joint/constraint.
	rotationConstraint = on;
 	ReinitJoint();
}

bool UVRPhysicsHandleComponent::IsRotationConstrained()
{
	return rotationConstraint;
}
