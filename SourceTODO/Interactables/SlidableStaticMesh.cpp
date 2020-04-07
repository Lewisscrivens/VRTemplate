// Fill out your copyright notice in the Description page of Project Settings.

#include "SlidableStaticMesh.h"
#include "Player/VRHand.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogSlidableMesh);

USlidableStaticMesh::USlidableStaticMesh()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Initialise this component.
	SetCollisionProfileName("Grabbable");
	ComponentTags.Add("Grabbable");

	// Initialise variables for this class.
	handRef = nullptr;
	currentAxis = ESlideAxis::X;
	slideLimit = 10.0f;
	startLocation = 0.0f;
	centerLimit = false;
	interpolationSpeed = 5.0f;
	relativeInterpolationPos = 0.0f;
	interpolating = false;
	releaseOnLimit = false;

	// Initialise interface variables.
	interactableSettings.releaseDistance = 30.0f;
	interactableSettings.handMinRumbleDistance = 5.0f;
}

void USlidableStaticMesh::BeginPlay()
{
	Super::BeginPlay();

	// Ensure physics is disabled for functionality.
	if (IsSimulatingPhysics())
	{
		SetSimulatePhysics(false);
		UE_LOG(LogSlidableMesh, Log, TEXT("Disabled physics on slidable static mesh for functionlity to work. %s"), *GetName());
	}

	// Check there is a slide limit. If not pointless setting up the class.
	if (slideLimit == 0) return;

	// Save the original transform of this mesh to its parent.
	originalRelativeTransform.SetLocation(GetAttachParent()->GetComponentTransform().InverseTransformPositionNoScale(this->GetComponentLocation()));
	originalRelativeTransform.SetRotation(GetAttachParent()->GetComponentTransform().InverseTransformRotation(this->GetComponentQuat()));

	// Update limits.
	UpdateConstraintBounds();
}

void USlidableStaticMesh::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Handle interpolation of this slidable component.
	if (interpolating)
	{
		FVector currentRelativeLocation = RelativeLocation;
		FVector newLocation = RelativeLocation;
		switch (currentAxis)
		{
		case ESlideAxis::X:
			newLocation.X = relativeInterpolationPos;
		break;
		case ESlideAxis::Y:
			newLocation.Y = relativeInterpolationPos;
		break;
		case ESlideAxis::Z:
			newLocation.Z = relativeInterpolationPos;
		break;
		}

		// Update Location and check if the interpolation is finished.
		FVector interpingLocation = FMath::VInterpTo(RelativeLocation, newLocation, DeltaTime, interpolationSpeed);
		SetRelativeLocation(interpingLocation);
		if (currentRelativeLocation == interpingLocation) interpolating = false;
 	}
}

#if WITH_EDITOR
void USlidableStaticMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get the name of the property that was changed.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If the property was the start location update it.
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USlidableStaticMesh, startLocation))
	{
		// Update Limits.
		UpdateConstraintBounds();

		// If the start location is within limits set the relative location.
		if (startLocation <= maxRelativeLoc && startLocation >= minRelativeLoc)
		{
			switch (currentAxis)
			{
			case ESlideAxis::X:
				SetRelativeLocation(FVector(startLocation, RelativeLocation.Y, RelativeLocation.Z));
				break;
			case ESlideAxis::Y:
				SetRelativeLocation(FVector(RelativeLocation.X, startLocation, RelativeLocation.Z));
				break;
			case ESlideAxis::Z:
				SetRelativeLocation(FVector(RelativeLocation.X, RelativeLocation.Y, startLocation));
				break;
			}
		}
		// Otherwise clamp the value back within its correct limits.
		else startLocation = FMath::Clamp(startLocation, minRelativeLoc, maxRelativeLoc);
	}
	// Check if it was the relativeInterpolationPos and ensure it stays clamped within its limits.
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(USlidableStaticMesh, relativeInterpolationPos))
	{
		// Update limits.
		UpdateConstraintBounds();

		// If the relativeInterpolationPos is not within its limits clamp it.
		if (relativeInterpolationPos > maxRelativeLoc || relativeInterpolationPos < minRelativeLoc)
		{
			relativeInterpolationPos = FMath::Clamp(relativeInterpolationPos, minRelativeLoc, maxRelativeLoc);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void USlidableStaticMesh::UpdateConstraintBounds()
{
	// Updates min and max so the startLocation can be clamped correctly.
	if (centerLimit)
	{
		maxRelativeLoc = FMath::Abs(slideLimit) / 2;
		minRelativeLoc = maxRelativeLoc * -1;
	}
	else
	{
		if (slideLimit < 0)
		{
			maxRelativeLoc = 0.0f;
			minRelativeLoc = slideLimit;
		}
		else
		{
			maxRelativeLoc = slideLimit;
			minRelativeLoc = 0.0f;
		}
	}
}

void USlidableStaticMesh::UpdateSlidable()
{
	// Get current hands relative offset and clamp it.
	FTransform targetTransform = handRef->grabCollider->GetComponentTransform();
	FVector slidingOffset = targetTransform.TransformPositionNoScale(originalGrabLocation);
	FVector slidingClampedOffset = ClampPosition(slidingOffset);

	// Update the slidables relative location.
	SetRelativeLocation(slidingClampedOffset);
	switch (currentAxis)
	{
	case ESlideAxis::X:
		currentPosition = RelativeLocation.X;
	break;
	case ESlideAxis::Y:
		currentPosition = RelativeLocation.Y;
	break;
	case ESlideAxis::Z:
		currentPosition = RelativeLocation.Z;
	break;
	}

	// Update the hand grab distance for handling when to release the intractable etc.
	interactableSettings.handDistance = FMath::Abs((targetTransform.TransformPositionNoScale(originalGrabLocation) - GetComponentLocation()).Size());
}

FVector USlidableStaticMesh::ClampPosition(FVector position)
{
	// Clamp current location of the slidingMesh to the closest location within the constrained limits.
	FVector clampedPosition = originalRelativeTransform.GetLocation();
	FVector relativePosition = GetAttachParent()->GetComponentTransform().InverseTransformPositionNoScale(position);
	switch (currentAxis)
	{
	case ESlideAxis::X:
		clampedPosition.X = FMath::Clamp(relativePosition.X, minRelativeLoc, maxRelativeLoc);
		currentPosition = relativePosition.X;
	break;
	case ESlideAxis::Y:
		clampedPosition.Y = FMath::Clamp(relativePosition.Y, minRelativeLoc, maxRelativeLoc);
		currentPosition = relativePosition.Y;
	break;
	case ESlideAxis::Z:		
		clampedPosition.Z = FMath::Clamp(relativePosition.Z, minRelativeLoc, maxRelativeLoc);
		currentPosition = relativePosition.Z;
	break;
	}
	return clampedPosition;
}

void USlidableStaticMesh::SetSlidablePosition(float positionAlongAxis, bool interpolate, float interpSpeed)
{
	// Check return if the position to set is out of bounds.
	bool validPosition = positionAlongAxis < minRelativeLoc && positionAlongAxis > maxRelativeLoc;
	CHECK_RETURN(LogSlidableMesh, validPosition, "Slidable position is out of bounds so cannot set position in slidable class %s.", *GetName());

	// Release if grabbed and trying to set position.
	if (handRef) handRef->ReleaseGrabbedActor();

	// If interpolate do so otherwise just set position instantly.
	if (interpolate)
	{
		interpolationSpeed = interpSpeed;
		relativeInterpolationPos = positionAlongAxis;
		interpolating = true;
	}
	else
	{
		FVector newRelativeLocation = originalRelativeTransform.GetLocation();
		switch (currentAxis)
		{
		case ESlideAxis::X:
			newRelativeLocation.X = positionAlongAxis;
		break;
		case ESlideAxis::Y:
			newRelativeLocation.Y = positionAlongAxis;
		break;
		case ESlideAxis::Z:
			newRelativeLocation.Z = positionAlongAxis;
		break;
		}
		SetRelativeLocation(newRelativeLocation);
	}
}

void USlidableStaticMesh::GrabPressed_Implementation(AVRHand* hand)
{
	// Call delegate for being grabbed.
	OnMeshGrabbed.Broadcast(hand);

	// Save new hand.
	handRef = hand;

	// Save original grab location.
	originalGrabLocation = handRef->grabCollider->GetComponentTransform().InverseTransformPositionNoScale(GetComponentLocation());
}

void USlidableStaticMesh::GrabReleased_Implementation(AVRHand* hand)
{
	// Call delegate for being grabbed.
	OnMeshReleased.Broadcast(hand);
	
	// Delete variables.
	interpolating = false;
	handRef = nullptr;
}

void USlidableStaticMesh::Dragging_Implementation(float deltaTime)
{
	// If grabbed update slidable.
	if (handRef)
	{
		UpdateSlidable();

		// Check to see if the component needs releasing on limit reached.
		if (releaseOnLimit)
		{
			float currentRelativePos = 0.0f;
			switch (currentAxis)
			{
			case ESlideAxis::X:
				currentRelativePos = RelativeLocation.X;
			break;
			case ESlideAxis::Y:
				currentRelativePos = RelativeLocation.Y;
			break;
			case ESlideAxis::Z:
				currentRelativePos = RelativeLocation.Z;
			break;
			}
			if (currentRelativePos >= maxRelativeLoc)
			{
				OnMeshReleasedOnLimit.Broadcast(handRef);
				handRef->ReleaseGrabbedActor();
			}
		}
	}
}

void USlidableStaticMesh::Overlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::Overlapping_Implementation(hand);
}

void USlidableStaticMesh::EndOverlapping_Implementation(AVRHand* hand)
{
	IHandsInterface::EndOverlapping_Implementation(hand);
}

FHandInterfaceSettings USlidableStaticMesh::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void USlidableStaticMesh::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}
