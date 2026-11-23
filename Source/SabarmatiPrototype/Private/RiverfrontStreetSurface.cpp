#include "RiverfrontWorld.h"

void ARiverfrontWorld::DressStreetSurface()
{
    FRandomStream Layout(654107);
    for (const double Bank:{1.,-1.})
    {
        const double RoadY=Bank>0.?4090.:-26010.;
        for (double X=-79900;X<82700;X+=Layout.FRandRange(3650,6810))
        {
            if (FMath::Abs(X+64400)<1100. || FMath::Abs(X-59310)<1200. || FMath::Abs(X-3150)<1150.) continue;
            const double Y=RoadY+Bank*(Layout.FRandRange(-85,260));
            const double Turn=Layout.FRandRange(-125,125);
            const FRotator R(0,Turn,0);
            auto At=[&](double A,double B,double Z){return FVector(X,Y,Z)+R.RotateVector(FVector(A,B,0));};
            // A 600 mm recessed access cover with its cast frame, diamond tread and lifting slots.
            Shape(EShape::Cylinder,ETint::Rail,At(0,0,.65),FVector(70,70,1.1));
            Shape(EShape::Cylinder,ETint::Rail,At(0,0,1.27),FVector(63.8,63.8,.65));
            Shape(EShape::Cylinder,ETint::Shadow,At(0,0,1.62),FVector(58.5,58.5,.24));
            for (int32 Row=-2;Row<=2;++Row) for (int32 Col=-2;Col<=2;++Col)
            {
                if (Row*Row+Col*Col>5) continue;
                Box(ETint::Rail,At(Row*8.4,Col*8.4,1.86),FVector(7.4,2,.28),false,FRotator(0,Turn+(Row%2?43.:-43.),0));
            }
            for (double S:{-1.,1.}) Box(ETint::Shadow,At(0,23*S,2.),FVector(10.4,3.5,.3),false,R);
        }
        for (double X=-78000; X<82200; X+=2290.)
        {
            if (FMath::Abs(X+64400)<1000. || FMath::Abs(X-59310)<1000. || FMath::Abs(X-3150)<950.) continue;
            const double Y=RoadY+Bank*489.;
            Box(ETint::Shadow,FVector(X,Y,1.35),FVector(73,37,1.5));
            Box(ETint::Rail,FVector(X,Y,2.05),FVector(66,31,.75));
            for (int32 J=-4;J<=4;++J) Box(ETint::Shadow,FVector(X+J*6.9,Y,2.52),FVector(3.2,23,.42));
        }
    }
}
