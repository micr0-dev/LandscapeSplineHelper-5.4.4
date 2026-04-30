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

void ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(
	const ULandscapeSpline* LandscapeSpline,
	TArray<FLandscapeSplinePointData>& SplinePoints,
	bool& bSuccess)
{
	SplinePoints.Empty();
	bSuccess = false;

	if (!LandscapeSpline || LandscapeSpline->Segments.IsEmpty()) return;

	// --- Step 1: build a graph ---
	// Map each underlying control point to every (segment, conn_index) touching it.
	// conn_index 0 = start of that segment, 1 = end.
	// We use the raw ULandscapeSplineControlPoint* as key so that different wrapper
	// instances wrapping the same engine object compare equal.
	using FSegConn = TPair<USplineSegmentWrapper*, int32>;
	TMap<ULandscapeSplineControlPoint*, TArray<FSegConn>> CPToSegs;

	for (USplineSegmentWrapper* Seg : LandscapeSpline->Segments)
	{
		if (!Seg) continue;
		FBlueprintableSplineSegmentConnection C0, C1;
		Seg->GetConnections(false, C0, C1);

		if (C0.ControlPoint && C0.ControlPoint->GetOriginalControlPoint())
			CPToSegs.FindOrAdd(C0.ControlPoint->GetOriginalControlPoint()).Emplace(Seg, 0);
		if (C1.ControlPoint && C1.ControlPoint->GetOriginalControlPoint())
			CPToSegs.FindOrAdd(C1.ControlPoint->GetOriginalControlPoint()).Emplace(Seg, 1);
	}

	if (CPToSegs.IsEmpty()) return;

	// --- Step 2: find a chain endpoint ---
	// An endpoint is a control point connected to exactly one segment.
	// If the spline is a closed loop every point has 2 connections, so we fall back
	// to the first point we find.
	ULandscapeSplineControlPoint* StartCP = nullptr;
	for (auto& Pair : CPToSegs)
	{
		if (Pair.Value.Num() == 1)
		{
			StartCP = Pair.Key;
			break;
		}
	}
	if (!StartCP)
	{
		// Closed loop: no endpoint exists, start from any point
		auto It = CPToSegs.CreateIterator();
		if (It) StartCP = It.Key();
	}

	// --- Step 3: walk the chain ---
	TSet<USplineSegmentWrapper*> VisitedSegs;
	ULandscapeSplineControlPoint* CurrentCP  = StartCP;
	USplineSegmentWrapper*        PrevSeg    = nullptr;
	int32                         PrevConnIdx = -1; // which conn of PrevSeg is CurrentCP

	while (CurrentCP)
	{
		TArray<FSegConn>* ConnectedSegs = CPToSegs.Find(CurrentCP);
		if (!ConnectedSegs) break;

		// Retrieve a wrapper for CurrentCP (any segment that touches it works)
		ULandscapeControlPointWrapper* CPWrapper = nullptr;
		for (const FSegConn& SC : *ConnectedSegs)
		{
			FBlueprintableSplineSegmentConnection C0, C1;
			SC.Key->GetConnections(false, C0, C1);
			ULandscapeControlPointWrapper* Candidate = (SC.Value == 0) ? C0.ControlPoint : C1.ControlPoint;
			if (Candidate && Candidate->GetOriginalControlPoint() == CurrentCP)
			{
				CPWrapper = Candidate;
				break;
			}
		}
		if (!CPWrapper) break;

		// Find the next unvisited segment leaving this point
		USplineSegmentWrapper* NextSeg     = nullptr;
		int32                  NextConnIdx = -1;
		for (const FSegConn& SC : *ConnectedSegs)
		{
			if (!VisitedSegs.Contains(SC.Key))
			{
				NextSeg     = SC.Key;
				NextConnIdx = SC.Value;
				break;
			}
		}

		// Tangent magnitudes:
		//   - Arrive: from the segment we just walked in on (PrevSeg), at the conn that
		//     touches CurrentCP.
		//   - Leave : from the segment we are about to walk out on (NextSeg), at the conn
		//     that touches CurrentCP.
		// ULandscapeSplineControlPoint::Rotation is stored in world space, so
		// Rotation.Vector() * TangentLen gives the correct world-space tangent.
		float ArriveTLen = 0.f;
		float LeaveTLen  = 0.f;

		if (PrevSeg)
		{
			FBlueprintableSplineSegmentConnection PC0, PC1;
			PrevSeg->GetConnections(false, PC0, PC1);
			ArriveTLen = (PrevConnIdx == 0) ? PC0.TangentLen : PC1.TangentLen;
		}
		if (NextSeg)
		{
			FBlueprintableSplineSegmentConnection NC0, NC1;
			NextSeg->GetConnections(false, NC0, NC1);
			LeaveTLen = (NextConnIdx == 0) ? NC0.TangentLen : NC1.TangentLen;
		}

		// At a chain endpoint with no prior segment, mirror the leave tangent
		if (!PrevSeg) ArriveTLen = LeaveTLen;
		// At the final point with no next segment, mirror the arrive tangent
		if (!NextSeg) LeaveTLen  = ArriveTLen;

		const FVector Forward = CPWrapper->GetWorldForwardVector();
		FLandscapeSplinePointData Point;
		Point.WorldLocation  = CPWrapper->GetWorldLocation();
		Point.ArriveTangent  = Forward * ArriveTLen;
		Point.LeaveTangent   = Forward * LeaveTLen;
		SplinePoints.Add(Point);

		if (!NextSeg) break;

		VisitedSegs.Add(NextSeg);
		PrevSeg = NextSeg;

		// Advance: move to the other end of NextSeg
		FBlueprintableSplineSegmentConnection NC0, NC1;
		NextSeg->GetConnections(false, NC0, NC1);
		const int32 OtherIdx = (NextConnIdx == 0) ? 1 : 0;
		ULandscapeControlPointWrapper* OtherCP = (OtherIdx == 0) ? NC0.ControlPoint : NC1.ControlPoint;

		PrevConnIdx = OtherIdx;
		CurrentCP   = OtherCP ? OtherCP->GetOriginalControlPoint() : nullptr;
	}

	bSuccess = SplinePoints.Num() > 0;
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


