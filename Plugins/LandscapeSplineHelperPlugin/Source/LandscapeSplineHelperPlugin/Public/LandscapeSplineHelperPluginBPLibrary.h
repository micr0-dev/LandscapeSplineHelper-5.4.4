// Copyright 2023 Arbitrary Games. All rights reserved.

#pragma once

#include "LandscapeSpline.h"
#include "Landscape.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Overrides/FLandscapeSplinePointData.h"
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
	 * Converts a landscape spline into an ordered array of spline points suitable for
	 * populating a USplineComponent.  Each point carries a world-space location and
	 * world-space arrive/leave tangents that reproduce the Hermite curve of the original
	 * landscape spline.
	 *
	 * For branching networks only the first chain found is returned.  For simple
	 * road/path splines (no branches) the full ordered chain is always returned.
	 *
	 * Usage in Blueprint:
	 *   1. Call this node to get SplinePoints.
	 *   2. Clear your USplineComponent (RemoveSplinePoint all / ClearSplinePoints).
	 *   3. For each point: AddSplineWorldPoint(Point.WorldLocation),
	 *      then SetTangentsAtSplinePoint(index, Point.ArriveTangent, Point.LeaveTangent, World).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "Convert Landscape Spline To Spline Points", Keywords = "LandscapeSpline Spline Points Convert"), Category = "LandscapeSplineHelper")
	static void ConvertLandscapeSplineToSplinePoints(const ULandscapeSpline* LandscapeSpline, TArray<FLandscapeSplinePointData>& SplinePoints, bool& bSuccess);
};
