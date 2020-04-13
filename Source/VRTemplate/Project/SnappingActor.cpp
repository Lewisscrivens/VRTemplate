// Fill out your copyright notice in the Description page of Project Settings.

#include "Project/SnappingActor.h"
#include "Interactables/GrabbableActor.h"
#include "Player/HandsInterface.h"
#include "Player/VRHand.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/Material.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogSnappingActor);

ASnappingActor::ASnappingActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PrePhysics;

	// Setup the box component as the root component of this actor.
	snapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SnappingBox"));
	snapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	snapBox->SetCollisionResponseToChannel(ECC_Interactable, ECR_Overlap);
	snapBox->SetCollisionResponseToChannel(ECC_ConstrainedComp, ECR_Overlap);
	snapBox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);

	// Bind overlap event to this box component.
	snapBox->OnComponentBeginOverlap.AddDynamic(this, &ASnappingActor::OverlapBegin);
	snapBox->OnComponentEndOverlap.AddDynamic(this, &ASnappingActor::OverlapEnd);
	RootComponent = snapBox;

	// Setup the preview component.
	previewComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewCompoenent"));
	previewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Variable defaults.
	snapMode = ESnappingMode::Instant;
	interpMode = EInterpMode::Disabled;
	full = false;
	previewMaterial = UGlobals::GetMaterial(M_Translucent);
	rotateAroundYaw = false;
	snatch = false;
	rotationSpeed = 1.0f;
	timeToInterp = 0.2f;
	locationOffset = FVector::ZeroVector;
	rotationOffset = FRotator::ZeroRotator;
	snappingTag = "NULL";
	componentSnapped = false;

	// Setup physics handle for physics snap mode.
	physicsHandleSettings = FPhysicsHandleData();
	physicsHandleSettings.handleDataEnabled = true;
	physicsHandleSettings.interpolate = timeToInterp != 0;
	physicsHandleSettings.linearStiffness = 5000.0f;
	physicsHandleSettings.angularStiffness = 5000.0f;
}

void ASnappingActor::BeginPlay()
{
	Super::BeginPlay();

	// If there is an actor to snap ensure it is a snappable object and snap it into position. Delay this to next frame to give all other actors a chance to bind to the on release function.
	FTimerDelegate nextFrame;
	nextFrame.BindLambda([&]{ if (actorToSnap) { ForceSnap(actorToSnap); } });
	GetWorldTimerManager().SetTimerForNextTick(nextFrame);
	
}

void ASnappingActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Interpolate preview mesh if interpolating is enabled.
	if (interpMode != EInterpMode::Disabled) Interpolate(DeltaTime);
}

#if WITH_EDITOR
bool ASnappingActor::CanEditChange(const UProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	// Can only change physics handle settings if in the physics mode(s).
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(ASnappingActor, physicsHandleSettings))
	{
		return snapMode == ESnappingMode::PhysicsOnRelease;
	}

	return ParentVal;
}
#endif

void ASnappingActor::OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// Get the enum for type of component to use/effect.
	if (AGrabbableActor* grabbableActor = Cast<AGrabbableActor>(OtherActor))
	{
		if (overlappingGrabbable || (snappingTag != "NULL" && !grabbableActor->ActorHasTag(snappingTag))) return;
		if (!(grabbableActor->handRefInfo.handRef || (overlappingGrabbable && grabbableActor == overlappingGrabbable))) return;
		if (grabbableActor->hasSnappingActor) grabbableActor->hasSnappingActor->ResetPreviewMesh();
		grabbableActor->hasSnappingActor = this;

		// Full now.
		full = true;

		// If in returning interpolation mode attach back to the snapping box.
		if (previewComponent && interpMode == EInterpMode::Returning) previewComponent->AttachToComponent(snapBox, FAttachmentTransformRules::KeepWorldTransform);

		// Re-init the preview mesh and if preview mesh was successfully created interpolate to the center of the snap box + offset. Also Bind to release function while overlapping.
		overlappingGrabbable = (AGrabbableActor*)OtherActor;
		if (!overlappingGrabbable->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbable->OnMeshReleased.AddDynamic(this, &ASnappingActor::OnGrabbableRealeased);
		SetupPreviewMesh(overlappingGrabbable->grabbableMesh);

		// Depending on the snap mode hide the grabbed mesh in the hand.
		switch (snapMode)
		{
		case ESnappingMode::Instant:
		{
			interpMode = EInterpMode::Disabled;
			FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
			FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
			overlappingGrabbable->grabbableMesh->SetVisibility(false, true);
			previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
			if (!overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);
		}
		break;
		case ESnappingMode::Interpolate:
		{
			StartInterpolation(EInterpMode::Interpolate);
			overlappingGrabbable->grabbableMesh->SetVisibility(false, true);
		}
		break;
		case ESnappingMode::PhysicsOnRelease:
		case ESnappingMode::InstantOnRelease:
		case ESnappingMode::InterpolateOnRelease:
		{
			// Interpolate to the center of the snapBox + the location and rotation offset.
			FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
			FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
			if (!overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);
			previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
		}
		break;
		}

		// Perform snatch if need be after delegates are setup.
		if (snatch) overlappingGrabbable->handRefInfo.handRef->ReleaseGrabbedActor();
	}
}

void ASnappingActor::OverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// If the current overlapping grabbable is valid and is equal to the actor that ended the overlap with this component lose the reference to said grabbable.
	if (overlappingGrabbable)
	{
		if (overlappingGrabbable->handRefInfo.handRef &&  overlappingGrabbable == OtherActor)
		{
			// Attach to hand to stop movement affecting interpolation.
			if (previewComponent) previewComponent->AttachToComponent(overlappingGrabbable->grabbableMesh, FAttachmentTransformRules::KeepWorldTransform);

			// Remove if its this.
			if (overlappingGrabbable->hasSnappingActor == this) overlappingGrabbable->hasSnappingActor = nullptr;

			// Unbind to release function when end overlap.
			if (overlappingGrabbable->OnMeshReleased.Contains(this, "OnGrabbableRealeased")) overlappingGrabbable->OnMeshReleased.RemoveDynamic(this, &ASnappingActor::OnGrabbableRealeased);
			if (overlappingGrabbable->OnMeshGrabbed.Contains(this, "OnGrabbablePressed")) overlappingGrabbable->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);
			if (snapMode == ESnappingMode::InstantOnRelease || snapMode == ESnappingMode::InterpolateOnRelease || snapMode == ESnappingMode::PhysicsOnRelease)
			{
				ResetPreviewMesh();	
				overlappingGrabbable = nullptr;
				componentSnapped = false;
			}
			// Interpolate the preview mesh back to the hand.
			else StartInterpolation(EInterpMode::Returning);
			full = false;	
			componentSnapped = false;
		}
	}
}

void ASnappingActor::OnGrabbablePressed(AVRHand* hand, UPrimitiveComponent* compPressed)
{
	// Show preview mesh if grabbed and not showing.
	bool snap = false;
	if (overlappingGrabbable)
	{
		snap = true;
		SetupPreviewMesh(overlappingGrabbable->grabbableMesh);
	}

	// Destroy physics handle if in snap mode.
	if (snapMode == ESnappingMode::PhysicsOnRelease) DestroyPhysicsHandle();
	else if (snapMode == ESnappingMode::Instant) overlappingGrabbable->grabbableMesh->SetVisibility(false, true);

	// Of component is found set it up correctly.
	if (snap)
	{
		previewComponent->AttachToComponent(snapBox, FAttachmentTransformRules::KeepWorldTransform);
		FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
		FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
		previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
	}

	// Broadcast to snapped disconnection delegate.
	OnSnapDisconnect.Broadcast(compPressed);

	// Detach it.
	compPressed->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
}

void ASnappingActor::OnGrabbableRealeased(AVRHand* hand, UPrimitiveComponent* compReleased)
{
	// If the snapping actor doesn't equal this ignore it and force release.
	// NOTE: Fix for overlapping multiple snapping actors at once.
	if (overlappingGrabbable && overlappingGrabbable->hasSnappingActor != nullptr)
	{
		if (overlappingGrabbable->hasSnappingActor != this)
		{
			ForceRelease();
			return;
		}
	}

	// Reset preview mesh as the component has been released.
	ResetPreviewMesh();

	// Get snapping location/rotation.
	FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
	FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;

	// Perform releasing snapping functionality on the current released component.
	if (compReleased)
	{
		// Depending on snap mode, snap the overlapping grabbable to the snapBox location + offset.
		switch (snapMode)
		{
		case ESnappingMode::PhysicsOnRelease:
		{
			interpMode = EInterpMode::Disabled;
			previewComponent->SetWorldLocationAndRotation(targetLocation, targetRotation);
			CreateAndAttatchPhysicsHandle(compReleased);
		}
		break;
		case ESnappingMode::Interpolate:
		case ESnappingMode::Instant:
			compReleased->SetVisibility(true, true);
		case ESnappingMode::InstantOnRelease:
		{
			// Disable any physics on grabbable skel.
			compReleased->SetSimulatePhysics(false);
			FVector targetLocation = snapBox->GetComponentLocation() + locationOffset;
			FRotator targetRotation = snapBox->GetComponentRotation() + rotationOffset;
			compReleased->SetWorldLocationAndRotation(targetLocation, targetRotation);
			compReleased->AttachToComponent(snapBox, FAttachmentTransformRules::KeepWorldTransform);
		}
		break;
		case ESnappingMode::InterpolateOnRelease:
		{
			// Disable any physics on grabbable skel.
			compReleased->SetSimulatePhysics(false);
			StartInterpolation(EInterpMode::InterpolateOverlapping);
		}
		break;
		}
	}

	// Broadcast to snapped connection delegate.
	OnSnapConnect.Broadcast(compReleased, targetLocation, targetRotation);
}

void ASnappingActor::StartInterpolation(EInterpMode mode)
{
	// Start interpolation.
	interpMode = mode;
	interpolationStartTime = GetWorld()->GetTimeSeconds();

	// Return and disable interp if there no grabbable mesh.
	if (!overlappingGrabbable)
	{
		interpMode = EInterpMode::Disabled;
		return;
	}

	// Change the comp to interpolate if its the overlapping comp to be interpolated.
	componentToInterpolate = previewComponent;
	if (interpMode == EInterpMode::InterpolateOverlapping)
	{
		// Get the correct mesh to interpolate.
		componentToInterpolate = overlappingGrabbable->grabbableMesh;
	}

	// Get start location to interp from.
	interpStartTransform = componentToInterpolate->GetComponentTransform();
}

void ASnappingActor::Interpolate(float deltaTime)
{
	if (previewComponent)
	{	
		// Get the target location and rotation to interpolate to from the current interpolation mode.
		switch (interpMode)
		{
		case EInterpMode::InterpolateOverlapping:
		case EInterpMode::Interpolate:
		{
			// Interpolate to the center of the snapBox + the location and rotation offset.
			lerpLocation = snapBox->GetComponentLocation() + locationOffset;
			lerpRotation = snapBox->GetComponentRotation() + rotationOffset;
		}
		break;
		case EInterpMode::Returning:
		{
			if (overlappingGrabbable)
			{
				// If grabbable actor interpolate to current grabbed offset from the hand.
				lerpLocation = overlappingGrabbable->grabbableMesh->GetComponentLocation();
				lerpRotation = overlappingGrabbable->grabbableMesh->GetComponentRotation();
			}
			else return;
		}
		break;
		}

		// Lerp to target position/rotation/scale.
		float lerpProgress = GetWorld()->GetTimeSeconds() - interpolationStartTime;
		float alpha = FMath::Clamp(lerpProgress / timeToInterp, 0.0f, 1.0f);
		FVector lerpingLocation = FMath::Lerp(interpStartTransform.GetLocation(), lerpLocation, alpha);
		FRotator lerpingRotation = FMath::Lerp(interpStartTransform.GetRotation().Rotator(), lerpRotation, alpha);
		componentToInterpolate->SetWorldLocationAndRotation(lerpingLocation, lerpingRotation);

		// When the component has lerped into snapping position or the hand.
		if (alpha >= 1)
		{
			if (interpMode == EInterpMode::Returning)
			{
				componentToInterpolate->AttachToComponent(snapBox, FAttachmentTransformRules::KeepWorldTransform);

				// Once finished returning reset the preview mesh and nullify the overlapping grabbable after setting it back to visible in-case hidden.
				overlappingGrabbable->grabbableMesh->SetVisibility(true, true);
				overlappingGrabbable = nullptr;
				ResetPreviewMesh();
			}

			// Disable the interp mode.
			interpMode = EInterpMode::Disabled;
		}
	}
	else
	{
		// If the preview component is null, log warning message.
		UE_LOG(LogSnappingActor, Warning, TEXT("Snapping Component %s, has not previewComponent to interpolate."), *GetName());
		interpMode = EInterpMode::Disabled;
	}
}

bool ASnappingActor::SetupPreviewMesh(UPrimitiveComponent* comp)
{
	// Reset the preview mesh if there is one.
	ResetPreviewMesh();

	// Cannot spawn preview actor when this component is full.
	if (comp)
	{
		// Setup the previewMesh.
		UStaticMeshComponent* newStaticMesh = NewObject<UStaticMeshComponent>(this, TEXT("PreviewMesh"));
		newStaticMesh->SetMobility(EComponentMobility::Movable);
		newStaticMesh->RegisterComponent();

		UStaticMeshComponent* staticMeshComp = Cast<UStaticMeshComponent>(comp);
		newStaticMesh->SetStaticMesh(staticMeshComp->GetStaticMesh());
		newStaticMesh->SetWorldScale3D(staticMeshComp->GetComponentScale());
		// If in instant or interpolate snap mode don't change material to preview material, copy it.
		if (snapMode == ESnappingMode::Instant || snapMode == ESnappingMode::Interpolate)
		{
			for (int m = 0; m < newStaticMesh->GetNumMaterials(); m++)
			{
				newStaticMesh->SetMaterial(m, staticMeshComp->GetMaterial(m));
			}
		}
		// Set all materials as the preview material.
		else
		{
			for (int m = 0; m < newStaticMesh->GetNumMaterials(); m++)
			{
				newStaticMesh->SetMaterial(m, previewMaterial);
			}
		}
		previewComponent = newStaticMesh;

		// Check preview component was spawned.
		if (!previewComponent)
		{
			UE_LOG(LogSnappingActor, Log, TEXT("SetupPreviewMesh has failed to prodice previewComponent in the snapping component %s."), *GetName());
			return false;
		}

		// Move component to duplicate location, disable any collision and set the preview material.
 		previewComponent->SetWorldTransform(comp->GetComponentTransform());
		previewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Loop through and add children from comp to the preview mesh.
		TArray<USceneComponent*> childrenComponents;
		childrenComponents.Add(comp);
		comp->GetChildrenComponents(true, childrenComponents);
		for (int i = 0; i < childrenComponents.Num(); i++)
		{
			// If the child component is a type of mesh duplicate it and attach it to the preview with the preview material.
			UMeshComponent* typeOfMesh = Cast<UMeshComponent>(childrenComponents[i]);
			if (typeOfMesh)
			{
				FName uniqueCompName = MakeUniqueObjectName(previewComponent, typeOfMesh->GetClass(), FName("preview"));
				UMeshComponent* coppiedComp = DuplicateObject(typeOfMesh, previewComponent, uniqueCompName);
				coppiedComp->RegisterComponent();
				if (previewComponent) coppiedComp->AttachToComponent(previewComponent, FAttachmentTransformRules::KeepWorldTransform);
				else return false;

				// Remove all collision and change material to preview material.
				coppiedComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				
				// If in instant or interpolate snap mode don't change material to preview material, copy it.
				if (snapMode != ESnappingMode::Instant && snapMode != ESnappingMode::Interpolate)
				{
					for (int m = 0; m < coppiedComp->GetNumMaterials(); m++)
					{
						coppiedComp->SetMaterial(m, previewMaterial);
					}
				}
			}
		}
		return true;
	}
	else
	{
		UE_LOG(LogSnappingActor, Log, TEXT("Function SpawnGrabbableActor: The snapping component, %s is full or comp is null."), *GetName());
		return false;
	}
}

void ASnappingActor::ResetPreviewMesh()
{
	if (previewComponent)
	{
		// Destroy any child components.
		TArray<USceneComponent*> childrenComponents;
		previewComponent->GetChildrenComponents(true, childrenComponents);
		for (USceneComponent* child : childrenComponents)
		{
			child->DestroyComponent();
		}

		// Destroy main component.
		previewComponent->DestroyComponent();
	}

	// Reset other variables.
	interpMode = EInterpMode::Disabled;
	interpolationStartTime = 0.0f;
}

void ASnappingActor::CreateAndAttatchPhysicsHandle(UPrimitiveComponent* compToAttatch)
{
	UVRPhysicsHandleComponent* newHandle = NewObject<UVRPhysicsHandleComponent>(this, UVRPhysicsHandleComponent::StaticClass());
	newHandle->RegisterComponent();
	if (newHandle)
	{
		AGrabbableActor* grabbable = Cast<AGrabbableActor>(compToAttatch->GetOwner());
		if (grabbable)
		{
			physicsHandleSettings.softLinearConstraint = true;
			grabbable->grabbableMesh->SetSimulatePhysics(true);
			newHandle->CreateJointAndFollowLocationWithRotation(grabbable->grabbableMesh, snapBox, NAME_None, grabbable->grabbableMesh->GetComponentLocation(),
				grabbable->grabbableMesh->GetComponentRotation() + rotationOffset, physicsHandleSettings);
			newHandle->grabOffset = false;
			currentHandle = newHandle;
		}
		else newHandle->DestroyComponent();
	}
	else UE_LOG(LogSnappingActor, Warning, TEXT("Snapping actor %s, could not create a physics handle..."));
}

void ASnappingActor::DestroyPhysicsHandle()
{
	if (currentHandle)
	{
		currentHandle->DestroyJoint();
		currentHandle->DestroyComponent();
		currentHandle = nullptr;
	}
}

void ASnappingActor::ForceSnap(AActor* snappingActor)
{
	// If the actor is a grabbable actor snap into position.
	if (AGrabbableActor* isGrabbableActor = Cast<AGrabbableActor>(snappingActor))
	{
		// Snap actor into default snap Modes.
		bool hasCorrectTag = snappingTag == "NULL" ? true : isGrabbableActor->ActorHasTag(snappingTag);
		if (overlappingGrabbable || !hasCorrectTag) return;
		overlappingGrabbable = isGrabbableActor;
		OnGrabbableRealeased(nullptr, isGrabbableActor->grabbableMesh);
		isGrabbableActor->OnMeshGrabbed.AddDynamic(this, &ASnappingActor::OnGrabbablePressed);
		isGrabbableActor->OnMeshReleased.AddDynamic(this, &ASnappingActor::OnGrabbableRealeased);
	}
	// Otherwise cancel function.
	else return;

	// Now full.
	full = true;
}

void ASnappingActor::ForceRelease()
{
	// Remove delegates and destroy references.
	if (overlappingGrabbable)
	{
		overlappingGrabbable->OnMeshGrabbed.RemoveDynamic(this, &ASnappingActor::OnGrabbablePressed);
		overlappingGrabbable->OnMeshReleased.RemoveDynamic(this, &ASnappingActor::OnGrabbableRealeased);
		OnSnapDisconnect.Broadcast(overlappingGrabbable->grabbableMesh);
		overlappingGrabbable = nullptr;
	}
	// Otherwise there is nothing to release so cancel.
	else return;

	// Reset the preview mesh.
	ResetPreviewMesh();

	// Empty.
	full = false;
}