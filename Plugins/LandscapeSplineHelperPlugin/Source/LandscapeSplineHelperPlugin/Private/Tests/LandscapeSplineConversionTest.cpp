// Copyright 2023 Arbitrary Games. All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "LandscapeSplineHelperPluginBPLibrary.h"
#include "LandscapeSpline.h"
#include "Overrides/USplineSegmentWrapper.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "Components/SplineComponent.h"

// ---------------------------------------------------------------------------
// Null-safety: null input must not crash and must return bSuccess=false
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeSplineConvert_NullInput,
	"LandscapeSplineHelper.ConvertToSplinePoints.NullInput",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLandscapeSplineConvert_NullInput::RunTest(const FString& Parameters)
{
	TArray<FSplinePoint> Points;
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

	TArray<FSplinePoint> Points;
	bool bSuccess = true;

	ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(EmptySpline, Points, bSuccess);

	TestFalse(TEXT("Empty spline: bSuccess should be false"), bSuccess);
	TestEqual(TEXT("Empty spline: output array should be empty"), Points.Num(), 0);
	return true;
}

// ---------------------------------------------------------------------------
// Point-count + tangent test: linear chain A --S0-- B --S1-- C
//
//   S0.Connections[0] = A (TangentLen=100), S0.Connections[1] = B (TangentLen=150)
//   S1.Connections[0] = B (TangentLen=120), S1.Connections[1] = C (TangentLen=80)
//
// Expected: 3 FSplinePoints in order A, B, C with CurveCustomTangent type,
// tangents scaled by TangentLen in the control point's forward direction.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLandscapeSplineConvert_LinearChain,
	"LandscapeSplineHelper.ConvertToSplinePoints.LinearChain",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLandscapeSplineConvert_LinearChain::RunTest(const FString& Parameters)
{
	ULandscapeSplineControlPoint* CPA = NewObject<ULandscapeSplineControlPoint>();
	ULandscapeSplineControlPoint* CPB = NewObject<ULandscapeSplineControlPoint>();
	ULandscapeSplineControlPoint* CPC = NewObject<ULandscapeSplineControlPoint>();

	// Landscape at origin so component transform = identity; world pos == local pos
	CPA->Location = FVector(0.f,    0.f, 0.f);
	CPB->Location = FVector(1000.f, 0.f, 0.f);
	CPC->Location = FVector(2000.f, 0.f, 0.f);

	// All pointing +X; Rotation is world-space in ULandscapeSplineControlPoint
	CPA->Rotation = FRotator(0.f, 0.f, 0.f);
	CPB->Rotation = FRotator(0.f, 0.f, 0.f);
	CPC->Rotation = FRotator(0.f, 0.f, 0.f);

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
	Spline->Segments.Add(W0);
	Spline->Segments.Add(W1);

	TArray<FSplinePoint> Points;
	bool bSuccess = false;
	ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(Spline, Points, bSuccess);

	TestTrue(TEXT("Linear chain: bSuccess"), bSuccess);
	TestEqual(TEXT("Linear chain: 2 segments → 3 points"), Points.Num(), 3);

	if (Points.Num() == 3)
	{
		const FVector Fwd = FVector::ForwardVector; // FRotator(0,0,0).Vector() == +X

		// All points must be CurveCustomTangent so our tangents are preserved
		TestEqual(TEXT("A.Type"), Points[0].Type, ESplinePointType::CurveCustomTangent);
		TestEqual(TEXT("B.Type"), Points[1].Type, ESplinePointType::CurveCustomTangent);
		TestEqual(TEXT("C.Type"), Points[2].Type, ESplinePointType::CurveCustomTangent);

		// Point A: start endpoint — arrive mirrors leave (TangentLen = S0.conn[0] = 100)
		TestEqual(TEXT("A.Position"),     Points[0].Position,     FVector(0.f, 0.f, 0.f));
		TestEqual(TEXT("A.LeaveTangent"), Points[0].LeaveTangent, Fwd * 100.f);
		TestEqual(TEXT("A.ArriveTangent == LeaveTangent (start endpoint)"),
		          Points[0].ArriveTangent, Points[0].LeaveTangent);

		// Point B: interior — arrive from S0.conn[1]=150, leave from S1.conn[0]=120
		TestEqual(TEXT("B.Position"),      Points[1].Position,      FVector(1000.f, 0.f, 0.f));
		TestEqual(TEXT("B.ArriveTangent"), Points[1].ArriveTangent, Fwd * 150.f);
		TestEqual(TEXT("B.LeaveTangent"),  Points[1].LeaveTangent,  Fwd * 120.f);

		// Point C: end endpoint — leave mirrors arrive (TangentLen = S1.conn[1] = 80)
		TestEqual(TEXT("C.Position"),     Points[2].Position,     FVector(2000.f, 0.f, 0.f));
		TestEqual(TEXT("C.ArriveTangent"),Points[2].ArriveTangent,Fwd * 80.f);
		TestEqual(TEXT("C.LeaveTangent == ArriveTangent (end endpoint)"),
		          Points[2].LeaveTangent, Points[2].ArriveTangent);
	}

	return true;
}

// ---------------------------------------------------------------------------
// Reversed segment list: same A-B-C chain but with S1 listed before S0.
// Count must still be 3 and endpoint-mirroring invariant must hold.
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
	Spline->Segments.Add(W1); // intentionally reversed
	Spline->Segments.Add(W0);

	TArray<FSplinePoint> Points;
	bool bSuccess = false;
	ULandscapeSplineHelperPluginBPLibrary::ConvertLandscapeSplineToSplinePoints(Spline, Points, bSuccess);

	TestTrue(TEXT("Reversed list: bSuccess"), bSuccess);
	TestEqual(TEXT("Reversed list: still 3 points"), Points.Num(), 3);

	if (Points.Num() == 3)
	{
		// Endpoint points always have ArriveTangent == LeaveTangent
		TestEqual(TEXT("First point: arrive == leave (endpoint)"),
		          Points[0].ArriveTangent, Points[0].LeaveTangent);
		TestEqual(TEXT("Last point: arrive == leave (endpoint)"),
		          Points[2].ArriveTangent, Points[2].LeaveTangent);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
