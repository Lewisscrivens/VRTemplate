// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Interactable/SlidableActor.h"
#include "Player/HandsInterface.h"
#include "Globals.h"
#include "SlidableSkeletalMeshActor.generated.h"

/* NOTE: Slidable actor where slidable pivot is a skeletal mesh component. */
UCLASS(Blueprintable)
class NINETOFIVE_API ASlidableSkeletalMeshActor : public ASlidableActor
{
	GENERATED_BODY()

public:

	/* Constructor. */
	ASlidableSkeletalMeshActor();
};
