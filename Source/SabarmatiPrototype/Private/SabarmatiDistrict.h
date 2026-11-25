#pragma once
#include "CoreMinimal.h"

namespace SabarmatiDistrict
{
constexpr double HalfWidth = 50000.;
constexpr double BankZ = 0.;
constexpr double BedZ = -900.;
constexpr double SurfaceZ = BedZ + .30 * (BankZ - BedZ);
inline FVector Center(bool City) { return City ? FVector(-12000., 19000., 0.) : FVector(0., -10960., 0.); }
inline bool Contains(const FVector& P, bool City)
{
    const FVector Delta = P - Center(City);
    return FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y)) <= HalfWidth;
}
// Clip in XY, preserving the interpolated height or width in Z.
inline bool Clip(FVector& A, FVector& B, bool City, double Margin = 0.)
{
    const FVector D = B-A, Relative = A-Center(City);
    const double Limit = HalfWidth-Margin;
    double Start=0., End=1.;
    for (int32 Axis=0; Axis<2; ++Axis)
    {
        if (FMath::IsNearlyZero(D[Axis]))
        {
            if (FMath::Abs(Relative[Axis])>Limit) return false;
            continue;
        }
        const double Near=(-Limit-Relative[Axis])/D[Axis], Far=(Limit-Relative[Axis])/D[Axis];
        Start=FMath::Max(Start,FMath::Min(Near,Far));
        End=FMath::Min(End,FMath::Max(Near,Far));
    }
    if (End<=Start) return false;
    B=A+End*D;
    A+=Start*D;
    return FVector::DistSquared2D(A,B)>1.;
}
}
