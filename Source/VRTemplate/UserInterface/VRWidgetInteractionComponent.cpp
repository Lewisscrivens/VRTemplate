// Fill out your copyright notice in the Description page of Project Settings.

#include "UserInterface/VRWidgetInteractionComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/IWidgetReflector.h"
#include "Framework/Application/SlateApplication.h"

void UVRWidgetInteractionComponent::PressPointerKey(FKey Key)
{
	GSlateFastWidgetPath = 0;

	Super::PressPointerKey(Key);
}

UWidgetInteractionComponent::FWidgetTraceResult UVRWidgetInteractionComponent::PerformTrace() const
{
	FWidgetTraceResult TraceResult;

	TArray<FHitResult> MultiHits;

	switch (InteractionSource)
	{
		case EWidgetInteractionSource::World:
		{
			const FVector WorldLocation = GetComponentLocation();
			const FTransform WorldTransform = GetComponentTransform();
			const FVector Direction = WorldTransform.GetUnitAxis(EAxis::X);

			TArray<UPrimitiveComponent*> PrimitiveChildren;
			GetRelatedComponentsToIgnoreInAutomaticHitTesting(PrimitiveChildren);

			FCollisionQueryParams Params = FCollisionQueryParams::DefaultQueryParam;
			Params.AddIgnoredComponents(PrimitiveChildren);

			TraceResult.LineStartLocation = WorldLocation;
			TraceResult.LineEndLocation = WorldLocation + (Direction * InteractionDistance);

			GetWorld()->LineTraceMultiByChannel(MultiHits, TraceResult.LineStartLocation, TraceResult.LineEndLocation, TraceChannel, Params);
			break;
		}
		case EWidgetInteractionSource::Mouse:
		case EWidgetInteractionSource::CenterScreen:
		{
			TArray<UPrimitiveComponent*> PrimitiveChildren;
			GetRelatedComponentsToIgnoreInAutomaticHitTesting(PrimitiveChildren);

			FCollisionQueryParams Params = FCollisionQueryParams::DefaultQueryParam;
			Params.AddIgnoredComponents(PrimitiveChildren);

			APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
			ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

			if (LocalPlayer && LocalPlayer->ViewportClient)
			{
				if (InteractionSource == EWidgetInteractionSource::Mouse)
				{
					FVector2D MousePosition;
					if (LocalPlayer->ViewportClient->GetMousePosition(MousePosition))
					{
						FVector WorldOrigin;
						FVector WorldDirection;
						if (UGameplayStatics::DeprojectScreenToWorld(PlayerController, MousePosition, WorldOrigin, WorldDirection) == true)
						{
							TraceResult.LineStartLocation = WorldOrigin;
							TraceResult.LineEndLocation = WorldOrigin + WorldDirection * InteractionDistance;

							GetWorld()->LineTraceMultiByChannel(MultiHits, TraceResult.LineStartLocation, TraceResult.LineEndLocation, TraceChannel, Params);
						}
					}
				}
				else if (InteractionSource == EWidgetInteractionSource::CenterScreen)
				{
					FVector2D ViewportSize;
					LocalPlayer->ViewportClient->GetViewportSize(ViewportSize);

					FVector WorldOrigin;
					FVector WorldDirection;
					if (UGameplayStatics::DeprojectScreenToWorld(PlayerController, ViewportSize * 0.5f, WorldOrigin, WorldDirection) == true)
					{
						TraceResult.LineStartLocation = WorldOrigin;
						TraceResult.LineEndLocation = WorldOrigin + WorldDirection * InteractionDistance;

						GetWorld()->LineTraceMultiByChannel(MultiHits, WorldOrigin, WorldOrigin + WorldDirection * InteractionDistance, TraceChannel, Params);
					}
				}
			}
			break;
		}
		case EWidgetInteractionSource::Custom:
		{
			TraceResult.HitResult = CustomHitResult;
			TraceResult.bWasHit = CustomHitResult.bBlockingHit;
			TraceResult.LineStartLocation = CustomHitResult.TraceStart;
			TraceResult.LineEndLocation = CustomHitResult.TraceEnd;
			break;
		}
	}

	// If it's not a custom interaction, we do some custom filtering to ignore invisible widgets.
	if (InteractionSource != EWidgetInteractionSource::Custom)
	{
		for (const FHitResult& HitResult : MultiHits)
		{
			if (UWidgetComponent* HitWidgetComponent = Cast<UWidgetComponent>(HitResult.GetComponent()))
			{
				if (HitWidgetComponent->IsVisible())
				{
					TraceResult.bWasHit = true;
					TraceResult.HitResult = HitResult;
					break;
				}
			}
			else
			{
				// If we hit something that wasn't a widget component, we're done.
				break;
			}
		}
	}

	// Resolve trace to location on widget.
	if (TraceResult.bWasHit)
	{
		TraceResult.HitWidgetComponent = Cast<UWidgetComponent>(TraceResult.HitResult.GetComponent());
		if (TraceResult.HitWidgetComponent)
		{
			// @todo WASTED WORK: GetLocalHitLocation() gets called in GetHitWidgetPath();

			if (TraceResult.HitWidgetComponent->GetGeometryMode() == EWidgetGeometryMode::Cylinder)
			{
				const FTransform WorldTransform = GetComponentTransform();
				const FVector Direction = WorldTransform.GetUnitAxis(EAxis::X);

				TTuple<FVector, FVector2D> CylinderHitLocation = TraceResult.HitWidgetComponent->GetCylinderHitLocation(TraceResult.HitResult.ImpactPoint, Direction);
				TraceResult.HitResult.ImpactPoint = CylinderHitLocation.Get<0>();
				TraceResult.LocalHitLocation = CylinderHitLocation.Get<1>();
			}
			else
			{
				ensure(TraceResult.HitWidgetComponent->GetGeometryMode() == EWidgetGeometryMode::Plane);
				TraceResult.HitWidgetComponent->GetLocalHitLocation(TraceResult.HitResult.ImpactPoint, TraceResult.LocalHitLocation);
			}
			TraceResult.HitWidgetPath = FindHoveredWidgetPath(TraceResult);
		}
	}



	return TraceResult;
}

FWidgetPath UVRWidgetInteractionComponent::DetermineWidgetUnderPointer()
{
	FWidgetPath WidgetPathUnderPointer;

	bIsHoveredWidgetInteractable = false;
	bIsHoveredWidgetFocusable = false;
	bIsHoveredWidgetHitTestVisible = false;

	UWidgetComponent* OldHoveredWidget = HoveredWidgetComponent;

	HoveredWidgetComponent = nullptr;

	FWidgetTraceResult TraceResult = PerformTrace();
	LastHitResult = TraceResult.HitResult;
	HoveredWidgetComponent = TraceResult.HitWidgetComponent;
	LastLocalHitLocation = LocalHitLocation;
	LocalHitLocation = TraceResult.bWasHit
		? TraceResult.LocalHitLocation
		: LastLocalHitLocation;
	WidgetPathUnderPointer = TraceResult.HitWidgetPath;

	if (HoveredWidgetComponent)
	{
		HoveredWidgetComponent->RequestRedraw();
	}

	if (WidgetPathUnderPointer.IsValid())
	{
		const FArrangedChildren::FArrangedWidgetArray& AllArrangedWidgets = WidgetPathUnderPointer.Widgets.GetInternalArray();
		for (const FArrangedWidget& ArrangedWidget : AllArrangedWidgets)
		{
			const TSharedRef<SWidget>& Widget = ArrangedWidget.Widget;
			if (Widget->IsEnabled())
			{
				if (Widget->IsInteractable())
				{
					bIsHoveredWidgetInteractable = true;
				}

				if (Widget->SupportsKeyboardFocus())
				{
					bIsHoveredWidgetFocusable = true;
				}
			}

			if (Widget->GetVisibility().IsHitTestVisible())
			{
				bIsHoveredWidgetHitTestVisible = true;
			}
		}
	}

	if (HoveredWidgetComponent != OldHoveredWidget)
	{
		if (OldHoveredWidget)
		{
			OldHoveredWidget->RequestRedraw();
		}

		OnHoveredWidgetChanged.Broadcast(HoveredWidgetComponent, OldHoveredWidget);
	}

	return WidgetPathUnderPointer;
}