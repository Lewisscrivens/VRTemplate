// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetInteractionComponent.h"
#include "Project/Globals.h"
#include "VRWidgetInteractionComponent.generated.h"

/* Custom widget interaction component used to add some extra needed vr functionality and also to initalise with GSlateFastWidgetPath being set to 0/false. Prevents VR use of widget interactor in builds from being optimized away via macros. */
UCLASS()
class VRTEMPLATE_API UVRWidgetInteractionComponent : public UWidgetInteractionComponent
{
	GENERATED_BODY()

public:

	/* The last widget path, the path to the last widget to have been under the widget performTrace result. */
	FWidgetPath lastPath;

	/* Press key / run key pressed event within a user interface. */
	virtual void PressPointerKey(FKey Key) override;

	/* Perform collision trace with UI to determine current hovered widget. */
	virtual FWidgetTraceResult PerformTrace() const override;

	/** Returns the path to the widget that is currently beneath the pointer */
	FWidgetPath DetermineWidgetUnderPointer();
};
