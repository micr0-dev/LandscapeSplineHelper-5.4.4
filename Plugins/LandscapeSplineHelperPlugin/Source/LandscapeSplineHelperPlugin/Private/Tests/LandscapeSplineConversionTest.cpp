// Copyright 2023 Arbitrary Games. All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "LandscapeSplineHelperPluginBPLibrary.h"
#include "LandscapeSpline.h"
#include "Overrides/USplineSegmentWrapper.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"

// ---------------------------------------------------------------------------
// Null-safety: null input must not crash and must return bSuccess=false
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeSplineConvert_NullInput,
	"LandscapeSplineHelper.ConvertToSplinePoints.NullInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLandscapeSplineConvert_NullInput::RunTest(const FString& Parameters)
{
	TArray<FLandscapeSplinePointData> Points;
	bool bSuccess = true;

	ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(nullptr, Points, bSuccess);

	TestFalse(TEXT("Null spline: bSuccess should be false"), bSuccess);
	TestEqual(TEXT("Null spline: output array should be empty"), Points.Num(), 0);
	return true;
}

// ---------------------------------------------------------------------------
// Empty spline (no segments): must not crash and must return bSuccess=false
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeSplineConvert_EmptySpline,
	"LandscapeSplineHelper.ConvertToSplinePoints.EmptySpline",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLandscapeSplineConvert_EmptySpline::RunTest(const FString& Parameters)
{
	ULandscapeSpline* EmptySpline = NewObject<ULandscapeSpline>();
	// No segments added — Segments array is empty by default

	TArray<FLandscapeSplinePointData> Points;
	bool bSuccess = true;

	ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(EmptySpline, Points, bSuccess);

	TestFalse(TEXT("Empty spline: bSuccess should be false"), bSuccess);
	TestEqual(TEXT("Empty spline: output array should be empty"), Points.Num(), 0);
	return true;
}

// ---------------------------------------------------------------------------
// Point-count test: a linear chain of N segments must produce N+1 points.
//
// This test uses real ULandscapeSplineControlPoint / ULandscapeSplineSegment
// objects created via NewObject so they are GC-safe and do not require a
// full level/landscape to exist.
//
// Chain layout:   A --S0-- B --S1-- C
//   S0.Connections[0] = A (TangentLen=100), S0.Connections[1] = B (TangentLen=150)
//   S1.Connections[0] = B (TangentLen=120), S1.Connections[1] = C (TangentLen=80)
// Expected output: 3 points in order A, B, C.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeSplineConvert_LinearChain,
	"LandscapeSplineHelper.ConvertToSplinePoints.LinearChain",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLandscapeSplineConvert_LinearChain::RunTest(const FString& Parameters)
{
	// Build three control points
	ULandscapeSplineControlPoint* CPA = NewObject<ULandscapeSplineControlPoint>();
	ULandscapeSplineControlPoint* CPB = NewObject<ULandscapeSplineControlPoint>();
	ULandscapeSplineControlPoint* CPC = NewObject<ULandscapeSplineControlPoint>();

	// Positions (world-space, landscape at origin so component transform = identity)
	CPA->Location = FVector(0.f,    0.f, 0.f);
	CPB->Location = FVector(1000.f, 0.f, 0.f);
	CPC->Location = FVector(2000.f, 0.f, 0.f);

	// Rotations pointing +X (forward)
	CPA->Rotation = FRotator(0.f, 0.f, 0.f);
	CPB->Rotation = FRotator(0.f, 0.f, 0.f);
	CPC->Rotation = FRotator(0.f, 0.f, 0.f);

	// Build two segments
	ULandscapeSplineSegment* Seg0 = NewObject<ULandscapeSplineSegment>();
	ULandscapeSplineSegment* Seg1 = NewObject<ULandscapeSplineSegment>();

	Seg0->Connections[0].ControlPoint = CPA;  Seg0->Connections[0].TangentLen = 100.f;
	Seg0->Connections[1].ControlPoint = CPB;  Seg0->Connections[1].TangentLen = 150.f;
	Seg1->Connections[0].ControlPoint = CPB;  Seg1->Connections[0].TangentLen = 120.f;
	Seg1->Connections[1].ControlPoint = CPC;  Seg1->Connections[1].TangentLen = 80.f;

	// Wrap in plugin objects (identity component transform)
	USplineSegmentWrapper* W0 = NewObject<USplineSegmentWrapper>();
	USplineSegmentWrapper* W1 = NewObject<USplineSegmentWrapper>();
	W0->Init(Seg0, FTransform::Identity);
	W1->Init(Seg1, FTransform::Identity);

	ULandscapeSpline* Spline = NewObject<ULandscapeSpline>();
	Spline->Segments.Add(W0);
	Spline->Segments.Add(W1);

	TArray<FLandscapeSplinePointData> Points;
	bool bSuccess = false;
	ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(Spline, Points, bSuccess);

	TestTrue(TEXT("Linear chain: bSuccess"), bSuccess);
	TestEqual(TEXT("Linear chain: 2 segments → 3 points"), Points.Num(), 3);

	if (Points.Num() == 3)
	{
		const FVector Forward = FVector::ForwardVector; // Rotation(0,0,0).Vector() == +X

		// Point A: start endpoint — arrive mirrors leave
		TestEqual(TEXT("A.WorldLocation"), Points[0].WorldLocation, FVector(0.f, 0.f, 0.f));
		TestEqual(TEXT("A.LeaveTangent"), Points[0].LeaveTangent, Forward * 100.f);
		TestEqual(TEXT("A.ArriveTangent == LeaveTangent (start endpoint)"), Points[0].ArriveTangent, Points[0].LeaveTangent);

		// Point B: interior — arrive from S0.Conn[1], leave from S1.Conn[0]
		TestEqual(TEXT("B.WorldLocation"), Points[1].WorldLocation, FVector(1000.f, 0.f, 0.f));
		TestEqual(TEXT("B.ArriveTangent"), Points[1].ArriveTangent, Forward * 150.f);
		TestEqual(TEXT("B.LeaveTangent"),  Points[1].LeaveTangent,  Forward * 120.f);

		// Point C: end endpoint — leave mirrors arrive
		TestEqual(TEXT("C.WorldLocation"), Points[2].WorldLocation, FVector(2000.f, 0.f, 0.f));
		TestEqual(TEXT("C.ArriveTangent"), Points[2].ArriveTangent, Forward * 80.f);
		TestEqual(TEXT("C.LeaveTangent == ArriveTangent (end endpoint)"), Points[2].LeaveTangent, Points[2].ArriveTangent);
	}

	return true;
}

// ---------------------------------------------------------------------------
// Reverse-order segments: the same A-B-C chain but with S1 listed before S0.
// The result should still be 3 points (order may start from the other end,
// but the count and endpoint invariants must hold).
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeSplineConvert_ReversedSegmentList,
	"LandscapeSplineHelper.ConvertToSplinePoints.ReversedSegmentList",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLandscapeSplineConvert_ReversedSegmentList::RunTest(const FString& Parameters)
{
	ULandscapeSplineControlPoint* CPA = NewObject<ULandscapeSplineControlPoint>();
	ULandscapeSplineControlPoint* CPB = NewObject<ULandscapeSplineControlPoint>();
	ULandscapeSplineControlPoint* CPC = NewObject<ULandscapeSplineControlPoint>();

	CPA->Location = FVector(0.f,    0.f, 0.f);
	CPB->Location = FVector(1000.f, 0.f, 0.f);
	CPC->Location = FVector(2000.f, 0.f, 0.f);
	CPA->Rotation = CPB->Rotation = CPC->Rotation = FRotator::ZeroRotator;

	ULandscapeSplineSegment* Seg0 = NewObject<ULandscapeSplineSegment>();
	ULandscapeSplineSegment* Seg1 = NewObject<ULandscapeSplineSegment>();
	Seg0->Connections[0].ControlPoint = CPA;  Seg0->Connections[0].TangentLen = 100.f;
	Seg0->Connections[1].ControlPoint = CPB;  Seg0->Connections[1].TangentLen = 150.f;
	Seg1->Connections[0].ControlPoint = CPB;  Seg1->Connections[0].TangentLen = 120.f;
	Seg1->Connections[1].ControlPoint = CPC;  Seg1->Connections[1].TangentLen = 80.f;

	USplineSegmentWrapper* W0 = NewObject<USplineSegmentWrapper>();
	USplineSegmentWrapper* W1 = NewObject<USplineSegmentWrapper>();
	W0->Init(Seg0, FTransform::Identity);
	W1->Init(Seg1, FTransform::Identity);

	ULandscapeSpline* Spline = NewObject<ULandscapeSpline>();
	// Intentionally reversed order
	Spline->Segments.Add(W1);
	Spline->Segments.Add(W0);

	TArray<FLandscapeSplinePointData> Points;
	bool bSuccess = false;
	ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(Spline, Points, bSuccess);

	TestTrue(TEXT("Reversed list: bSuccess"), bSuccess);
	TestEqual(TEXT("Reversed list: still 3 points"), Points.Num(), 3);

	// Regardless of which end is chosen as start, the two endpoint points must
	// have identical arrive and leave tangents (endpoint-mirroring invariant).
	if (Points.Num() == 3)
	{
		TestEqual(TEXT("First point: arrive == leave (endpoint)"),  Points[0].ArriveTangent, Points[0].LeaveTangent);
		TestEqual(TEXT("Last point: arrive == leave (endpoint)"),   Points[2].ArriveTangent, Points[2].LeaveTangent);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
