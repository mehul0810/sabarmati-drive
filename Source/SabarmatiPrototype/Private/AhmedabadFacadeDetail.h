#pragma once

#include "CoreMinimal.h"

// Street-facing details for the offline building shells. Dimensions are in centimetres.
namespace AhmedabadFacadeDetail
{
template<typename AddGlass, typename AddTrim>
void Add(const FVector& Base, const FRotator& Rotation, double Width, double Depth, double Height,
    uint8 Style, AddGlass&& Glass, AddTrim&& Trim)
{
    if (Height < 275.0 || FMath::Min(Width, Depth) < 290.0)
    {
        return;
    }

    const auto Place = [&Base, &Rotation](auto&& Emit, const FVector& At, const FVector& Size)
    {
        Emit(Base + Rotation.RotateVector(At), Size, Rotation);
    };

    const int32 Floors = FMath::Clamp(FMath::RoundToInt(Height / 315.0), 1, 9);
    const double Floor = Height / Floors;
    const int32 Bays = FMath::Clamp(FMath::FloorToInt(Width / (Style == 1 ? 260.0 : 330.0)), 1, 12);
    const double Bay = Width / Bays;
    for (int32 Side : {-1, 1})
    {
        const double Y = Side * (Depth * .5 + 3.0);
        for (int32 Storey = 0; Storey < Floors; ++Storey)
        {
            const double Z = Floor * (Storey + .56);
            for (int32 Column = 0; Column < Bays; ++Column)
            {
                const double X = (Column + .5) * Bay - Width * .5;
                const bool bShopfront = Style == 1 && Storey == 0;
                const double PaneWidth = FMath::Min(Bay * (bShopfront ? .79 : .57), 265.0);
                Place(Glass, FVector(X, Y, Z), FVector(PaneWidth, 8.0, Floor * (bShopfront ? .69 : .43)));
                Place(Trim, FVector(X, Y + Side * 6.0, Z - Floor * (bShopfront ? .36 : .23)),
                    FVector(PaneWidth + 18.0, 15.0, 9.0));
            }
        }
        if (Width >= 510)
        {
            // A full-height entry and projecting rain hood break the ground-floor rhythm.
            const double DoorX = -Width * .5 + Bay;
            Place(Glass, FVector(DoorX, Y + Side * 7, 107), FVector(96, 12, 212));
            Place(Trim, FVector(DoorX, Y + Side * 30, 226), FVector(132, 63, 10));
        }
        if (Style == 2 && Floors >= 3 && Depth > 450.0)
        {
            // Continuous weather-sheds; deliberately shallow to stay within road clearances.
            for (int32 Storey = 1; Storey < Floors; Storey += 2)
            {
                Place(Trim, FVector(0.0, Y + Side * 15.0, Floor * Storey + 36.0),
                    FVector(Width - 24.0, 36.0, 14.0));
            }
        }
    }

    // Secondary walls use fewer bays than principal elevations.
    if (Depth >= 450.0)
    {
        for (const int32 Side : {-1, 1})
        {
            for (int32 Storey = 0; Storey < Floors; ++Storey)
            {
                const double Z = Floor * (Storey + .56);
                for (const double Y : {-Depth * .23, Depth * .23})
                {
                    Place(Glass, FVector(Side * (Width * .5 + 3.0), Y, Z),
                        FVector(8.0, FMath::Min(132.0, Depth * .23), Floor * .41));
                }
            }
        }
    }

    if (Width > 800 && Depth > 720 && Floors >= 2)
    {
        Place(Trim, FVector(Width * .17, -Depth * .13, Height + 84),
            FVector(FMath::Min(Width * .29, 350.), FMath::Min(Depth * .3, 270.), 158));
    }
    // Window AC housings and drains sit inside the clearance margin of the mapped footprint.
    for (int32 Side : {-1,1})
    {
        const double Y=Side*(Depth*.5+9.);
        if (Style!=1)
            Place(Trim,FVector(Width*.445,Y,Height*.51),FVector(7.,9.,Height*.98));
        for (int32 Storey=1; Storey<Floors; Storey+=2)
        {
            const double X=Width*((Storey+Style)%2?.32:-.25), Z=Floor*(Storey+.33);
            Place(Trim,FVector(X,Y+Side*7.,Z),FVector(67.,33.,46.));
            Place(Glass,FVector(X+11.,Y+Side*24.,Z),FVector(30.,3.,29.));
            if (Style==0 && Width>640.)
            {
                const double W=FMath::Min(296.,Width*.36);
                Place(Trim,FVector(-X,Y+Side*14.,Z-42.),FVector(W,48.,12.));
                Place(Trim,FVector(-X,Y+Side*35.,Z+35.),FVector(W,5.,5.));
                for (int32 Bar=-2; Bar<=2; ++Bar)
                    Place(Trim,FVector(-X+Bar*W*.22,Y+Side*35.,Z-3.),FVector(4.,4.,72.));
            }
        }
        if (Style==1 && Width>620.)
        {
            const double Canopy=FMath::Min(580.,Width*.75);
            Place(Trim,FVector(0,Y+Side*22.,244.),FVector(Canopy,56.,9.));
            Place(Trim,FVector(0,Y+Side*47.,239.),FVector(Canopy,4.,18.));
        }
    }
    const double RoofZ = Height + 22.0;
    for (const int32 Side : {-1, 1})
    {
        Place(Trim, FVector(0.0, Side * Depth * .5, RoofZ), FVector(Width, 12.0, 30.0));
        Place(Trim, FVector(Side * Width * .5, 0.0, RoofZ), FVector(12.0, Depth, 30.0));
    }
}
}
