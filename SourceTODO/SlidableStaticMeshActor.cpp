// Fill out your copyright notice in the Description page of Project Settings.

#include "SlidableStaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

ASlidableStaticMeshActor::ASlidableStaticMeshActor()
{
	// Initialise the pivot as a skeletal mesh component.
	slidingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlidingMesh"));
	slidingMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	slidingMesh->SetCollisionProfileName(FName("ConstrainedComponent"));
	slidingMesh->ComponentTags.Add(FName("Grabbable"));
	slidingMesh->SetSimulatePhysics(false);
	slidingMesh->SetupAttachment(pivot);
}