// Fill out your copyright notice in the Description page of Project Settings.

#include "Interactables/PeelableSplineActor.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Player/VRHand.h"
#include "SceneManagement.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogPeelable);

APeelableSplineActor::APeelableSplineActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.TickGroup = ETickingGroup::TG_PrePhysics;

	// Construct this actors sub-components.
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	root->SetMobility(EComponentMobility::Movable);
	RootComponent = root;

	// The spline that generates the peelable tape mesh.
	peelableSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PeelableSpline"));
	peelableSpline->SetupAttachment(root);

	// The grab area of where this peelable can be grabbed from.
	grabArea = CreateDefaultSubobject<USphereComponent>(TEXT("GrabbableArea"));
	grabArea->SetupAttachment(peelableSpline);
	grabArea->SetSphereRadius(5.0f);
	grabArea->SetCollisionProfileName("OverlapAll");
	grabArea->SetGenerateOverlapEvents(true);
	grabArea->ComponentTags.Add(TEXT("Grabbable"));
	grabArea->OnComponentBeginOverlap.AddDynamic(this, &APeelableSplineActor::OverlapDetected);
	grabArea->OnComponentEndOverlap.AddDynamic(this, &APeelableSplineActor::OverlapEnded);
	
	// Setup the spline used to curve current grabbed areas of the peelable spline towards the hand.
	grabCurveSpline = CreateDefaultSubobject<USplineComponent>(TEXT("GrabbingCurvedSpline"));
	grabCurveSpline->SetupAttachment(root);

	// Initialise the defaults.
	pointEndsDown = true;
	regenerateSpline = false;
	splineMeshDistance = 5.0f;
	tapeSections = 11;
	numOfOverlaps = 0;
	debug = false;
	beenPeeled = false;
	detachedSplineEnd = 0;

	// Initialise default interface settings.
	interactableSettings.releaseDistance = 25.0f;
	interactableSettings.handMinRumbleDistance = 10.0f;
	// Handled in this class.
	interactableSettings.hightlightInteractable = false;

	// Setup the spline mesh from defaults.
	RegenerateSplineFromDefaults(ERegenType::Default);
}

void APeelableSplineActor::BeginPlay()
{
	Super::BeginPlay();
	
	// Save original relative offset of spline.
	originalSplineTransform = peelableSpline->GetRelativeTransform();
}

#if WITH_EDITOR
void APeelableSplineActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get the name of the property that was changed.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If the property changed was the refresh spline tick box/boolean.
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(APeelableSplineActor, regenerateSpline)))
	{
		// Must have been changed to true so refresh spline from defaults and reset to false.
		RegenerateSplineFromDefaults(ERegenType::Default);
		regenerateSpline = false;
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void APeelableSplineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void APeelableSplineActor::RegenerateSplinePoint(int indexToReset)
{
	// Number of components to reset.
	FVector newSplinePointLoc = root->GetComponentLocation() + (root->GetForwardVector() * (splineMeshDistance * indexToReset));
	peelableSpline->SetWorldLocationAtSplinePoint(indexToReset, newSplinePointLoc);

	// Debug.
	if (debug) UE_LOG(LogPeelable, Log, TEXT("Regenerated Spline point %f."), (float)indexToReset);
}

void APeelableSplineActor::RegenerateSplineFromDefaults(ERegenType regenType)
{
	// Regenerate the spline.
	switch (regenType)
	{
	case ERegenType::Default:
	{
		// Generate number of spline point segments all spaced splineMeshDistance / 2 from each other.
		int numberOfPoints = tapeSections + 1;
		FOccluderVertexArray splinePoints;
		for (int i = 0; i < numberOfPoints; i++)
		{
			FVector newSplinePointLoc = root->GetComponentLocation() + (root->GetForwardVector() * (splineMeshDistance * i));
			splinePoints.Add(newSplinePointLoc);
		}
		peelableSpline->SetSplineWorldPoints(splinePoints);

		if (pointEndsDown)
		{
			// Round the tangents at the ends of the spline for each point. Do everything locally to avoid errors on rotation etc.
			peelableSpline->SetLocationAtSplinePoint(0, peelableSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local) - FVector(0.65f, 0.0f, splineMeshDistance), ESplineCoordinateSpace::Local);
			peelableSpline->SetLocationAtSplinePoint(numberOfPoints - 1, peelableSpline->GetLocationAtSplinePoint(numberOfPoints - 2, ESplineCoordinateSpace::Local) - FVector(-0.65f, 0.0f, splineMeshDistance), ESplineCoordinateSpace::Local);
			peelableSpline->SetTangentAtSplinePoint(1, FVector(splineMeshDistance / 2, 0.0f, 0.0f), ESplineCoordinateSpace::Local);
			peelableSpline->SetTangentAtSplinePoint(numberOfPoints - 2, FVector(splineMeshDistance / 2, 0.0f, 0.0f), ESplineCoordinateSpace::Local);
		}

		// Position grab area at the start spline section.
		FVector splinePointLoc = peelableSpline->GetWorldLocationAtSplinePoint(0);
		FVector relativeOffset = peelableSpline->GetWorldLocationAtSplinePoint(1) - splinePointLoc;
		grabArea->SetWorldLocation(splinePointLoc + relativeOffset / 2);
		grabArea->SetSphereRadius(splineMeshDistance);
		
		// Reset peeled index.
		detachedSplineEnd = 0;

		if (debug) UE_LOG(LogPeelable, Log, TEXT("Regenerated Spline Mesh."));
	}
	break;
	case ERegenType::Grabbed:
	{
		// Position grabbing curve spline points to help position current grabbed spline areas.
		int nextSplinePointStuckDown = detachedSplineEnd + 1;
		FOccluderVertexArray splinePoints;
		FVector splinePointStuckDown = peelableSpline->GetWorldLocationAtSplinePoint(nextSplinePointStuckDown);

		// Clamp hand location from worldGrabOffset to avoid going too far in the relative y axis to the spline.
		FTransform splineTransform = peelableSpline->GetComponentTransform();
		FVector relativeWorldGrabOffset = splineTransform.InverseTransformPositionNoScale(worldGrabOffset);
		float clampY = (splineMeshDistance / 3) * nextSplinePointStuckDown;// For each section clamp a third of there size.
		relativeWorldGrabOffset.Y = FMath::Clamp(relativeWorldGrabOffset.Y, -clampY, clampY);
		float clampX = detachedSplineEnd * splineMeshDistance;
		relativeWorldGrabOffset.X = FMath::Clamp(relativeWorldGrabOffset.X, -clampX, clampX);
		FVector clampedWorldGrabOffset = splineTransform.TransformPositionNoScale(relativeWorldGrabOffset);

		// Get relative offset from the next spline point to be peeled and detached and the spline point grabbed by the hand.
		FVector relativeToHandOffset = clampedWorldGrabOffset - splinePointStuckDown;
		FVector unstrechedHandOffset = splinePointStuckDown + (relativeToHandOffset.GetSafeNormal() * nextSplinePointStuckDown * splineMeshDistance);
		
		// Create the curve spline between the hand and the current point stuck down.
		splinePoints.Add(splinePointStuckDown);
		splinePoints.Add(unstrechedHandOffset);
		grabCurveSpline->SetSplineWorldPoints(splinePoints);

		// Curve the spline smoothly in-between the current stuck down point and the grabbed area.
		grabCurveSpline->SetTangentAtSplinePoint(0, peelableSpline->GetTangentAtSplinePoint(detachedSplineEnd, ESplineCoordinateSpace::Local) + FVector(splineMeshDistance * -4.0f, 0.0f, 0.0f), ESplineCoordinateSpace::Local);// Stuck down area.

		// Update the hand grab distance in the interface settings.
		interactableSettings.handDistance = controllerTransform.InverseTransformPositionNoScale(unstrechedHandOffset).Size();

		// Adjust locations of detached areas of the spline which have already been peeled.
		peelableSpline->SetTangentAtSplinePoint(0, -grabCurveSpline->GetDirectionAtSplinePoint(1, ESplineCoordinateSpace::Local), ESplineCoordinateSpace::Local);
		peelableSpline->SetWorldLocationAtSplinePoint(0, grabCurveSpline->GetWorldLocationAtSplinePoint(1));
		for (int i = 1; i < nextSplinePointStuckDown; i++)
		{
			// Get the distance from the point stuck down to the current spline point being positioned.
			float currentPointDistance = splineMeshDistance * ((detachedSplineEnd + 1) - i);
			FVector curvedWorldLocation, curvedLocalTangent;

			// get the location and tangent of each point relative to the curve created from the current stuck down point and the world grab offset.
			curvedWorldLocation = grabCurveSpline->GetLocationAtDistanceAlongSpline(currentPointDistance, ESplineCoordinateSpace::World);
			curvedLocalTangent = grabCurveSpline->GetDirectionAtDistanceAlongSpline(currentPointDistance, ESplineCoordinateSpace::Local);

			// Update the location and tangent of current spline point.
			peelableSpline->SetTangentAtSplinePoint(i, -curvedLocalTangent * (splineMeshDistance / 2), ESplineCoordinateSpace::Local);
			peelableSpline->SetWorldLocationAtSplinePoint(i, curvedWorldLocation);
		}
		peelableSpline->SetTangentAtSplinePoint(nextSplinePointStuckDown, FVector(2.0f, 0.0f, 0.0f), ESplineCoordinateSpace::Local);

		// Check if any spline points need to be detached.
		if (relativeWorldGrabOffset.Z > (splineMeshDistance / 2) * nextSplinePointStuckDown)
		{
			// Call peeled up delegate.
			OnPeeled.Broadcast(nextSplinePointStuckDown, true);

			// Increase peeled up area.
			detachedSplineEnd++;	

			// If the next spline point stuck down is the last one run the finished peeling delegate and release the interactable.
			if (detachedSplineEnd + 1 == tapeSections)
			{
				handRef->ReleaseGrabbedActor();
				OnPeelableComplete.Broadcast();
				return;
			}
		}
		// Or check if it needs to be stuck back down.
		else if (relativeWorldGrabOffset.Z < ((splineMeshDistance / 3) * nextSplinePointStuckDown) - (splineMeshDistance / 2))
		{
			// Check if it needs to be released based on current location.
			if (detachedSplineEnd == 0)
			{
				handRef->ReleaseGrabbedActor();
				return;
			}

			// Call stuck down delegate.
			OnPeeled.Broadcast(nextSplinePointStuckDown - 1, false);

			// Decrease peeled up area.
			detachedSplineEnd--;

			// Refresh areas that have been stuck back down.
			RegenerateSplinePoint(nextSplinePointStuckDown - 1);
 		}
	}
	break;
	}

	// Update the regenerated spline and spline meshes.
	RefreshSplineMeshesFromSpline();
}

void APeelableSplineActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Do not construct and throw error to the log if any of the spline meshes are not yet initialized.
	CHECK_RETURN(LogPeelable, (!splineStartMesh || !splineEndMesh || !splineMiddleMesh),
		"APeelableSplineActor::OnConstruction: Cannot construct spline meshes for the actor %s. Ensure all splineMeshes are set and not null.", *GetName());

	// Binding are not needed if the spline is being refreshed while grabbed.
	bool bindOverlaps = !handRef;

	// Remove any old spline meshes that are stored.
	for (USplineMeshComponent* mesh : splineMeshes)
	{
		if (mesh)
		{
			mesh->OnComponentBeginOverlap.RemoveDynamic(this, &APeelableSplineActor::OverlapDetected);
			mesh->OnComponentEndOverlap.RemoveDynamic(this, &APeelableSplineActor::OverlapEnded);
			mesh->DestroyComponent();
		}
	}
	splineMeshes.Empty();
	numOfOverlaps = 0;

	// Construct the start of the tape with the first spline end mesh.
	USplineMeshComponent* splineStartMeshReff = NewObject<USplineMeshComponent>(this, "SplineStart");
	splineStartMeshReff->SetMobility(EComponentMobility::Movable);
	splineStartMeshReff->AttachToComponent(peelableSpline, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	splineStartMeshReff->SetCollisionProfileName("BlockAll");
	splineStartMeshReff->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	splineStartMeshReff->SetCollisionObjectType(ECC_Destructible);
	splineStartMeshReff->SetGenerateOverlapEvents(true);
	splineStartMeshReff->RegisterComponent();
	splineStartMeshReff->SetStaticMesh(splineStartMesh); 
	if (splineMeshMaterial) splineStartMeshReff->SetMaterial(0, splineMeshMaterial);
	splineStartMeshReff->SetStartAndEnd(peelableSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local),
		peelableSpline->GetTangentAtSplinePoint(0, ESplineCoordinateSpace::Local),
		peelableSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local),
		peelableSpline->GetTangentAtSplinePoint(1, ESplineCoordinateSpace::Local));
	splineMeshes.Add(splineStartMeshReff);
	splineStartMeshReff->OnComponentBeginOverlap.AddDynamic(this, &APeelableSplineActor::OverlapDetected);
	splineStartMeshReff->OnComponentEndOverlap.AddDynamic(this, &APeelableSplineActor::OverlapEnded);

	// Construct the areas of the spline mesh in-between the start and end meshes.
	int numberOfSplinePoints = peelableSpline->GetNumberOfSplinePoints();
	for (int i = 1; i < numberOfSplinePoints - 2; i++)
	{
		FName splineMeshName = MakeUniqueObjectName(this, USplineMeshComponent::StaticClass(), FName("SplineMeshMiddle"));
		USplineMeshComponent* splineMiddleMeshRef = NewObject<USplineMeshComponent>(this, splineMeshName);
		splineMiddleMeshRef->SetMobility(EComponentMobility::Movable);
		splineMiddleMeshRef->AttachToComponent(peelableSpline, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		splineMiddleMeshRef->SetCollisionProfileName("BlockAll");
		splineMiddleMeshRef->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		splineMiddleMeshRef->SetCollisionObjectType(ECC_Destructible);
		splineMiddleMeshRef->SetGenerateOverlapEvents(true);
		splineMiddleMeshRef->RegisterComponent();
		splineMiddleMeshRef->SetStaticMesh(splineMiddleMesh);
		if (splineMeshMaterial) splineMiddleMeshRef->SetMaterial(0, splineMeshMaterial);
		splineMiddleMeshRef->SetStartAndEnd(peelableSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local),
			peelableSpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local),
			peelableSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local),
			peelableSpline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local));
		splineMeshes.Add(splineMiddleMeshRef);
		splineMiddleMeshRef->OnComponentBeginOverlap.AddDynamic(this, &APeelableSplineActor::OverlapDetected);
		splineMiddleMeshRef->OnComponentEndOverlap.AddDynamic(this, &APeelableSplineActor::OverlapEnded);
	}

	// Construct the end of the tape with the ending spline mesh.
	USplineMeshComponent* splineEndMeshReff = NewObject<USplineMeshComponent>(this, "SplineEnd");
	splineEndMeshReff->SetMobility(EComponentMobility::Movable);
	splineEndMeshReff->AttachToComponent(peelableSpline, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	splineEndMeshReff->SetCollisionProfileName("BlockAll");
	splineEndMeshReff->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	splineEndMeshReff->SetCollisionObjectType(ECC_Destructible);
	splineEndMeshReff->SetGenerateOverlapEvents(true);
	splineEndMeshReff->RegisterComponent();
	splineEndMeshReff->SetStaticMesh(splineEndMesh);
	if (splineMeshMaterial) splineEndMeshReff->SetMaterial(0, splineMeshMaterial);
	splineEndMeshReff->SetStartAndEnd(peelableSpline->GetLocationAtSplinePoint(numberOfSplinePoints - 2, ESplineCoordinateSpace::Local),
		peelableSpline->GetTangentAtSplinePoint(numberOfSplinePoints - 2, ESplineCoordinateSpace::Local),
		peelableSpline->GetLocationAtSplinePoint(numberOfSplinePoints - 1, ESplineCoordinateSpace::Local),
		peelableSpline->GetTangentAtSplinePoint(numberOfSplinePoints - 1, ESplineCoordinateSpace::Local));
	splineMeshes.Add(splineEndMeshReff);
	splineEndMeshReff->OnComponentBeginOverlap.AddDynamic(this, &APeelableSplineActor::OverlapDetected);
	splineEndMeshReff->OnComponentEndOverlap.AddDynamic(this, &APeelableSplineActor::OverlapEnded);
}

void APeelableSplineActor::RefreshSplineMeshesFromSpline(int index)
{
	FVector startLocation, endLocation, startTangent, endTangent;

	// Update spline mesh/meshes.
	if (index < 0)
	{
		for (int i = 0; i < splineMeshes.Num(); i++)
		{
			if (splineMeshes[i])
			{
				
				peelableSpline->GetLocationAndTangentAtSplinePoint(i, startLocation, startTangent, ESplineCoordinateSpace::Local);
				peelableSpline->GetLocationAndTangentAtSplinePoint(i + 1, endLocation, endTangent, ESplineCoordinateSpace::Local);
				splineMeshes[i]->SetStartAndEnd(startLocation, startTangent, endLocation, endTangent);
				splineMeshes[i]->UpdateRenderStateAndCollision();
			}
		}
	}
	// If index is valid reset specific part of spline.
	else if (index < splineMeshes.Num() && splineMeshes[index])
	{
		peelableSpline->GetLocationAndTangentAtSplinePoint(index, startLocation, startTangent, ESplineCoordinateSpace::Local);
		peelableSpline->GetLocationAndTangentAtSplinePoint(index + 1, endLocation, endTangent, ESplineCoordinateSpace::Local);
		splineMeshes[index]->SetStartAndEnd(startLocation, startTangent, endLocation, endTangent);
		splineMeshes[index]->UpdateRenderStateAndCollision();
	}
}

TArray<USplineMeshComponent*>& APeelableSplineActor::GetGeneratedSplineMeshes()
{
	return splineMeshes;
}

void APeelableSplineActor::OverlapDetected(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (AVRHand* isHand = Cast<AVRHand>(OtherActor))
	{
		// If not rendering custom depth enable it.
		if (splineMeshes.Num() > 0 && !splineMeshes[0]->bRenderCustomDepth)
		{
			splineMeshes[0]->SetRenderCustomDepth(true);
			splineMeshes[0]->SetCustomDepthStencilValue(2);
			if (debug) UE_LOG(LogPeelable, Warning, TEXT("Overlap detected. Highlighting."), *GetName());
		}
		// Keep track of how many spline meshes have been overlapped by the hand.
		numOfOverlaps++;
	}
}

void APeelableSplineActor::OverlapEnded(class UPrimitiveComponent* OverlappedComp, class AActor* OtherActor, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AVRHand* isHand = Cast<AVRHand>(OtherActor))
	{
		// If the last overlap has been ended and 
		if (numOfOverlaps <= 1 && splineMeshes.Num() > 0 && splineMeshes[0]->bRenderCustomDepth)
		{
			splineMeshes[0]->SetRenderCustomDepth(false);
			splineMeshes[0]->SetCustomDepthStencilValue(0);		
			if (debug) UE_LOG(LogPeelable, Warning, TEXT("Overlap ended. Un-Highlighting."), *GetName());
		}
		numOfOverlaps--;
	}
}

void APeelableSplineActor::GrabPressed_Implementation(AVRHand* hand)
{
	if (splineMeshes.Num() > 0)
	{
		handRef = hand;

		// If still highlighting end highlight.
		if (splineMeshes[0]->bRenderCustomDepth)
		{
			splineMeshes[0]->SetRenderCustomDepth(false);
			splineMeshes[0]->SetCustomDepthStencilValue(0);
		}	

		// Peel up the start spline mesh to the same tangent and height as the second spline mesh.
		peelableSpline->SetLocationAtSplinePoint(0, peelableSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local) - FVector(splineMeshDistance, 0.0f, 0.0f), ESplineCoordinateSpace::Local);
		peelableSpline->SetTangentAtSplinePoint(1, FVector(splineMeshDistance, 0.0f, 0.0f), ESplineCoordinateSpace::Local);
		RefreshSplineMeshesFromSpline(0);
		RefreshSplineMeshesFromSpline(1);

		// Save the original grab offsets.
		FTransform controllerTransform = handRef->controller->GetComponentTransform();
		originalGrabOffset = controllerTransform.InverseTransformPositionNoScale(splineMeshes[0]->GetComponentLocation());

		// Call peelable grabbed delegate.
		OnPeelableGrabbed.Broadcast();

		if (debug) UE_LOG(LogPeelable, Log, TEXT("Peelable actor was grabbed."));
	}
	else
	{
		hand->ReleaseGrabbedActor();
		if (debug) UE_LOG(LogPeelable, Log, TEXT("Peelable actor was released as there was no spline meshes to grab."));
	}
}

void APeelableSplineActor::GrabReleased_Implementation(AVRHand* hand)
{
	// Call grab released delegate.
	OnPeelableReleased.Broadcast();

	// Nullify saved values.
	handRef = nullptr;

	// If released without being fully peeled reset this peelable back to its default state.
	RegenerateSplineFromDefaults(ERegenType::Default);
	if (debug) UE_LOG(LogPeelable, Log, TEXT("Peelable actor was released."));
}

void APeelableSplineActor::Dragging_Implementation(float deltaTime)
{
	// If grabbed by the hand, update the grab distance and the peelable spline actor/component.
	if (handRef)
	{
		// Update current intended world location/rotation offset.
		controllerTransform = handRef->controller->GetComponentTransform();
		worldGrabOffset = controllerTransform.TransformPositionNoScale(originalGrabOffset);

		// If current hand grabbing this interactable is moving over a specific velocity play haptic effect. Also play sound.
		if (handRef->handVelocity.Size() > 10.0f && !handRef->IsPlayingFeedback())
		{
			float intensity = (handRef->handVelocity.Size() - 10.0f) / 50.0f;
			if (hapticCurve) handRef->PlayFeedback(hapticCurve, intensity, false);
			if (peelSound) UGameplayStatics::PlaySoundAtLocation(GetWorld(), peelSound, peelableSpline->GetWorldLocationAtSplinePoint(detachedSplineEnd), intensity, intensity);
		}
		
		// Re-generate the spline between the hand, current peel state spline point and the end of the spline.
		// NOTE: Grab offset from hand updated in grabbed regeneration function.
		RegenerateSplineFromDefaults(ERegenType::Grabbed);
	}
}

FHandInterfaceSettings APeelableSplineActor::GetInterfaceSettings_Implementation()
{
	return interactableSettings;
}

void APeelableSplineActor::SetInterfaceSettings_Implementation(FHandInterfaceSettings newInterfaceSettings)
{
	interactableSettings = newInterfaceSettings;
}

