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

void ARiverfrontWorld::DressServiceElevations(double X, double Front, double Bank, double Width,
    double Depth, double Height, int32 Style, bool bStreet)
{
    const int32 Floors = FMath::Clamp(FMath::RoundToInt(Height / 305.), 2, 7);
    const double Storey = Height / Floors;
    const auto Elevation = [&](FVector Center, FVector Along, FVector Out, double Span, bool bRear)
    {
        const FRotator Rotation = Along.Rotation();
        const auto Part = [&](ETint Tint, double Offset, double Projection, double Z,
            double W, double D, double H)
        {
            const FVector Location = Center + Along * Offset + Out * Projection + FVector(0, 0, Z);
            Box(Tint, Location, FVector(W, D, H), false, Rotation);
        };
        const int32 Bays = FMath::Clamp(FMath::FloorToInt(Span / 355.), 1, bStreet ? 4 : 3);
        const double Pitch = Span * .82 / Bays;
        const double ServiceEnd = ((Style % 2) ? -1. : 1.) * Span * .412;
        // A staggered stair light, high utility windows and projecting stone chajjas
        // give different depths to the walls seen on the walk around the block.
        for (int32 Floor = 0; Floor < Floors; ++Floor)
        {
            for (int32 Bay = 0; Bay < Bays; ++Bay)
            {
                if (bRear && Floor == 0 && Bay == 0) continue;
                const bool Stair = Bay == Bays - 1 && Style % 3 != 1;
                const double W = Stair ? 64. : FMath::Min(Pitch * .58, 146.);
                const double H = Stair ? 121. : 96.;
                const double Z = Storey * (Floor + (Stair ? .69 : .55));
                const double A = Pitch * (Bay - (Bays - 1) * .5);
                Part(ETint::Shadow, A, 2., Z, W + 14., 7., H + 12.);
                Part(ETint::Glass, A, 7., Z, W, 5., H);
                Part(ETint::FacadeStone, A, 13., Z - H * .5 - 4., W + 18., 29., 8.);
                if (bStreet || Stair)
                {
                    Part(ETint::Rail, A, 11., Z, 4., 5., H);
                    Part(ETint::Retaining, A, 20., Z + H * .5 + 8., W + 31., 42., 9.);
                }
                if (bStreet && Stair)
                {
                    for (int32 Louver = -1; Louver <= 1; ++Louver)
                        Part(ETint::Terracotta, A, 15., Z + Louver * 25., W + 1., 9., 7.);
                }
            }
        }
        if (!bStreet) return;
        Part(ETint::Retaining, ServiceEnd, 8., Height * .49, 7., 9., Height * .96);
        for (int32 Floor = 0; Floor < Floors; ++Floor)
            Part(ETint::Rail, ServiceEnd, 8., (Floor + .58) * Storey, 12., 14., 3.);
        if (bRear)
        {
            const double Door = -(Bays - 1) * Pitch * .5;
            Part(ETint::Shadow, Door, 2., 108., 107., 8., 216.);
            Part(ETint::Timber, Door, 8., 103., 94., 7., 206.);
            Part(ETint::Brass, Door + 31., 15., 101., 4., 8., 19.);
            Part(ETint::Retaining, Door, 23., 229., 142., 48., 11.);
        }
    };
    const double Middle = Front + Bank * Depth * .5;
    for (const double Side : {-1., 1.})
        Elevation(FVector(X + Side * Width * .5, Middle, 0), FVector(0, Bank, 0),
            FVector(Side, 0, 0), Depth, false);
    Elevation(FVector(X, Front + Bank * Depth, 0), FVector(1, 0, 0), FVector(0, Bank, 0), Width, true);
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

void ARiverfrontWorld::AddTeaCart(const FVector& Ground,double Yaw)
{
    const FRotator Rotation(0,Yaw,0);
    const auto At=[&](double X,double Y,double Z){return Ground+Rotation.RotateVector(FVector(X,Y,Z));};
    const auto Panel=[&](ETint Tint,double X,double Y,double Z,double W,double D,double H,bool Solid=false)
    { Box(Tint,At(X,Y,Z),FVector(W,D,H),Solid,Rotation); };
    Panel(ETint::Timber,0,0,71,174,84,51,true);
    Panel(ETint::Rail,0,0,99,186,96,5);
    for (double X:{-65.,65.}) for (double Y:{-42.,42.})
    {
        const FRotator Axle=(Rotation.Quaternion()*FQuat(FVector::ForwardVector,PI*.5)).Rotator();
        Shape(EShape::Cylinder,ETint::Shadow,At(X,Y,23),FVector(46,46,5),false,Axle);
        Shape(EShape::Cylinder,ETint::Brass,At(X,Y*1.06,23),FVector(22,22,5),false,Axle);
        Panel(ETint::Rail,X,Y,150,3,3,102);
    }
    for (int32 I=0;I<7;++I)
    {
        const ETint Cloth=I%2?ETint::Terracotta:ETint::Canvas;
        Panel(Cloth,-89.+I*29.5,0,205,29.5,119,4);
        Panel(Cloth,-89.+I*29.5,-58,196,29.5,3,17);
    }
    Shape(EShape::Cylinder,ETint::Rail,At(-43,5,119),FVector(31,31,35));
    Shape(EShape::Sphere,ETint::Rail,At(-43,5,137),FVector(33,33,9));
    Panel(ETint::Timber,-43,5,144,10,6,7);
    for (int32 I=0;I<5;++I)
        Shape(EShape::Cylinder,ETint::Ceramic,At(11+I*10,-16,106),FVector(7,7,9));
}
