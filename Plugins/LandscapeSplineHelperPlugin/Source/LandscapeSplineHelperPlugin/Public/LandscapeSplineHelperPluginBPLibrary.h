// Copyright 2023 Arbitrary Games. All rights reserved.

#pragma once

#include "LandscapeSpline.h"
#include "Landscape.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/SplineComponent.h"
#include "LandscapeSplineHelperPluginBPLibrary.generated.h"

UCLASS()
class ULandscapeSplineHelperPluginBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	
	/* Returns a representation of the splines given by the landscape. */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Get Landscape Spline", Keywords = "LandscapeSpline"), Category = "LandscapeSplineHelper")
	static void GetLandscapeSpline(ULandscapeSpline*& landscapeSpline, bool& success, const ALandscapeProxy* landscape);

	/* Returns a blueprint usable object with the landscape spline. */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Make Landscape Object From Actor", Keywords = "LandscapeSpline"), Category = "LandscapeSplineHelper")
	static void WrapLandscapeSplineActor(ULandscapeSpline*& landscapeSpline, bool& success, const ALandscapeSplineActor* actor);

	/**
	 * Converts a landscape spline into an ordered array of native FSplinePoint values
	 * that can be fed directly into USplineComponent::AddPoint / AddPoints.
	 *
	 * Each FSplinePoint is produced with:
	 *   - Position      : world-space location (matches GetWorldLocation on the control point)
	 *   - ArriveTangent : world-space Hermite arrive tangent
	 *   - LeaveTangent  : world-space Hermite leave tangent
	 *   - Type          : CurveCustomTangent  ← critical: keeps our tangents instead of
	 *                     letting the engine recompute them (which AddSplineWorldPoint does)
	 *
	 * Blueprint usage:
	 *   1. ClearSplinePoints (bUpdateSpline = false)
	 *   2. For each FSplinePoint: AddPoint (Point, bUpdateSpline = false)
	 *   3. UpdateSpline
	 *
	 * Note: Position is in world space. AddPoints treats it as local space, so this
	 * works correctly when the SplineComponent is at world origin. If the component
	 * is offset, subtract its world location from each Position before calling AddPoint.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Convert Landscape Spline To Spline Points", Keywords = "LandscapeSpline Spline Points Convert"), Category = "LandscapeSplineHelper")
	static void ConvertLandscapeSplineToSplinePoints(const ULandscapeSpline* LandscapeSpline, TArray<FSplinePoint>& SplinePoints, bool& bSuccess);
};
