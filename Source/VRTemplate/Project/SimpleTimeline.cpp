// Fill out your copyright notice in the Description page of Project Settings.

#include "Project/SimpleTimeline.h"
#include "GameFramework/Actor.h"
#include "Curves/RichCurve.h"
#include "Curves/CurveFloat.h"

DEFINE_LOG_CATEGORY(LogSimpleTimeline);

USimpleTimeline::USimpleTimeline()
{
	//...
}

USimpleTimeline* USimpleTimeline::CreateSimpleTimeline(UCurveFloat * timelineCurve, FName timelineName, UObject * propertySetObject, FName callbackFunction, FName finishFunction, AActor * owningActor, FName timelineVariableName, bool looping, ETimelineLengthMode timelineLength, TEnumAsByte<ETimelineDirection::Type> timelineDirection)
{
	// Only create timeline if curve has been given.
	if (timelineCurve)
	{
		// Create simple timeline to return
		FString name = timelineName.ToString().Append("_SimpleTimeline");
		USimpleTimeline* timeline = NewObject<USimpleTimeline>(propertySetObject, timelineName);

		// Create a timeline component and set it inside the SimpleTimeline
		timeline->timelineComponent = NewObject<UTimelineComponent>(propertySetObject, FName(*name));

		// Time line callbacks
		FOnTimelineFloat onTimelineCallback;

		// Get the created time line
		UTimelineComponent* timelineComponent = timeline->timelineComponent;
		timelineComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;

		// Add the owning actor to the time line
		owningActor->BlueprintCreatedComponents.Add(timelineComponent);

		// Set Time line Components
		timelineComponent->SetPropertySetObject(propertySetObject);
		timelineComponent->SetLooping(looping);
		timelineComponent->SetTimelineLengthMode(timelineLength);
		timelineComponent->SetPlaybackPosition(0.0f, false, false);
		onTimelineCallback.BindUFunction(propertySetObject, callbackFunction);

		// if finish function name = NAME_None don't create a finish function callback
		if (finishFunction != NAME_None)
		{
			FOnTimelineEventStatic onTimelingFinishCallback;
			onTimelingFinishCallback.BindUFunction(propertySetObject, finishFunction);
			timelineComponent->SetTimelineFinishedFunc(onTimelingFinishCallback);
		}

		// Add float curve to the time line
		timelineComponent->AddInterpFloat(timelineCurve, onTimelineCallback, timelineVariableName);
		timelineComponent->RegisterComponent();

		// Return the created timeline.
		return timeline;
	}

	// Loge error and return null.
	UE_LOG(LogSimpleTimeline, Error, TEXT("Could not create SimpleTimeline!"));
	return nullptr;
}

USimpleTimeline * USimpleTimeline::CreateLinearSimpleTimeline(FName timelineName, UObject * propertySetObject, FName callbackFunction, FName finishFunction, AActor * owningActor, FName timelineVariableName, bool looping, ETimelineLengthMode timelineLength, TEnumAsByte<ETimelineDirection::Type> timelineDirection)
{
	// Make a linear curve
	FRichCurve* curve = new FRichCurve();
	curve->SetKeyInterpMode(curve->AddKey(0.0f, 0.0f), ERichCurveInterpMode::RCIM_Linear);
	curve->SetKeyInterpMode(curve->AddKey(1.0f, 1.0f), ERichCurveInterpMode::RCIM_Linear);
	UCurveFloat* timelineCurve = NewObject<UCurveFloat>();
	timelineCurve->FloatCurve = *curve;

	// Cleanup
	delete curve;

	// Create and return the simple timeline.
	return CreateSimpleTimeline(timelineCurve, timelineName, propertySetObject, callbackFunction, finishFunction, owningActor, timelineVariableName, looping, timelineLength, timelineDirection);
}

bool USimpleTimeline::IsPlaying() const
{
	return timelineComponent->IsPlaying();
}

bool USimpleTimeline::IsReversing() const
{
	return timelineComponent->IsReversing();
}

void USimpleTimeline::Stop()
{
	timelineComponent->Stop();
	timelineComponent->SetPlaybackPosition(0.0f, false, false);
}

void USimpleTimeline::Pause()
{
	timelineComponent->Stop();
}

void USimpleTimeline::PlayFromStart()
{
	timelineComponent->PlayFromStart();
}

void USimpleTimeline::PlayFromCurrentLocation()
{
	timelineComponent->Play();
}

void USimpleTimeline::Reverse()
{
	timelineComponent->Reverse();
}

void USimpleTimeline::SetPosition(int position, bool fireEvents, bool fireUpdateEvent)
{
	timelineComponent->SetPlaybackPosition(position, fireEvents, fireUpdateEvent);
}

void USimpleTimeline::SetPlayRate(float playrate)
{
	timelineComponent->SetPlayRate(playrate);
}