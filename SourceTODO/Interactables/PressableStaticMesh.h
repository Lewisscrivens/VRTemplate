// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Globals.h"
#include "PressableStaticMesh.generated.h"

/* Define this actors log category. */
DECLARE_LOG_CATEGORY_EXTERN(LogPressable, Log, All);

/* Declare classes used. */
class USoundBase;
class UHapticFeedbackEffect_Base;
class USoundAttenuation;

/* Button delegates. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPressed, bool, on);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPressedOn, FString, pressableName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPressedRefference, UPressableStaticMesh*, pressable);

/* Different button modes. */
UENUM(BlueprintType)
enum class EButtonMode : uint8
{
	Default UMETA(DisplayName = "Default", ToolTip = "Button has to be held in position to be on and when released is set back to off."),
	Toggle  UMETA(DisplayName = "Toggle", ToolTip = "Same as default mode, except it toggles on and off on each press."),
	KeepPosition UMETA(DisplayName = "KeepPosition", ToolTip = "Keep the on position of the button after pressed, when pressed again return to off position and value."),
	SingleUse UMETA(DisplayName = "SingleUse", ToolTip = "Button will keep on position after pressed once."),
};

/* Different collision shape options, had to make custom enum as UE4's enums are not blueprint types. */
UENUM(BlueprintType)
enum class EButtonTraceCollision : uint8
{
	Sphere UMETA(DisplayName = "Sphere", ToolTip = "Trace for button position will be done using a sphere than encapsulates this button."),
	Box UMETA(DisplayName = "Box", ToolTip = "Trace for button position will be done using a box that encapsulates this button."),
};

/* To adjust the buttons update distance make the intractable sphere collider larger. NOTE: Created a blueprint class from this class in editor for use in blueprint based actors. */
/* Also this button updates every frame, to improve this only update this button when overlapping with a shape collider in the actor blueprint/class. */
UCLASS(ClassGroup = (Custom), Blueprintable, BlueprintType, PerObjectConfig, EditInlineNew)
class NINETOFIVE_API UPressableStaticMesh : public UStaticMeshComponent
{
	GENERATED_BODY()

public:

	// Sets default values for this component's properties
	UPressableStaticMesh();

	/* Current Button mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	EButtonMode buttonMode;

	/* Button trace shape. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	EButtonTraceCollision shapeTraceType;

	/* Haptic effect to play when the button state is changed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	UHapticFeedbackEffect_Base* hapticEffect;

	/* Sound to play when the button is switched on or pressed down. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	USoundBase* buttonPressed;

	/* Sound to play when the button returns up to the unpressed state. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	USoundBase* buttonReleased;

	/* Ignored actors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	TArray<AActor*> ignoredActors;

	/* Offset of the buttons trace overall if the origin is incorrect. Relative offset. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	FVector buttonOffset;

	/* Travel distance. Note: Travel axis is the relative z-axis for this component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button", meta = (ClampMin = "0.0", UIMin = "0"))
	float travelDistance;

	/* The distance pressed that should be classed as the button being on. NOTE: 1 would be the end position and 0 would be the start. 0.5 would be half way... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button", meta = (ClampMin = "0.0", UIMin = "0", ClampMax = "1.0", UIMax = "1"))
	float onPercentage;

	/* Time taken to interpolate button position back to the current state. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button", meta = (ClampMin = "0.0", UIMin = "0"))
	float interpolationSpeed;

	/* Artificial press speed for the pressButton function. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button", meta = (ClampMin = "0.0", UIMin = "0"))
	float pressSpeed;

	/* Use haptic feedback on this button. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	bool hapticFeedbackEnabled;

	/* Is the button currently on. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Button|CurrentValues")
	bool on;

	/* Locked in place. NOTE: Use reset function to reset the button to default. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Button|CurrentValues")
	bool locked;

	/* Prevents the button on events from working if they temporarily need to be disabled. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Button|CurrentValues")
	bool cannotPress;

	/* Boolean to control when to update button position. Controlled from owning actor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button|CurrentValues")
	bool buttonIsUpdating;

	/* Show any debug information. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Button")
	bool debug;

	//////////////////////////
	//	 Button Delegates   //
	//////////////////////////

	/* Button has been pressed down or released. */
	UPROPERTY(BlueprintAssignable)
	FPressed OnPressed;

	/* Button has been pressed down. */
	UPROPERTY(BlueprintAssignable)
	FPressedOn OnPressedOn;

	/* Button has been pressed or released. NOTE: Passes through the pressable that has been pressed/released. */
	UPROPERTY(BlueprintAssignable)
	FPressedRefference onPressedReff;

private:

	FHitResult buttonHit; /* Current frames hit result. */
	FTransform startTransform; /* Storage for the start and end of the button. */
	FTransform startRelativeTransform; /* Store start transform for the location and for transforming offsets. */
	FVector bounds;
	FVector lerpRelativeLocation; /* Current position to lerp back two when there is no interaction. */
	bool keepingPos;
	bool interpToPosition;
	bool alreadyToggled; /* Has the buttons on or off value been toggled... */
	bool resetInterpolationValues;
	FVector endPositionRel, startPositionRel, onPositionRel; /* Stored relative offsets for on and end positions. */
	FVector endTraceToUse; /* Relative position pointer to the current end trace. NOTE: Only used in the keep position mode and single use mode. */
	FVector buttonExtent;
	float sphereSize; /* Generated from the VR function library using bounds check. */
	float onDistance; /* Distance to travel until classed as on. */
	float oldInteractionSpeed; /* Old Interaction speed. */
	bool forcePressed, turnOn;

	/* Sound variables for playing audio for this pressable component. Updated/Set with the update audio function... */
	USoundAttenuation* soundAttenuation;
	float soundIntensity;
	float soundPitch;

protected:
	
	/* Level start. */
	virtual void BeginPlay() override;

	/* Remove the buttonOffset from a relative vector.
	 * @Param relativeVector, The relative vector to remove the button offset from. */
	FVector RemoveRelativeOffset(FVector relativeVector);

public:
	
	/* Frame. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/* Ran when hand is overlapping with interaction sphere. NOTE: needs adding manually. */
	void UpdateButtonPosition();

	/* Used to run functions for rumbling the hand, sound effects etc. for when the on value is changed. */
	void UpdateButton(bool isOn);

	/* Function to lerp smoothly between different relative locations. */
	void InterpButtonPosition(float DeltaTime);

	/* Press this button smoothly. */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void PressButton();

	/* Release this button smoothly. */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void ReleaseButton();

	/* Reset the button back to default. */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void ResetButton();

	/* Gets the correct parent transform for this component. */
	UFUNCTION(BlueprintCallable, Category = "Button")
	FTransform GetParentTransform();

	/* Update this pressable components audio settings for pitch and sound intensity. */
	UFUNCTION(BlueprintCallable, Category = "Button")
	void UpdateAudio(USoundBase* downSound, USoundBase* upSound, float intesity = 1.0f, float pitch = 1.0f, USoundAttenuation* attenuation = nullptr);
};
