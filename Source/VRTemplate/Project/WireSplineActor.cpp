// Fill out your copyright notice in the Description page of Project Settings.

#include "Project/WireSplineActor.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Components/CapsuleComponent.h"
#include "AssertionMacros.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Interactables/SlidableActor.h"

DEFINE_LOG_CATEGORY(LogWireSpline);

AWireSplineActor::AWireSplineActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	// Construct this actors sub-components.
	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	root->SetMobility(EComponentMobility::Movable);
	RootComponent = root;

	// The spline that generates the peelable tape mesh.
	wireSpline = CreateDefaultSubobject<USplineComponent>(TEXT("WireSpline"));
	wireSpline->SetupAttachment(root);

	// Default variables.
	wireMesh = nullptr;
	generatePhysics = false;
	wirePhysicsMaterial = UGlobals::GetPhysicalMaterial(PM_NoFriction);
	splineMeshLength = 5.0f;
	angularConstraintLimit = 45.0f;
	wireStiffness = 35.0f;
	splineMeshNo = 11;
	debug = false;

	// Generate the default spline.
	RegenerateSpline();
}

void AWireSplineActor::BeginPlay()
{
	Super::BeginPlay();

	// Generate the physics bodies used to update the locations at each spline point and update the rendering of each generatedWireMesh.
	if (generatePhysics)
	{
		GeneratePhysicsBodies();
	}
	// If physics is not enabled or generating the physics bodies failed disable tick functions and log message.
	else
	{
		SetActorTickEnabled(false);
		UE_LOG(LogWireSpline, Warning, TEXT("The wire spline actors tick functions have been disabled for %s."), *GetName());
	}
}

void AWireSplineActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update the spline locations from the generated physics bodies if physics is enabled on this wire spline actor.
	if (generatePhysics) UpdateSplineLocationsFromPhysicsBodies();
}
 
#if WITH_EDITOR
void AWireSplineActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//Get the name of the property that was changed.
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	// If the property changed was the refresh spline tickbox/boolean.
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(AWireSplineActor, regenerateSplineFully)))
	{
		RegenerateSpline();
		regenerateSplineFully = false;
	}
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(AWireSplineActor, regenerateSplineKeepShape)))
	{
		RegenerateSplineKeepShape();
		regenerateSplineKeepShape = false;
	}
	// Generate the spline meshes along the regenerated spline when it is set to something that isn't a null pointer.
	else if ((PropertyName == GET_MEMBER_NAME_CHECKED(AWireSplineActor, wireMesh)))
	{
		if (wireMesh) GenerateSplineMeshes();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AWireSplineActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// If there are generated spline meshes update the start and end locations and tangents of each.
	if (wireMesh) GenerateSplineMeshes();
}

void AWireSplineActor::RegenerateSpline()
{
	// Generate spline points from default values like splineMeshLength, splineMeshNo, etc.
	int numberOfPoints = splineMeshNo + 1;
	FOccluderVertexArray splinePoints;
	for (int i = 0; i < numberOfPoints; i++)
	{
		FVector newSplinePointLoc = root->GetComponentLocation() + (root->GetForwardVector() * (splineMeshLength * i));
		splinePoints.Add(newSplinePointLoc);
	}
	wireSpline->SetSplineWorldPoints(splinePoints);

	// Log generation.
	if (debug) UE_LOG(LogWireSpline, Log, TEXT("Regenerated Spline Mesh for wire spline actor %s."), *GetName());

	// Generate the spline meshes along the regenerated spline.
	GenerateSplineMeshes();
}

void AWireSplineActor::RegenerateSplineKeepShape()
{
	// Regenerate world locations from distances along the spline. 
	float currentSplineDistance = wireSpline->GetSplineLength();

	// Get number of spline points to generate. Round to int.
	int numberOfSplinePoints = (int)(currentSplineDistance / splineMeshLength);
	float roundedSplineMeshLength = currentSplineDistance / numberOfSplinePoints;

	// Generate new spline from current spline shape.
	FOccluderVertexArray splinePoints;
	TArray<FVector> newTangents;
	for (int x = 0; x < numberOfSplinePoints + 1; x++)
	{
		float distance = x * roundedSplineMeshLength;
		splinePoints.Add(wireSpline->GetLocationAtDistanceAlongSpline(distance, ESplineCoordinateSpace::World));
		newTangents.Add(wireSpline->GetDirectionAtDistanceAlongSpline(distance, ESplineCoordinateSpace::Local));
	}
	wireSpline->SetSplineWorldPoints(splinePoints);

	// Update the tangents at each of the regenerated points.
	for (int y = 0; y < numberOfSplinePoints + 1; y++)
	{
		wireSpline->SetTangentAtSplinePoint(y, newTangents[y], ESplineCoordinateSpace::Local);
	}

	// Log generation.
	if (debug) UE_LOG(LogWireSpline, Log, TEXT("Regenerated Spline Mesh for wire spline actor %s."), *GetName());

	// Generate the spline meshes along the regenerated spline.
	GenerateSplineMeshes();
}

void AWireSplineActor::GenerateSplineMeshes()
{
	// Do not construct and throw error to the log if any of the spline meshes are not yet initialized.
	if (!wireMesh)
	{
		splineSuccesfullyGenerated = false;
		UE_LOG(LogWireSpline, Warning, TEXT("GenerateSplineMeshes: Cannot generate spline meshes as wireMesh is null in the Wire spline actor %s."), *GetName());
		return;
	}

	// Remove any old spline meshes that are stored.
	for (USplineMeshComponent* mesh : generatedSplineMeshes)
	{
		if (mesh) mesh->DestroyComponent();
	}
	generatedSplineMeshes.Empty();

	// Construct the areas of the spline mesh in-between the start and end meshes.
	int numberOfSplinePoints = wireSpline->GetNumberOfSplinePoints();
	for (int i = 0; i < numberOfSplinePoints - 1; i++)
	{
		FName generatedWireMeshName = MakeUniqueObjectName(this, USplineMeshComponent::StaticClass(), FName("WireSplineMesh"));
		USplineMeshComponent* generatedWireMesh = NewObject<USplineMeshComponent>(this, generatedWireMeshName);
		generatedWireMesh->SetMobility(EComponentMobility::Movable);
		generatedWireMesh->AttachToComponent(wireSpline, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		generatedWireMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		generatedWireMesh->SetGenerateOverlapEvents(true);
		generatedWireMesh->RegisterComponent();
		generatedWireMesh->SetStaticMesh(wireMesh);
		generatedWireMesh->SetStartAndEnd(wireSpline->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local),
										  wireSpline->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::Local),
										  wireSpline->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::Local),
										  wireSpline->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::Local));
		generatedSplineMeshes.Add(generatedWireMesh);
	}

	// Success.
	splineSuccesfullyGenerated = true;
}

void AWireSplineActor::UpdateSplineMeshes()
{
	FVector startLocation, endLocation, startTangent, endTangent;
	float numberOfSplineMeshes = generatedSplineMeshes.Num();

	// Update spline mesh/meshes.
	for (int i = 0; i < numberOfSplineMeshes; i++)
	{
		if (generatedSplineMeshes[i])
		{
			wireSpline->GetLocationAndTangentAtSplinePoint(i, startLocation, startTangent, ESplineCoordinateSpace::Local);
			wireSpline->GetLocationAndTangentAtSplinePoint(i + 1, endLocation, endTangent, ESplineCoordinateSpace::Local);
			generatedSplineMeshes[i]->SetStartAndEnd(startLocation, startTangent, endLocation, endTangent);
			generatedSplineMeshes[i]->UpdateRenderStateAndCollision();
			generatedSplineMeshes[i]->SplineUpDir = wireSpline->GetUpVectorAtSplinePoint(i, ESplineCoordinateSpace::Local);
		}
	}
}

bool AWireSplineActor::GeneratePhysicsBodies()
{
	int numberOfSplineMeshes = generatedSplineMeshes.Num();

	// Update spline mesh/meshes.
	for (int i = 0; i < numberOfSplineMeshes; i++)
	{
		// Spawn body for current spline section.
		FName generatedShapeName = MakeUniqueObjectName(this, UCapsuleComponent::StaticClass(), FName("WirePhysicsShape"));
		UCapsuleComponent* generatedShape = NewObject<UCapsuleComponent>(this, generatedShapeName);
		generatedShape->SetMobility(EComponentMobility::Movable);
		generatedShape->AttachToComponent(wireSpline, FAttachmentTransformRules::KeepWorldTransform);
		generatedShape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		generatedShape->SetCollisionProfileName("ConstrainedComponent");
		generatedShape->SetCollisionObjectType(ECC_PhysicsBody);
		generatedShape->SetCollisionResponseToChannel(ECC_ConstrainedComp, ECR_Ignore);
		generatedShape->SetSimulatePhysics(true); // Simulate physics.
		generatedShape->BodyInstance.bLockRotation = false;
		generatedShape->BodyInstance.bLockXRotation = false;
		generatedShape->BodyInstance.bLockYRotation = false;
		generatedShape->BodyInstance.bLockZRotation = false;
		if (i == 0 || i == numberOfSplineMeshes - 1) generatedShape->SetMassOverrideInKg(NAME_None, 4.0f);
		generatedShape->GetBodyInstance()->InertiaTensorScale = FVector(0.8f);
		generatedShape->GetBodyInstance()->AngularDamping = 0.5f;
		generatedShape->GetBodyInstance()->LinearDamping = 0.8f;
		generatedShape->GetBodyInstance()->UpdateMassProperties();
		generatedShape->SetMassOverrideInKg(NAME_None, 0.5f);
		if (wirePhysicsMaterial) generatedShape->GetBodyInstance()->SetPhysMaterialOverride(wirePhysicsMaterial); // Set physics material if not nullptr.
		generatedShape->SetGenerateOverlapEvents(true);
		generatedShape->RegisterComponent();

		// Position physics body.
		generatedShape->SetCapsuleRadius(wireMesh->GetBounds().BoxExtent.Y);
		FVector bodySize = (wireSpline->GetWorldLocationAtSplinePoint(i + 1) - wireSpline->GetWorldLocationAtSplinePoint(i));
		FVector bodyLocation = wireSpline->GetWorldLocationAtSplinePoint(i) + (bodySize / 2);
		FRotator bodyRotation = bodySize.Rotation() + FRotator(90.0f, 0.0f, 0.0f);
		generatedShape->SetWorldLocationAndRotation(bodyLocation, bodyRotation, false, nullptr, ETeleportType::TeleportPhysics);
		generatedShape->SetCapsuleHalfHeight(bodySize.Size() / 2);

		// Add and store body in array.
		generatedPhysicsBodies.Add(generatedShape);

		// NOTE: Add any specific connections for certain actors HERE.
		// Setup start and end attachments. If they are setup.
		if (i == 0 && startConnection)
		{		
			// Spawn and attach the current body to the start connection component.
			generatedShape->SetSimulatePhysics(false);
			startAttachScene = NewObject<USceneComponent>(this, "startLocationScene");
			startAttachScene->AttachToComponent(root, FAttachmentTransformRules::KeepWorldTransform);
			startAttachScene->SetWorldLocationAndRotation(bodyLocation, bodyRotation);
			startAttachScene->RegisterComponent();

			// Calculate original offsets during connection so it can be updated.
			FTransform currSceneTransform = startAttachScene->GetComponentTransform();
			if (ASlidableActor* isSlidable = Cast<ASlidableActor>(startConnection)) startAttachScene->AttachToComponent(isSlidable->slidingMesh, FAttachmentTransformRules::KeepWorldTransform);
			else if (startConnection->GetRootComponent()) startAttachScene->AttachToComponent(startConnection->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			else UE_LOG(LogWireSpline, Warning, TEXT("Start connection could not be created as there was no root component in the actor to attach to for the wire spline %s."), *GetName());
		}
		else if (i == numberOfSplineMeshes - 1 && endConnection)
		{
			// Spawn and attach the current body to the end connection component.
			generatedShape->SetSimulatePhysics(false);
			endAttachScene = NewObject<USceneComponent>(this, "endLocationScene");
			endAttachScene->AttachToComponent(root, FAttachmentTransformRules::KeepWorldTransform);
			endAttachScene->SetWorldLocationAndRotation(bodyLocation, bodyRotation);
			endAttachScene->RegisterComponent();
			generatedShape->AttachToComponent(endAttachScene, FAttachmentTransformRules::KeepWorldTransform);

			// Calculate original offsets during connection so it can be updated.
			FTransform currSceneTransform = endAttachScene->GetComponentTransform();
			if (ASlidableActor* isSlidable = Cast<ASlidableActor>(endConnection)) endAttachScene->AttachToComponent(isSlidable->slidingMesh, FAttachmentTransformRules::KeepWorldTransform);
			else if (endConnection->GetRootComponent()) endAttachScene->AttachToComponent(endConnection->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
			else UE_LOG(LogWireSpline, Warning, TEXT("End connection could not be created as there was no root component in the actor to attach to for the wire spline %s."), *GetName());
		}

		// If not the first spawned body generate a constraint between the last body and the current body.
		if (i != 0)
		{
			// Spawn and setup constraint between generated bodies.
			FName generatedConstraintName = MakeUniqueObjectName(this, UPhysicsConstraintComponent::StaticClass(), FName("WireConstraint"));
			UPhysicsConstraintComponent* generatedConstraint = NewObject<UPhysicsConstraintComponent>(this, generatedConstraintName);
			generatedConstraint->AttachToComponent(wireSpline, FAttachmentTransformRules::KeepWorldTransform);
			generatedConstraint->RegisterComponent();
			generatedConstraint->SetWorldLocationAndRotation(bodyLocation - (bodySize / 2), bodyRotation - FRotator(90.0f, 0.0f, 0.0f));
			generatedConstraint->SetDisableCollision(true); // Disable collision between bodies.
			generatedConstraint->SetConstrainedComponents(generatedPhysicsBodies[i], NAME_None, generatedPhysicsBodies[i - 1], NAME_None);
			generatedConstraint->SetLinearXLimit(LCM_Locked, 0.0f);
			generatedConstraint->SetLinearYLimit(LCM_Locked, 0.0f);
			generatedConstraint->SetLinearZLimit(LCM_Locked, 0.0f);
			generatedConstraint->SetAngularSwing1Limit(ACM_Limited, angularConstraintLimit);
			generatedConstraint->SetAngularSwing2Limit(ACM_Limited, angularConstraintLimit);
			generatedConstraint->SetAngularTwistLimit(ACM_Limited, 0.0f);
			generatedConstraint->ConstraintInstance.ProfileInstance.TwistLimit.bSoftConstraint = false; // Don't allow constraint breakage.
			generatedConstraint->ConstraintInstance.ProfileInstance.ConeLimit.bSoftConstraint = false; // Don't allow constraint breakage.

			// Setup velocity drive to get it to act more realistic in terms of a wire.
			generatedConstraint->SetAngularDriveMode(EAngularDriveMode::TwistAndSwing);
			generatedConstraint->SetAngularVelocityDrive(true, false);
			generatedConstraint->SetAngularVelocityTarget(FVector(0.0f));

			// Use more stiffness towards the middle of the wire and less at the ends.
			float halfSplineMeshesNo = (float)(numberOfSplineMeshes - 1);
			float newStiffness = i <= halfSplineMeshesNo ? (1 - ((halfSplineMeshesNo - i) / halfSplineMeshesNo)) * wireStiffness : (1 - ((i - halfSplineMeshesNo) / halfSplineMeshesNo)) * wireStiffness;
			generatedConstraint->SetAngularDriveParams(0.0f, newStiffness, 0.0f);

			// Add the generated constraint to a storage array.
			generatedConstraints.Add(generatedConstraint);
		}	
	}

	// Logging.
	if (debug) UE_LOG(LogWireSpline, Warning, TEXT("Generated bodies and constraints."));

	// Success.
	return true;
}

void AWireSplineActor::UpdateSplineLocationsFromPhysicsBodies()
{
	// Update locations of the spline points from the generated physics bodies.
	if (generatedPhysicsBodies.Num() > 0)
	{
		// Update location of the attached ends of the wire.
		if (startAttachScene) generatedPhysicsBodies[0]->SetWorldLocationAndRotation(startAttachScene->GetComponentLocation(), startAttachScene->GetComponentQuat());
		if (endAttachScene) generatedPhysicsBodies[generatedPhysicsBodies.Num() - 1]->SetWorldLocationAndRotation(endAttachScene->GetComponentLocation(), endAttachScene->GetComponentQuat());

		// Generate spline points from default values like splineMeshLength, splineMeshNo, etc.
		FOccluderVertexArray splinePoints;
		for (int i = 0; i < generatedPhysicsBodies.Num(); i++)
		{
			FVector bodyLocation = generatedPhysicsBodies[i]->GetComponentLocation();
			float capsuleHalfHeight = generatedPhysicsBodies[i]->GetUnscaledCapsuleHalfHeight();
			FVector newPoint = bodyLocation + (generatedPhysicsBodies[i]->GetUpVector() * capsuleHalfHeight);
			splinePoints.Add(newPoint);
		}
		wireSpline->SetSplineWorldPoints(splinePoints);

		// Update the rendering of the spline mesh components.
		UpdateSplineMeshes();
	}
}