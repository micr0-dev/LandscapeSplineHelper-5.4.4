// Copyright 2023 Arbitrary Games. All rights reserved.

#include "LandscapeSplineHelperPluginBPLibrary.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeSplineActor.h"
#include "LandscapeSplinesComponent.h"
#include "Overrides/ULandscapeControlPointWrapper.h"
#include "Overrides/USplineSegmentWrapper.h"

ULandscapeSplineHelperPluginBPLibrary::ULandscapeSplineHelperPluginBPLibrary(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULandscapeSplineHelperPluginBPLibrary::GetLandscapeSpline(ULandscapeSpline*& landscapeSpline, bool& success, const ALandscapeProxy* landscape)
{
	landscapeSpline = nullptr;
	success = false;

	if (!landscape) return;

	// In UE 5.3+, landscape splines were moved from ALandscapeProxy to a dedicated
	// ALandscapeSplineActor. Search the world for the spline actor that belongs to
	// this landscape by comparing ULandscapeInfo pointers.
	UWorld* World = landscape->GetWorld();
	ULandscapeInfo* LandscapeInfo = landscape->GetLandscapeInfo();

	if (World && LandscapeInfo)
	{
		for (TActorIterator<ALandscapeSplineActor> It(World); It; ++It)
		{
			ALandscapeSplineActor* SplineActor = *It;
			if (SplineActor && SplineActor->GetLandscapeInfo() == LandscapeInfo)
			{
				if (const auto Spline = SplineActor->GetSplinesComponent())
				{
					landscapeSpline = NewObject<ULandscapeSpline>();
					success = landscapeSpline->Init(Spline, SplineActor->GetTransform());
					return;
				}
			}
		}
	}

	// Legacy fallback: pre-5.3 splines lived directly on the proxy
	if (const auto Spline = landscape->GetSplinesComponent())
	{
		landscapeSpline = NewObject<ULandscapeSpline>();
		success = landscapeSpline->Init(Spline, landscape->GetTransform());
	}
}

void ULandscapeSplineHelperPluginBPLibrary::WrapLandscapeSplineActor(ULandscapeSpline*& landscapeSpline, bool& success, const ALandscapeSplineActor* actor)
{
	if(actor)
	{
		if (const auto Spline = actor->GetSplinesComponent())
		{
			landscapeSpline = NewObject<ULandscapeSpline>();
			const bool s = landscapeSpline->Init(Spline, actor->GetTransform());
			success = s;
			return;
		}
		success = false;
	}
}


