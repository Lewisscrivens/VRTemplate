// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Object.h"
#include "Globals.generated.h"

//=======================
// Macros
//=======================

// Checks condition, if true, will log STRING and return.
#define CHECK_RETURN(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return;};
// Checks condition, if true, will log STRING and return.
#define CHECK_RETURN_FALSE(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return false;};
// Checks condition, if true, will log STRING and return.
#define CHECK_RETURN_WARNING(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Warning, TEXT(logString), ##__VA_ARGS__); return;};
// Checks condition, if true, will log STRING and return.
#define CHECK_OBJECT_RETURN_WARNING(CategoryName, condition, object, logString, ...) if (condition) {UE_LOG(CategoryName, Warning, TEXT(logString), ##__VA_ARGS__); return object;};
// Checks condition, if true, will log STRING and return object.
#define CHECK_OBJECT_RETURN(CategoryName, condition, object, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return object;};
// Checks condition, if true, will log STRING and return nullptr.
#define CHECK_RETURN_NULL(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); return nullptr;};
// Checks condition, if true, will log STRING and will exit the current loop and run continue command
#define CHECK_CONTINUE(CategoryName, condition, logString, ...) if (condition) {UE_LOG(CategoryName, Error, TEXT(logString), ##__VA_ARGS__); continue;};
// Checks condition, if true, will log STRING and return.
#define RETURN(condition) if (condition) {return;};
// Will print string to the log file if the condition is true
#define CHECK_LOG(CategoryName, verbosity, condition, logString, ...) if (condition) {UE_LOG(CategoryName, verbosity, TEXT(logString), ##__VA_ARGS__);};
// Print log message, shortening macro.
#define PRINT(logString, ...) {UE_LOG(LogTemp, Warning, TEXT(logString), ##__VA_ARGS__);};
// Logging macro for float variables.
#define PRINTF(floatVariable) { UE_LOG(LogTemp, Warning, TEXT("%f"), floatVariable);};
// Logging macro for boolean variables.
#define PRINTB(condition) { UE_LOG(LogTemp, Warning, TEXT("%s"), condition ? TEXT("True") : TEXT("False")); };
// Return a boolean as a String.
#define SBOOL(condition) condition ? TEXT("true") : TEXT("false")
// Return null or valid as text for condition.
#define SNULL(condition) condition ? TEXT("Valid") : TEXT("Nullptr")

//==============================
// Collisions Object types
//===============================

#define ECC_Hand ECC_GameTraceChannel1
#define ECC_Walkable ECC_GameTraceChannel2
#define ECC_StaticCollisionOnly ECC_GameTraceChannel3
#define ECC_Interactable ECC_GameTraceChannel4
#define ECC_ConstrainedComp ECC_GameTraceChannel5
#define ECC_BlockMovement ECC_GameTraceChannel6
#define ECC_Teleport ECC_GameTraceChannel7
#define ECC_UI ECC_GameTraceChannel9;

//===============================
// Asset destinations.
//===============================

#define M_Translucent FString("/Game/Assets/Materials/Mesh/Misc/MI_Transparency")
#define PM_NoFriction FString("/Game/Assets/Materials/Physics/PM_NoFriction")

//===============================
// Macro to remove debug code from builds.
//===============================

#define DEVELOPMENT 1

//===============================
// Misc
//===============================

#define MAX_WIDGET_POOL_SIZE 30

// Enable temporal AA anti ghosting feature.
#define AA_DYNAMIC_ANTIGHOST 1

//===============================
// Global Class
//===============================

class UHapticFeedbackEffect_Base;
class UPhysicalMaterial;
class USoundBase;
class UMaterialInterface;

/* Global class for defining macro's and any other definitions used across the code for this project... */
UCLASS()
class VRTEMPLATE_API UGlobals : public UObject
{
	GENERATED_BODY()

public:

	// CONSTRUCTOR ONLY // 

	/* Get a material interface from a destination. NOTE: Use material definitions in globals.h
	 * @Param materialDestination, String of project file destination.*/
	static UMaterialInterface* GetMaterial(FString materialDestination);

	/* Get a physics material reference from the file destination in current project.
	 * @Param materialDestination, String of project file destination.*/
	static UPhysicalMaterial* GetPhysicalMaterial(FString materialDestination);

	/* Get a haptic feedback effect from a destination. NOTE: Use feedback definitions in VRHand.h
	 * @Param feedbackDestination, String of project file destination.*/
	static UHapticFeedbackEffect_Base* GetFeedback(FString feedbackDestination);

	/* Get a sound from a destination.
	 * @Param soundDestination, String of project file destination.*/
	static USoundBase* GetSound(FString soundDestination);
};