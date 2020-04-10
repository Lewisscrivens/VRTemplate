#pragma once
#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "Components/ActorComponent.h"
#include "Delegates/DelegateSignatureImpl.inl"
#include "Globals.h"
#include "SimpleTimeline.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSimpleTimeline, Log, All);

/** Simpler class for implementing a time line via C++. */
UCLASS()
class USimpleTimeline : public UActorComponent
{
	GENERATED_BODY()

public:

	/** Constructor. */
	USimpleTimeline();

	/** Time line used to morph the player. */
	UPROPERTY()
	UTimelineComponent* timelineComponent;

	/** Return a time line and setup from single function. 
	 * @Param timelineCurve, initializes a time line.
	 * @Param timelineName, The name of the time line.
	 * @Param propertySetObject, The object that the time line callbacks will be called to.
	 * @Param callbackFunction, The function name to call for each tick of the time line.
	 * @Param finishFunction, The function name to call at the end of the time line.
	 * @Param owningActor, The actor that this time line will be attached to.
	 * @Param timelineVariableName, The name of the variable located in the property set object that the time line will change if none = NAME_None.
	 * @Param looping, should loop the time line after playing.
	 * @Param timelineLength, The length of the time line can be the total length of the curve of between the key points. */
	UFUNCTION(BlueprintCallable, Category = "Objects", meta = (DeterminesOutputType = "ObjClass"))
	static USimpleTimeline* CreateSimpleTimeline(UCurveFloat* timelineCurve, FName timelineName, UObject* propertySetObject,
			FName callbackFunction, FName finishFunction, AActor* owningActor, FName timelineVariableName = NAME_None, bool looping = false,
			ETimelineLengthMode timelineLength = ETimelineLengthMode::TL_LastKeyFrame, TEnumAsByte<ETimelineDirection::Type> timelineDirection = ETimelineDirection::Forward);


	/** Initializes a linear time line and returns it.
	* @Param timelineCurve, initializes a time line.
	* @Param propertySetObject, The object that the time line callbacks will be called to.
	* @Param callbackFunction, The function name to call for each tick of the time line.
	* @Param finishFunction, The function name to call at the end of the time line.
	* @Param owningActor, The actor that this time line will be attached to.
	* @Param timelineVariableName, The name of the variable located in the property set object that the time line will change if none = NAME_None.
	* @Param looping, should loop the time line after playing.
	* @Param timelineLength, The length of the time line can be the total length of the curve of between the key points. */
	UFUNCTION(BlueprintCallable, Category = "Objects", meta = (DeterminesOutputType = "ObjClass"))
	static USimpleTimeline* CreateLinearSimpleTimeline(FName timelineName, UObject* propertySetObject,
			FName callbackFunction, FName finishFunction, AActor* owningActor, FName timelineVariableName = NAME_None, bool looping = false,
			ETimelineLengthMode timelineLength = ETimelineLengthMode::TL_LastKeyFrame, TEnumAsByte<ETimelineDirection::Type> timelineDirection = ETimelineDirection::Forward);

	/** Is the time line playing? */
	UFUNCTION(BlueprintPure)
	bool IsPlaying() const;

	/** Is the time line playing in reverse? */
	UFUNCTION(BlueprintPure)
	bool IsReversing() const;

	/** Stop and reset the time line. */
	UFUNCTION(BlueprintCallable)
	void Stop();

	/** Pause the time line playback in its current location. */
	UFUNCTION(BlueprintCallable)
	void Pause();

	/** Play the time line from the start. */
	UFUNCTION(BlueprintCallable)
	void PlayFromStart();

	/** Continue playing from the current location of the time line state. */
	UFUNCTION(BlueprintCallable)
	void PlayFromCurrentLocation();

	/** Play the time line in reverse. */
	UFUNCTION(BlueprintCallable)
	void Reverse();

	/** Set the position of the time line. */
	UFUNCTION(BlueprintCallable)
	void SetPosition(int position, bool fireEvents = false, bool fireUpdateEvent = false);

	/** Set playrate of the time line. */
	UFUNCTION(BlueprintCallable)
	void SetPlayRate(float playrate);
};