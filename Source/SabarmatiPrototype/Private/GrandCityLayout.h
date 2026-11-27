#pragma once
#include "Math/UnrealMathUtility.h"

// Street centerlines, lot boundaries and destinations in Unreal centimetres.
namespace GrandCityLayout
{
constexpr int BoulevardWidth = 1120;
constexpr int LocalWidth = 840;
constexpr int CanalWest = -14500;
constexpr int CanalEast = -4400;
constexpr int WestAbutment = -22000;
constexpr int EastAbutment = 3100;
constexpr int WestStreets[] = {-55000, -43500, -27000};
constexpr int EastStreets[] = {8600, 20500, 33000};
constexpr int CrossStreets[] = {9000, 32000, 50000, 69000, 92000};
constexpr int MarketStreet = CrossStreets[1];
constexpr int FoundryStreet = CrossStreets[3];
constexpr int StartX = -35500;
constexpr int CarY = MarketStreet + 462;
constexpr int WalkY = MarketStreet + 688;
constexpr int MarketX = -39500;
constexpr int ParkX = 14400;
constexpr int QuayX = 4700;
constexpr int FoundryX = -33200;

inline bool HasBridge(int StreetY)
{
    return StreetY == MarketStreet || StreetY == FoundryStreet;
}

inline int CrossWidth(int StreetY)
{
    return HasBridge(StreetY) ? BoulevardWidth : LocalWidth;
}

inline double BridgeHeight(double X)
{
    const double Distance=FMath::Min(X-WestAbutment,EastAbutment-X);
    const double Grade=FMath::Clamp(Distance/7400.,0.,1.);
    return 460.*Grade*Grade*(3.-2.*Grade);
}

inline double RoadHeight(double X,double Y)
{
    return FMath::Min(FMath::Abs(Y-MarketStreet),FMath::Abs(Y-FoundryStreet))<=BoulevardWidth*.5
        ? BridgeHeight(X) : 0.;
}
}
