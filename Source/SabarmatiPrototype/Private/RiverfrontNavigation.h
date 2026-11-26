#pragma once

#include "CoreMinimal.h"
#include "RiverfrontCrossings.h"
#include "RiverfrontWorld.h"
#include "SabarmatiDistrict.h"

namespace RiverfrontNavigation
{
struct FCrossing
{
    double X, HalfWidth, WestEnd, EastEnd, WestApproach, EastApproach;
    bool bPedestrian;
};

// Atal is a footbridge; the other spans carry vehicles. The Sardar crossing is
// outside the current playable boundary and must not be offered as a route.
constexpr FCrossing Atal{ARiverfrontWorld::AtalX, 500., 3290., -25210., 3410., -25330., true};
constexpr FCrossing Roads[] = {
    {RiverfrontCrossings::CentralRoadX, 360., 3800., -25720., 4090., -26010., false},
    {57890., 320., 3800., -25720., 4090., -26010., false},
    {59310., 320., 3800., -25720., 4090., -26010., false}
};

inline bool OnCrossing(const FVector& Here, const FCrossing& C, double PawnHeight)
{
    if (Here.Y >= C.WestEnd || Here.Y <= C.EastEnd || FMath::Abs(Here.X-C.X) >= C.HalfWidth) return false;
    const double Deck = C.bPedestrian ? 23. : RiverfrontCrossings::DeckZ(Here.Y);
    return FMath::Abs(Here.Z-PawnHeight-Deck) < 80.;
}

inline FVector Waypoint(const FVector& Here, const FVector& Target, bool Driving, double PawnHeight)
{
    const int32 TargetBank = Target.Y > 2300. ? 1 : Target.Y < -24300. ? -1 : 0;
    if (!TargetBank) return Target;

    // Once on a span, keep the marker on that span until its destination end.
    const FCrossing* On = !Driving && OnCrossing(Here, Atal, PawnHeight) ? &Atal : nullptr;
    if (!On)
        for (const FCrossing& Road : Roads)
            if (OnCrossing(Here, Road, PawnHeight)) { On=&Road; break; }
    if (On)
        return FVector(Here.X, TargetBank > 0 ? On->WestApproach : On->EastApproach, 90.);

    const int32 HereBank = Here.Y > RiverfrontCrossings::CentreY ? 1 : -1;
    if (HereBank == TargetBank) return Target;

    const FCrossing* Best = Driving ? nullptr : &Atal;
    if (Driving)
    {
        double BestDistance = TNumericLimits<double>::Max();
        for (const FCrossing& Road : Roads)
        {
            if (!SabarmatiDistrict::Contains(FVector(Road.X, Road.WestApproach, 0.), false)) continue;
            const double Distance = FMath::Abs(Here.X-Road.X) + FMath::Abs(Target.X-Road.X);
            if (Distance < BestDistance) { BestDistance=Distance; Best=&Road; }
        }
    }
    if (!Best) return Target;

    // Match the traffic's left-hand lane instead of pointing cars at the centre line.
    const double LaneX = Best->X + (Driving ? TargetBank * Best->HalfWidth * .46 : 0.);
    const bool Aligned = FMath::Abs(Here.X-LaneX) < Best->HalfWidth * .65;
    const double Side = Aligned ? TargetBank : HereBank;
    return FVector(LaneX, Side > 0 ? Best->WestApproach : Best->EastApproach, 90.);
}
}
