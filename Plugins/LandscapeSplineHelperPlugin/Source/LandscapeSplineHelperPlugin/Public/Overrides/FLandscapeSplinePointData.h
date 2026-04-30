// Copyright 2023 Arbitrary Games. All rights reserved.
#pragma once
#include "FLandscapeSplinePointData.generated.h"

/**
 * A single point in an ordered spline built from a landscape spline.
 * ArriveTangent and LeaveTangent are in world space and can be fed directly
 * into USplineComponent::SetTangentsAtSplinePoint(..., ESplineCoordinateSpace::World).
 */
USTRUCT(BlueprintType)
struct LANDSCAPESPLINEHELPERPLUGIN_API FLandscapeSplinePointData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Spline")
	FVector WorldLocation = FVector::ZeroVector;

	/** Tangent entering this point, in world space (same direction as travel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Spline")
	FVector ArriveTangent = FVector::ZeroVector;

	/** Tangent leaving this point, in world space (same direction as travel). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landscape Spline")
	FVector LeaveTangent = FVector::ZeroVector;
};
