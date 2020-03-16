// Fill out your copyright notice in the Description page of Project Settings.

#include "SlidableSkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "DrawDebugHelpers.h"

ASlidableSkeletalMeshActor::ASlidableSkeletalMeshActor()
{
	// Initialise the pivot as a skeletal mesh component.
	slidingMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SlidingMesh"));
	slidingMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	slidingMesh->SetCollisionProfileName(FName("ConstrainedComponent"));
	slidingMesh->ComponentTags.Add(FName("Grabbable"));
	slidingMesh->SetSimulatePhysics(false);
	slidingMesh->bMultiBodyOverlap = true;
	slidingMesh->SetupAttachment(pivot);
}
