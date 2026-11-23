#include "RiverfrontWorld.h"

void ARiverfrontWorld::DressFacade(double X, double Front, double Bank, double Width, double Height, int32 Style)
{
    const auto Point = [=](double Across, double Out, double Up)
    {
        return FVector(X + Across, Front - Bank * Out, Up);
    };
    const auto Panel = [&](ETint Tint, double Across, double Out, double Up, double W, double D, double H)
    {
        Box(Tint, Point(Across, Out, Up), FVector(W, D, H));
    };
    // Perforated clay screens shelter the stair on two facade families. The holes are open geometry.
    if (Style == 0 || Style == 4)
    {
        const double Centre = Width * (Style == 0 ? -.32 : .30);
        constexpr double Cell = 18.0;
        for (int32 Floor = 1; Floor * 302. + 253. < Height; ++Floor)
        {
            const double Bottom = Floor * 302. + 49.;
            Panel(ETint::FacadeStone, Centre, 26., Bottom - 4., 137., 36., 8.);
            Panel(ETint::FacadeStone, Centre, 26., Bottom + Cell * 10, 137., 36., 8.);
            // Each continuous rib is clipped to the 7 by 10 cell rectangle.
            for (const double Slope : {-1., 1.})
            {
                for (int32 I = -7; I <= 17; ++I)
                {
                    const double Intercept = I * Cell;
                    const double AtBottom = -Intercept / Slope;
                    const double AtTop = (10. * Cell - Intercept) / Slope;
                    const double A = FMath::Max(0., FMath::Min(AtBottom, AtTop));
                    const double B = FMath::Min(7. * Cell, FMath::Max(AtBottom, AtTop));
                    if (B - A < 1.) continue;
                    Beam(ETint::Terracotta, Point(Centre - 3.5 * Cell + A, 25., Bottom + Intercept + Slope * A),
                        Point(Centre - 3.5 * Cell + B, 25., Bottom + Intercept + Slope * B), 4.);
                }
            }
        }
    }
    // Painted rainwater leaders, offsets and wall clamps follow the actual edge of this building.
    const double PipeX = Width * (Style % 2 ? .455 : -.458);
    const FVector Foot = Point(PipeX, 20., 33.);
    const FVector Knee = Point(PipeX, 12., 75.);
    Beam(ETint::Retaining, Foot, Knee, 7.);
    Beam(ETint::Retaining, Knee, Point(PipeX, 12., Height + 17.), 7.);
    for (double Z=95.; Z<Height; Z+=225.) Panel(ETint::Rail, PipeX, 11., Z, 11., 17., 3.);
    if (Style == 2 || Style == 3)
    {
        const double SignX = -Width * .22;
        for (const double Z : {286., 326.})
            Beam(ETint::Rail, Point(SignX, 4., Z), Point(SignX, 76., Z), 3.);
        Panel(ETint::Timber, SignX, 52., 250., 5., 65., 61.);
        // Shallow ribbing on an aged aluminium face reads from the side lane.
        for (int32 I = 0; I < 4; ++I)
            Panel(ETint::Brass, SignX + 3.1, 53., 235. + I * 8., 1.2, 32. - I * 3., 2.4);
    }
}

void ARiverfrontWorld::DressMarketUtilities()
{
    // An overhead service run stays on the inland shop edge, clear of the vehicle lane and arcade passage.
    const double Y = 5072.;
    constexpr double PoleX[] = {-7490., -4890., -1940., 875., 3725., 6700.};
    for (int32 I = 0; I < UE_ARRAY_COUNT(PoleX); ++I)
    {
        const double X = PoleX[I];
        const double Height = 663. + (I % 3) * 13.;
        Box(ETint::Retaining, FVector(X,Y,Height * .5), FVector(18.,24.,Height), false);
        Box(ETint::Rail, FVector(X,Y,Height - 26.), FVector(11.,131.,8.));
        Box(ETint::Retaining, FVector(X,Y,58.), FVector(25.,30.,116.));
        for (int32 Wire = -1; Wire <= 1; ++Wire)
        {
            const double WireY = Y + Wire * 48.;
            for (int32 Ring = 0; Ring < 3; ++Ring)
                Shape(EShape::Cylinder, ETint::Ceramic, FVector(X, WireY, Height - 17. + Ring * 5.), FVector(10. - Ring,10. - Ring,4.));
            if (I == 0) continue;
            const double PreviousHeight = 663. + ((I - 1) % 3) * 13.;
            const auto Cable = [&](double T)
            {
                const double Sag = 4. * T * (1. - T) * (49. + 6. * Wire);
                return FVector(FMath::Lerp(PoleX[I - 1], X, T), WireY, FMath::Lerp(PreviousHeight, Height, T) - 6. - Sag);
            };
            for (int32 S = 0; S < 8; ++S)
                Beam(ETint::Rail, Cable(S / 8.), Cable((S + 1) / 8.), 1.65);
        }
        if (I % 2 == 0)
        {
            Box(ETint::Rail, FVector(X+13.,Y,183.), FVector(14.,32.,43.));
            Box(ETint::Retaining, FVector(X+21.,Y,183.), FVector(2.,26.,32.));
            Beam(ETint::Rail, FVector(X+14.,Y,204.), FVector(X,Y,328.), 2.3);
        }
    }
}
