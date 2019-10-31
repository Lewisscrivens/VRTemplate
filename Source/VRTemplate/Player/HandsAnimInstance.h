// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Project/Globals.h"
#include "HandsAnimInstance.generated.h"

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogHandAnimInst, Log, All);

/* This is set as the parent of the animation blueprint so I can communicate these variables from C++ into that animation blueprint. */
UCLASS(transient, Blueprintable, hideCategories = AnimInstance, BlueprintType)
class VRTEMPLATE_API UHandsAnimInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

public:
	
	/** Current hand close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float handClosingAmount;

	/** Current hand lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
		float handLerpingAmount;

	/** Current hands pinky close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float pinkyClosingAmount;

	/** Current hands ring close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float ringClosingAmount;

	/** Current hands middle close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float middleClosingAmount;

	/** Current hands thumb close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float thumbClosingAmount;

	/** Current hands finger close amount coming from the hands class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float fingerClosingAmount;

	/** Current hand pinky lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float pinkyLerpAmount;

	/** Current hand ring lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float ringLerpAmount;

	/** Current hand middle lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float middleLerpAmount;

	/** Current hand thumb lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float thumbLerpAmount;

	/** Current hand lerping close amount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float fingerLerpingAmount;

	/** Speed to lerp in between animation states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	float handLerpSpeed;

	/** Speed to lerp in between animation states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Hands)
	bool pointing;
};
