// Fill out your copyright notice in the Description page of Project Settings.

#include "Globals.h"
#include "ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/ObjectLibrary.h"
#include "AssetRegistryModule.h"
#include "ModuleManager.h"
#include "Sound/SoundBase.h"

UMaterialInterface* UGlobals::GetMaterial(FString materialDestination)
{
 	static ConstructorHelpers::FObjectFinder<UMaterialInterface> mat(*materialDestination);
	// If found return the material otherwise return the default material.
	if (mat.Object) return mat.Object;
	else return UMaterial::GetDefaultMaterial(MD_Surface);
}

UPhysicalMaterial* UGlobals::GetPhysicalMaterial(FString materialDestination)
{
	static ConstructorHelpers::FObjectFinder<UPhysicalMaterial> mat(*materialDestination);
	if (mat.Object) return mat.Object;
	else return nullptr;
}

UHapticFeedbackEffect_Base* UGlobals::GetFeedback(FString feedbackDestination)
{
	static ConstructorHelpers::FObjectFinder<UHapticFeedbackEffect_Base> feedback(*feedbackDestination);
	return feedback.Object;
}

USoundBase* UGlobals::GetSound(FString soundDestination)
{
	static ConstructorHelpers::FObjectFinder<USoundBase> sound(*soundDestination);
	return sound.Object;
}
