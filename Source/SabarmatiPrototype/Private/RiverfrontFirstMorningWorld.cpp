#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontWorld.h"

#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
// Centimetres. The facade faces the river (-Y), immediately east of the existing arcade.
FVector At(double X, double Y, double Z) { return FVector(-2520.+X, 5670.+Y, Z); }
constexpr double Upper = 330.;
constexpr double ApartmentGroundZ = 4.;
constexpr double FirstRiser = -42.;
constexpr double Tread = 30.;
constexpr int32 Steps = 20;
constexpr double Rise = (Upper-ApartmentGroundZ)/Steps;
constexpr double Landing = FirstRiser + Tread*Steps;
constexpr double FenceX = 782., FenceY = -111.;
constexpr double FenceWidth = 318., FenceDepth = 9., FenceRailZ = 90., FenceRailHeight = 12.;
constexpr double WallX = 1234., WallY = -32.;

void Lettering(ARiverfrontWorld& World, const TCHAR* Words, FVector Position, double Yaw,
    float Size, FColor Ink)
{
    auto* Sign = NewObject<UTextRenderComponent>(&World);
    World.AddInstanceComponent(Sign);
    Sign->SetupAttachment(World.GetRootComponent());
    Sign->SetMobility(EComponentMobility::Static);
    Sign->SetHorizontalAlignment(EHTA_Center);
    Sign->SetVerticalAlignment(EVRTA_TextCenter);
    Sign->SetText(FText::FromString(Words));
    Sign->SetTextRenderColor(Ink);
    Sign->SetWorldSize(Size);
    Sign->SetRelativeLocationAndRotation(Position, FRotator(0,Yaw,0));
    Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sign->RegisterComponent();
}

void InteriorLight(ARiverfrontWorld& World, FVector Position, float Lumens, float Radius, float Kelvin)
{
    auto* Light = NewObject<UPointLightComponent>(&World);
    World.AddInstanceComponent(Light);
    Light->SetupAttachment(World.GetRootComponent());
    Light->SetMobility(EComponentMobility::Movable);
    Light->SetIntensityUnits(ELightUnits::Lumens);
    Light->SetIntensity(Lumens);
    Light->SetAttenuationRadius(Radius);
    Light->SetUseTemperature(true);
    Light->SetTemperature(Kelvin);
    Light->SetSourceRadius(16.f);
    Light->SetCastShadows(false);
    Light->MaxDrawDistance = 2400;
    Light->MaxDistanceFadeRange = 400;
    Light->SetRelativeLocation(Position);
    Light->RegisterComponent();
}

struct FClothSection
{
    TArray<FVector> Vertices, Normals;
    TArray<FVector2D> UV;
    TArray<int32> Triangles;

    void Add(int32 Across, int32 Rows, TFunctionRef<FVector(double,double)> Position)
    {
        const int32 Base = Vertices.Num();
        for (int32 J=0; J<=Rows; ++J)
        {
            for (int32 I=0; I<=Across; ++I)
            {
                const double U=double(I)/Across, V=double(J)/Rows;
                const FVector Tangent=Position(U+.001,V)-Position(U-.001,V);
                const FVector Bitangent=Position(U,V+.001)-Position(U,V-.001);
                Vertices.Add(Position(U,V));
                Normals.Add(FVector::CrossProduct(Tangent,Bitangent).GetSafeNormal());
                UV.Emplace(U,V);
                if (J && I)
                {
                    const int32 D=Base+J*(Across+1)+I, C=D-1, B=D-Across-1, A=B-1;
                    Triangles.Append({A,C,B,B,C,D});
                }
            }
        }
    }
};

void FitTextiles(ARiverfrontWorld& World)
{
    enum { Indigo, Ivory, Madder, Marigold, Sage, Count };
    FClothSection Panels[Count];
    FClothSection Covers[2][3];
    for (int32 Occupied=0;Occupied<=1;++Occupied)
    {
        auto& Cover=Covers[Occupied];
        auto Quilt=[Occupied](double U,double V)
        {
            const double Across=2.*U-1.;
            const double Roll=FMath::Clamp((FMath::Abs(Across)-.84)/.16,0.,1.);
            const double Foot=FMath::Clamp((.10-V)/.10,0.,1.);
            const double Z=Upper+58.5-26.*Roll*Roll-17.*Foot*Foot
                +1.3*FMath::Sin(16.*U+3.*V)+.8*FMath::Sin(26.*U-6.*V);
            const double X=-218.+94.*Across, Y=472.+168.*V;
            // Interpolate the cover over the sleeping body and back onto the mattress.
            const double AcrossBody=(X+180.)/35.;
            const double Shoulder=(Y-607.)/45.;
            const double Rise=Occupied ? (26.+22.*FMath::Exp(-Shoulder*Shoulder))*
                FMath::Exp(-FMath::Square(AcrossBody*AcrossBody))*
                FMath::SmoothStep(472.,488.,Y) : 0.;
            return At(X,Y,Z+Rise);
        };
        Cover[0].Add(36,28,Quilt);
        for (double V0 : {.145,.18,.925,.96})
            Cover[1].Add(36,1,[&](double U,double V){return Quilt(U,V0+V*.008)+FVector(0,0,.2);});
        for (int32 J=0; J<5; ++J)
        {
            for (int32 I=0; I<7; ++I)
            {
                const double CX=.15+I*.112+(J%2)*.035, CY=.265+J*.138;
                const auto Stamp=[&](double U,double V,double Scale,double Bias)
                {
                    const double Diamond=1.-FMath::Abs(2.*V-1.);
                    return Quilt(CX+(U-.5)*.057*Diamond*Scale,CY+(V-.5)*.064*Scale)+FVector(0,0,Bias);
                };
                Cover[1].Add(2,2,[&](double U,double V){return Stamp(U,V,1.,.24);});
                Cover[2].Add(2,2,[&](double U,double V){return Stamp(U,V,.48,.37);});
            }
        }
    }
    // In the south-facing windows the curtain folds face into the home, including the gathered waist.
    for (const double End : {-1.,1.})
    {
        Panels[Ivory].Add(30,24,[End](double U,double V)
        {
            const double Width=43.+12.*FMath::Square(2.*V-1.);
            return At(End*136.+(.5-U)*Width,15.+9.*FMath::Sin(U*PI*8.)+3.*FMath::Sin(V*PI),
                Upper+36.+231.*V-1.3*FMath::Cos(U*PI*8.)*(1.-V));
        });
    }
    // Weighted hems hang at individual lengths on a removable textile display rail.
    for (int32 I=0; I<5; ++I)
    {
        const double X=93.+I*44., Length=107.+(I*19)%37;
        auto Drape=[I,X,Length](double U,double V)
        {
            return At(X+(U-.5)*37.,147.+5.5*FMath::Sin(PI*(4.*U+.12*I))*(1.-.3*V),
                ApartmentGroundZ+157.-Length+Length*V+2.2*FMath::Cos(5.*U+I)*(1.-V));
        };
        const int32 Dye[]={Madder,Marigold,Indigo,Sage,Ivory};
        Panels[Dye[I]].Add(16,12,Drape);
        for (double Hem : {.04,.075,.13})
            Panels[I==4?Indigo:Ivory].Add(16,1,[&](double U,double V)
            {
                return Drape(U,Hem+V*.012)-FVector(0,.15,0);
            });
    }

    // A flat-woven cotton dhurrie breaks up the polished floor in the sitting area.
    auto Rug=[](double U,double V)
    {
        return At(-205.+U*270.,223.+V*208.,Upper+1.9+.12*FMath::Sin(U*14.+V*8.));
    };
    Panels[Sage].Add(12,12,Rug);
    for (double V0 : {.055,.083,.14,.81,.87,.91})
        Panels[Indigo].Add(10,1,[&](double U,double V){return Rug(U,V0+V*.019)+FVector(0,0,.1);});
    for (int32 I=0; I<7; ++I)
        for (int32 J=0; J<3; ++J)
            Panels[Ivory].Add(2,2,[&](double U,double V)
            {
                const double CX=.09+I*.135, CY=.29+J*.21;
                return Rug(CX+(U-.5)*.085*(1.-FMath::Abs(V*2.-1.)),CY+(V-.5)*.12)+FVector(0,0,.16);
            });
    for (int32 I=0; I<48; ++I)
        for (double Edge : {0.,1.})
            Panels[Ivory].Add(1,1,[&](double U,double V)
            {
                return Rug((I+.18+U*.18)/48.,Edge+(Edge>.5?1.:-1.)*V*(.019+.005*(I%3)));
            });

    // Folds have a flattened curved return and a distinct overlocked selvedge.
    // Several patterns share the same sections and one draw per dye.
    auto Bolt=[&](int32 Dye,double X,double Y,double Z,double Width,double Depth,double Thick,bool Round)
    {
        const double E=Round?1.:.43;
        auto Section=[&](double U)
        {
            const double C=FMath::Cos(-2.*PI*U), S=FMath::Sin(-2.*PI*U);
            return FVector2D(FMath::Sign(C)*FMath::Pow(FMath::Abs(C),E)*Width*.5,
                FMath::Sign(S)*FMath::Pow(FMath::Abs(S),E)*Thick*.5);
        };
        auto Surface=[&](double U,double V)
        {
            const FVector2D P=Section(U);
            return At(X+P.X+.4*FMath::Sin(PI*V),Y+(V-.5)*Depth,Z+P.Y+.4*FMath::Sin(PI*V));
        };
        Panels[Dye].Add(24,3,Surface);
        for (double Face : {0.,1.})
            Panels[Dye].Add(24,1,[&](double U,double V)
            {
                const FVector2D P=Section(U)*(Face>.5?1.-V:V);
                return At(X+P.X,Y+(Face-.5)*Depth,Z+P.Y);
            });
        if (Round)
        {
            Panels[Marigold].Add(18,1,[&](double U,double V)
            {
                const double A=-2.*PI*U;
                return At(X+FMath::Cos(A)*2.1*V,Y-Depth*.5-.13,Z+FMath::Sin(A)*2.1*V);
            });
        }
        else
        {
            Panels[Dye==Ivory?Indigo:Ivory].Add(24,1,[&](double U,double V)
            {
                auto P=Surface(U,.09+V*.017);
                const auto Offset=Section(U).GetSafeNormal()*.12;
                return P+FVector(Offset.X,0,Offset.Y);
            });
        }
    };
    constexpr int32 Colors[]={Indigo,Sage,Madder,Ivory,Marigold};
    for (int32 L=0; L<3; ++L)
    {
        const double Shelf=ApartmentGroundZ+52.+L*56.;
        for (int32 Group=0; Group<2; ++Group)
        {
            const int32 CountInPile=2+(L+Group)%3;
            const double Width=Group?56.:72.;
            for (int32 Layer=0; Layer<CountInPile; ++Layer)
            {
                const int32 Dye=Layer<2 ? Colors[(L+Group*2)%5] : Colors[(L+Group*2+2)%5];
                Bolt(Dye,-268.+Group*91.+1.7*(Layer%3),686.+1.2*Layer,Shelf+6.+Layer*6.,
                    Width-Layer*2.7,46.-Layer,6.,false);
            }
        }
        const int32 Rolls=L==1?3:5;
        for (int32 R=0; R<Rolls; ++R)
        {
            const double Dia=17.+(R+L)%3*2.5;
            Bolt(Colors[(R/2+L*2)%5],160.+R*26.+(L%2)*4,679.+(R%2)*3.,
                Shelf+3.+Dia*.5,Dia,58.+R*1.7,Dia,true);
        }
    }
    // Hand-blocked sample displayed above the cutting table.
    auto WallCloth=[](double U,double V)
    {
        return At(-69.+126.*U,702.-2.9*FMath::Sin(PI*U*8.)-1.4*FMath::Cos(V*U*10.),
            ApartmentGroundZ+112.+148.*V+1.3*FMath::Cos(PI*U*6.)*(1.-V));
    };
    Panels[Madder].Add(28,18,WallCloth);
    for (double U0 : {.035,.08,.90,.95})
        Panels[Ivory].Add(1,12,[&](double U,double V)
        { return WallCloth(U0+U*.012,V)-FVector(0,.12,0); });
    for (int32 I=0; I<4; ++I)
        for (int32 J=0; J<5; ++J)
            Panels[(I+J)%3?Ivory:Marigold].Add(2,2,[&](double U,double V)
            {
                const double CX=.20+.2*I,CY=.14+.18*J;
                return WallCloth(CX+(U-.5)*.103*(1.-FMath::Abs(V*2.-1.)),CY+(V-.5)*.085)-FVector(0,.15,0);
            });
    auto* Fabric=NewObject<UProceduralMeshComponent>(&World);
    World.AddInstanceComponent(Fabric);
    Fabric->SetupAttachment(World.GetRootComponent());
    Fabric->SetMobility(EComponentMobility::Static);
    Fabric->SetCanEverAffectNavigation(false);
    Fabric->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Cloth=LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/M_ResidentCotton.M_ResidentCotton"));
    const FColor Dyes[]={FColor(31,52,68),FColor(181,163,129),FColor(114,48,38),
        FColor(147,103,45),FColor(67,84,73)};
    for (int32 I=0; I<Count; ++I)
    {
        auto& S=Panels[I];
        Fabric->CreateMeshSection_LinearColor(I,S.Vertices,S.Triangles,S.Normals,S.UV,{}, {},false);
        if (Cloth)
        {
            auto* M=UMaterialInstanceDynamic::Create(Cloth,Fabric);
            M->SetVectorParameterValue(TEXT("BaseColorFactor"),FLinearColor(Dyes[I]));
            M->SetScalarParameterValue(TEXT("RoughnessFactor"),.88f);
            Fabric->SetMaterial(I,M);
        }
    }
    Fabric->ComponentTags.Add(TEXT("BedTextiles"));
    for (int32 Occupied=0;Occupied<=1;++Occupied)
        for (int32 Colour=0;Colour<3;++Colour)
        {
            const int32 Section=Count+Occupied*3+Colour;
            const auto& S=Covers[Occupied][Colour];
            Fabric->CreateMeshSection_LinearColor(Section,S.Vertices,S.Triangles,S.Normals,S.UV,{}, {},false);
            Fabric->SetMaterial(Section,Fabric->GetMaterial(Colour));
            Fabric->SetMeshSectionVisible(Section,Occupied==0);
        }
    Fabric->RegisterComponent();
}
}

void FRiverfrontFirstMorningWorld::CoverSleeper(ARiverfrontWorld& World,bool Occupied)
{
    for (auto* Fabric:TInlineComponentArray<UProceduralMeshComponent*>(&World))
        if (Fabric->ComponentHasTag(TEXT("BedTextiles")))
        {
            const int32 FirstCover=Fabric->GetNumSections()-6;
            for (int32 I=0;I<3;++I)
            {
                Fabric->SetMeshSectionVisible(FirstCover+I,!Occupied);
                Fabric->SetMeshSectionVisible(FirstCover+3+I,Occupied);
            }
            return;
        }
}

FVector FRiverfrontFirstMorningWorld::HomeStart() { return At(-40,465,Upper+95); }
FVector FRiverfrontFirstMorningWorld::Bedside() { return At(-94,580,Upper+95); }
FVector FRiverfrontFirstMorningWorld::Pillow() { return At(-180,648,Upper+72); }
FVector FRiverfrontFirstMorningWorld::WardrobePoint() { return At(135,585,Upper+95); }
FVector FRiverfrontFirstMorningWorld::ShopPoint() { return At(-100,467,ApartmentGroundZ+95); }
FVector FRiverfrontFirstMorningWorld::StreetPoint() { return At(-83,-179,ApartmentGroundZ+95); }
FVector FRiverfrontFirstMorningWorld::FenceApproach() { return At(FenceX,FenceY-97.,99.); }
FVector FRiverfrontFirstMorningWorld::WallApproach() { return At(WallX,WallY-205.,99.); }

#if WITH_EDITOR
FBox FRiverfrontFirstMorningWorld::FenceTraversalBounds()
{
    return FBox(At(FenceX - FenceWidth / 2., FenceY - FenceDepth / 2., ApartmentGroundZ),
        At(FenceX + FenceWidth / 2., FenceY + FenceDepth / 2., FenceRailZ + FenceRailHeight / 2.));
}
#endif

FTransform FRiverfrontFirstMorningWorld::WakeStartView(bool bShowSleeper)
{
    if (!bShowSleeper)
    {
        const FVector Eye=At(-202,643,Upper+82);
        return FTransform((At(14,4,Upper+141)-Eye).Rotation(),Eye);
    }
    const FVector Eye=At(-31,529,Upper+170);
    return FTransform((At(-180,596,Upper+66)-Eye).Rotation(),Eye);
}
FTransform FRiverfrontFirstMorningWorld::WakeEndView(bool bShowSleeper)
{
    if (!bShowSleeper)
    {
        const FVector Eye=At(-150,608,Upper+127);
        return FTransform((At(25,12,Upper+143)-Eye).Rotation(),Eye);
    }
    const FVector Eye=At(-60,554,Upper+145);
    return FTransform((At(-180,613,Upper+68)-Eye).Rotation(),Eye);
}

TArray<FVector> FRiverfrontFirstMorningWorld::ExitRoute()
{
    return {HomeStart(),WardrobePoint(),At(305,638,Upper+95),At(455,638,Upper+95),
        At(455,Landing-15.,Upper+95),At(455,FirstRiser+9.5*Tread,ApartmentGroundZ+10*Rise+95),
        At(455,FirstRiser-60.,ApartmentGroundZ+95),At(455,-179,ApartmentGroundZ+95),StreetPoint(),
        At(-83,95,ApartmentGroundZ+95),ShopPoint()};
}

void FRiverfrontFirstMorningWorld::Build(ARiverfrontWorld& World)
{
    using ETint=ARiverfrontWorld::ETint;
    using EShape=ARiverfrontWorld::EShape;
    const auto Part=[&](ETint T,double X,double Y,double Z,double W,double D,double H,bool Solid=false,
        FRotator R=FRotator::ZeroRotator)
    {
        World.Box(T,At(X,Y,Z),FVector(W,D,H),Solid,R);
    };
    const auto Shape=[&](EShape S,ETint T,double X,double Y,double Z,double W,double D,double H)
    {
        World.Shape(S,T,At(X,Y,Z),FVector(W,D,H));
    };
    const auto Member=[&](ETint T,FVector A,FVector B,double Width)
    {
        World.Beam(T,At(A.X,A.Y,A.Z),At(B.X,B.Y,B.Z),Width);
    };
    const auto Mirror=[&](double X,double Y,double Z,double W,double H)
    {
        Part(ETint::Timber,X,Y,Z,W+8,5,H+8);
        auto* Glass=NewObject<UStaticMeshComponent>(&World);
        World.AddInstanceComponent(Glass);
        Glass->SetupAttachment(World.GetRootComponent());
        Glass->SetMobility(EComponentMobility::Static);
        Glass->SetStaticMesh(World.BoxMesh);
        if (auto* M=LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Sabarmati/Riverfront/Vehicles/SM_SabarmatiHatch/Materials/mirror_glass.mirror_glass")))
            Glass->SetMaterial(0,M);
        Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Glass->SetCanEverAffectNavigation(false);
        Glass->SetRelativeLocation(At(X,Y-3.4,Z));
        Glass->SetRelativeScale3D(FVector(W,1.3,H)/100.);
        Glass->RegisterComponent();
    };

    Part(ETint::Retaining,70,330,-2,925,880,12,true);
    Part(ETint::Paving,60,-129,1,925,166,6,true);

    // An open courtyard beside Kesar. The thin iron fence crosses to the same
    // paving, while the solid stone edge leads onto a raised dispatch landing.
    Part(ETint::Rail,FenceX,FenceY,FenceRailZ,FenceWidth,FenceDepth,FenceRailHeight,true);
    Part(ETint::Rail,FenceX,FenceY,19.,FenceWidth,7.,6.,true);
    for (int32 Bar = -7; Bar <= 7; ++Bar)
        Part(ETint::Rail,FenceX+Bar*21.,FenceY,54.,4.,5.,68.,true);
    for (const double Side : {-1.,1.})
        Part(ETint::Retaining,FenceX+Side*163.,FenceY,53.,18.,22.,106.,true);
    Part(ETint::FacadeStone,WallX,WallY,66.,382.,224.,132.,true);
    Part(ETint::PaleStone,WallX,WallY,132.5,388.,228.,1.);
    for (const double Row : {33.,65.,97.})
        Part(ETint::Shadow,WallX,WallY-112.2,Row,381.,.4,1.);
    for (int32 Step = 0; Step < 8; ++Step)
    {
        const double Height = 16.5*(8-Step);
        Part(ETint::FacadeStone,WallX+203.+Step*26.,WallY,Height*.5,26.,94.,Height,true);
    }

    // A continuous scanned stone finish with narrow divider joints, including at the bedroom threshold.
    for (double Base : {ApartmentGroundZ,Upper})
    {
        Part(ETint::Stone,0,366,Base+.35,721,717,.7);
        for (double X=-300.; X<=300.; X+=60.)
            Part(ETint::WoodDark,X,366,Base+.82,.48,717,.13);
        for (double Y=66.; Y<724.; Y+=60.)
            Part(ETint::WoodDark,0,Y,Base+.83,721,.48,.13);
    }
    Part(ETint::Plaster,0,370,Upper-10,740,740,20,true);
    Part(ETint::Plaster,0,370,Upper+320,764,760,23,true);
    Part(ETint::Terracotta,0,736,164,741,20,328,true);
    Part(ETint::Plaster,0,736,Upper+151,741,20,302,true);
    // East upstairs door opens directly onto the top landing. No full-height collision behind the opening.
    Part(ETint::Terracotta,364,370,166,18,741,332,true);
    Part(ETint::Plaster,364,262,Upper+152,18,524,304,true);
    Part(ETint::Plaster,364,723,Upper+152,18,24,304,true);
    Part(ETint::Plaster,364,618,Upper+271,18,188,66,true);
    Part(ETint::Timber,353,531,Upper+115,12,14,230);
    Part(ETint::Timber,353,704,Upper+115,12,14,230);
    Part(ETint::Timber,353,618,Upper+234,12,185,10);
    Part(ETint::Timber,306,704,Upper+115,96,7,229,true);
    Part(ETint::Brass,269,697,Upper+104,11,4,5);

    // West return with a high kitchen window. The bedroom has a full rear wall for the headboard.
    Part(ETint::Terracotta,-364,370,163,18,740,326,true);
    Part(ETint::Plaster,-364,451,Upper+154,18,532,308,true);
    Part(ETint::Plaster,-364,93,Upper+62,18,186,124,true);
    Part(ETint::Plaster,-364,93,Upper+279,18,186,58,true);
    Part(ETint::ClearGlass,-364,93,Upper+185,4,178,123,true);
    for (double Y : {19.,94.,166.})
        Part(ETint::Timber,-364,Y,Upper+186,9,5,125);

    // Glazed ground-floor shop with an open entrance under the painted fascia.
    for (const FVector2D Span : {FVector2D(-360,-202),FVector2D(48,360)})
    {
        const double Mid=(Span.X+Span.Y)*.5, Width=Span.Y-Span.X;
        Part(ETint::FacadeStone,Mid,0,27,Width,18,53,true);
        Part(ETint::ClearGlass,Mid,0,146,Width-8,3,185,true);
        Part(ETint::Timber,Mid,0,241,Width,12,8,true);
        for (double X=Span.X+3; X<Span.Y; X+=Width*.5)
            Part(ETint::Timber,X,-2,150,7,10,197,true);
    }
    Part(ETint::Timber,-198,43,123,8,85,238,true);
    Part(ETint::Brass,-204,72,105,6,23,4);
    Part(ETint::Plaster,0,0,282,738,20,74,true);
    Part(ETint::Ad,0,-13,279,653,9,58);
    Part(ETint::Brass,0,-19,254,645,2,2.2);
    Lettering(World,TEXT("S U T R A   C L O T H   H O U S E"),At(0,-19,278),-90,24,FColor(240,222,178));
    Part(ETint::Canvas,0,-55,259,705,100,5,false,FRotator(0,0,-6.));
    Part(ETint::Canvas,0,-103,247,705,5,17);
    for (double X : {-310.,0.,310.})
        Member(ETint::Rail,{X,-12,200},{X,-99,254},3.4);

    // Timber window reveals and a usable south-facing balcony, with a full-width room behind.
    for (const FVector2D Span : {FVector2D(-363,-98),FVector2D(98,363)})
    {
        const double X=(Span.X+Span.Y)*.5, W=Span.Y-Span.X;
        Part(ETint::Plaster,X,0,Upper+33,W,19,66,true);
        Part(ETint::ClearGlass,X,0,Upper+153,W-8,4,174,true);
        Part(ETint::Timber,X,0,Upper+66,W,13,6,true);
        Part(ETint::Timber,X,0,Upper+241,W,13,7,true);
        for (double Post : {Span.X+5,X,Span.Y-5})
            Part(ETint::Timber,Post,0,Upper+152,7,14,181,true);
    }
    Part(ETint::Plaster,0,0,Upper+281,740,20,70,true);
    Part(ETint::PaleStone,0,-62,Upper-8,740,128,16,true);
    for (double X : {-359.,359.})
        Part(ETint::Rail,X,-65,Upper+51,7,126,102,true);
    Part(ETint::Rail,0,-122,Upper+101,727,8,7,true);
    Part(ETint::Rail,0,-122,Upper+25,725,7,4,true);
    for (int32 I=0; I<=30; ++I)
        Part(ETint::Rail,-358+I*(716./30.),-122,Upper+53,3,5,99,true);

    // A 1.7 m clear common stair. Individual <17 cm treads are traversable by UE CharacterMovement.
    Part(ETint::PaleStone,455,(Landing+741.)*.5,Upper-8,176,741.-Landing,16,true);
    for (int32 I=0; I<Steps; ++I)
    {
        const double Y=FirstRiser+(I+.5)*Tread, Top=ApartmentGroundZ+(I+1)*Rise;
        Part(ETint::Retaining,455,Y,(ApartmentGroundZ+Top)*.5,176,Tread,Top-ApartmentGroundZ,true);
        Part(ETint::PaleStone,455,Y-2.5,Top-.8,178,Tread+5,1.6);
        if (!(I%3))
            Part(ETint::Rail,537,Y,Top+49,4.5,6,98,true);
    }
    Member(ETint::Timber,{537,FirstRiser+3,ApartmentGroundZ+Rise+98},{537,Landing,Upper+98},6);
    Part(ETint::Rail,537,(Landing+741.)*.5,Upper+48,5,741.-Landing,96,true);
    Part(ETint::Timber,537,(Landing+741.)*.5,Upper+100,8,741.-Landing,6);
    Part(ETint::Rail,455,741,Upper+50,176,6,100,true);
    Part(ETint::Plaster,456,440,Upper+314,197,609,14);
    Part(ETint::Timber,536,720,Upper+154,9,9,308,true);
    Part(ETint::Rail,454,738,Upper+185,145,6,34);
    Lettering(World,TEXT("KESAR   /   2B"),At(454,734,Upper+185),-90,14,FColor(239,219,174));

    // The roof reads as a working terrace from the street and balcony. The
    // parapet has a coping and shadow joint, with a set-back water tank plinth.
    for (const double Y : {0., 740.})
    {
        Part(ETint::FacadeStone,0,Y,Upper+362,762,18,73);
        Part(ETint::PaleStone,0,Y,Upper+402,773,27,7);
    }
    for (const double X : {-373., 373.})
    {
        Part(ETint::FacadeStone,X,370,Upper+362,18,740,73);
        Part(ETint::PaleStone,X,370,Upper+402,27,744,7);
    }
    Part(ETint::Retaining,-174,571,Upper+350,183,171,32);
    Shape(EShape::Cylinder,ETint::Shadow,-174,571,Upper+425,139,139,118);
    Shape(EShape::Cylinder,ETint::Rail,-174,571,Upper+485,105,105,13);
    for (double Z : {Upper+391, Upper+434, Upper+462})
        Shape(EShape::Cylinder,ETint::Shadow,-174,571,Z,143,143,5);
    Member(ETint::Rail,{-106,571,Upper+391},{-106,742,Upper+391},5.8);
    Member(ETint::Rail,{-106,742,Upper+391},{-106,753,32},5.8);

    // Perforated terracotta beside the upper landing leaves the stair width
    // and the handrail clear. The open cells cast individual shadows.
    Part(ETint::FacadeStone,548,624,Upper+19,20,225,11);
    Part(ETint::FacadeStone,548,624,Upper+289,20,225,11);
    for (int32 Column=0; Column<=9; ++Column)
        Part(ETint::Terracotta,548,514+Column*24.4,Upper+154,14,8,265);
    for (int32 Row=0; Row<=9; ++Row)
        Part(ETint::Terracotta,548,624,Upper+24+Row*28.8,14,220,8);
    for (const double X : {-332., 332.})
    {
        Member(ETint::Rail,{X,-17,Upper+317},{X,-17,Upper+115},7);
        Member(ETint::Rail,{X,-17,Upper+115},{X,-139,Upper+20},7);
        Member(ETint::Rail,{X,-139,Upper+20},{X,-139,28},7);
        for (double Z : {78., 192., Upper+183.})
            Part(ETint::Rail,X,-13,Z,14,11,4);
    }
    // A separate stair nosing and anti-slip strip catch light at every tread.
    for (int32 I=0; I<Steps; ++I)
    {
        const double Y=FirstRiser+I*Tread;
        const double Top=ApartmentGroundZ+(I+1)*Rise;
        Part(ETint::FacadeStone,455,Y+1.5,Top-.5,172,4,1.5);
        Part(ETint::Shadow,455,Y+6,Top+.08,164,2.4,.25);
    }

    // The bed, a caned headboard and an indigo block-print quilt sit at the back of the apartment.
    Part(ETint::Timber,-218,585,Upper+29,167,218,18,true);
    for (double X : {-288.,-148.})
        for (double Y : {494.,679.})
            Part(ETint::Timber,X,Y,Upper+13,9,9,26);
    Part(ETint::Paint,-218,585,Upper+46,161,211,18,true);
    Part(ETint::Timber,-218,693,Upper+57,174,11,105,true);
    Part(ETint::Canvas,-218,686,Upper+80,156,3,42);
    for (int32 I=0; I<16; ++I)
        Part(ETint::Sandstone,-289+I*9.4,683.5,Upper+80,2.4,2,38);
    for (double X : {-258.,-180.})
        Shape(EShape::Sphere,ETint::Paint,X,656,Upper+59,68,45,17);
    Part(ETint::Timber,-333,603,Upper+48,41,49,5,true);
    for (double X : {-347.,-319.})
        Part(ETint::Timber,X,603,Upper+23,5,38,46);
    Shape(EShape::Cylinder,ETint::Brass,-333,604,Upper+53,20,20,3);
    Shape(EShape::Cylinder,ETint::Brass,-333,604,Upper+66,3,3,27);
    Shape(EShape::Cylinder,ETint::Canvas,-333,604,Upper+84,26,26,20);

    // Wardrobe faces the bed aisle; the mirror and brass door pulls identify the interaction from play height.
    Part(ETint::Timber,135,689,Upper+109,203,64,218,true);
    for (double X : {76.,129.,190.})
    {
        Part(ETint::WoodDark,X,655,Upper+111,3,3,204);
        Part(ETint::Brass,X+8,650,Upper+110,3,6,22);
    }
    Mirror(174,651,Upper+115,69,175);

    // The pivoting mirror lamp shines across the dressing spot from above eye level.
    const FRotator LampAim(0,-143,0);
    const FVector LampCentre=At(216,643,Upper+175);
    const FVector LampForward=LampAim.Vector();
    Part(ETint::Brass,216,651,Upper+175,7,11,9);
    World.Box(ETint::Brass,LampCentre,FVector(4,7,53),false,LampAim);
    auto* Diffuser=NewObject<UStaticMeshComponent>(&World,TEXT("WardrobeLampDiffuser"));
    World.AddInstanceComponent(Diffuser);
    Diffuser->SetupAttachment(World.GetRootComponent());
    Diffuser->SetMobility(EComponentMobility::Static);
    Diffuser->SetStaticMesh(World.BoxMesh);
    Diffuser->SetRelativeScale3D(FVector(.010,.054,.49));
    Diffuser->SetRelativeLocationAndRotation(LampCentre+LampForward*2.4,LampAim);
    Diffuser->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Diffuser->SetCanEverAffectNavigation(false);
    Diffuser->SetCastShadow(false);
    if (auto* Lens=LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Sabarmati/Riverfront/M_PracticalLens.M_PracticalLens")))
    {
        auto* Frosted=UMaterialInstanceDynamic::Create(Lens,Diffuser);
        Frosted->SetVectorParameterValue(TEXT("LensTint"),FLinearColor::MakeFromColorTemperature(5000.f));
        Frosted->SetScalarParameterValue(TEXT("Glow"),1.6f);
        Diffuser->SetMaterial(0,Frosted);
    }
    Diffuser->RegisterComponent();

    auto* MirrorLight=NewObject<URectLightComponent>(&World,TEXT("WardrobeMirrorLight"));
    World.AddInstanceComponent(MirrorLight);
    MirrorLight->SetupAttachment(World.GetRootComponent());
    MirrorLight->SetMobility(EComponentMobility::Movable);
    MirrorLight->SetRelativeLocationAndRotation(LampCentre+LampForward*3.2,LampAim);
    MirrorLight->SetSourceWidth(5.4f);
    MirrorLight->SetSourceHeight(49.f);
    MirrorLight->SetUseTemperature(true);
    MirrorLight->SetTemperature(5000.f);
    MirrorLight->SetIntensityUnits(ELightUnits::Lumens);
    // The prototype uses a fixed exposure and a 5-lux morning sun.
    MirrorLight->SetIntensity(.75f);
    MirrorLight->SetAttenuationRadius(220.f);
    MirrorLight->MaxDrawDistance=900.f;
    MirrorLight->MaxDistanceFadeRange=180.f;
    MirrorLight->RegisterComponent();

    Part(ETint::Timber,135,688,Upper+222,212,73,8);
    for (int32 I=0; I<7; ++I)
        Part(ETint::Timber,83+I*8,651,Upper+141,2.4,3,133);

    // Kitchen platform, tiffin carrier, kettle, sink and a two-chair breakfast table.
    Part(ETint::Timber,-322,167,Upper+44,72,215,88,true);
    Part(ETint::FacadeStone,-321,167,Upper+90,78,222,7);
    Part(ETint::Rail,-323,111,Upper+94,54,51,2);
    Shape(EShape::Cylinder,ETint::Brass,-321,123,Upper+104,22,22,18);
    Shape(EShape::Sphere,ETint::Brass,-321,123,Upper+115,19,19,6);
    for (int32 I=0; I<3; ++I)
        Shape(EShape::Cylinder,ETint::PaleStone,-322,171,Upper+98+I*6.2,18,18,5.6);
    Part(ETint::Rail,-325,218,Upper+94,46,48,2);
    Part(ETint::Brass,-348,239,Upper+107,3,3,26);
    Part(ETint::Brass,-342,239,Upper+120,15,3,3);
    for (double Side : {-1.,1.})
    {
        const double X=73.+Side*49.;
        for (double Y : {156.,226.})
            Part(ETint::Timber,X,Y,Upper+36,5,5,72);
        const double ChairX=73.+Side*89.;
        Part(ETint::Timber,ChairX,191,Upper+45,43,45,5,true);
        for (double Shift : {-1.,1.})
        {
            Part(ETint::Timber,ChairX+Side*18,191+Shift*18,Upper+42,3,3,84);
            Part(ETint::Timber,ChairX-Side*18,191+Shift*18,Upper+21,3,3,42);
        }
        Part(ETint::Timber,ChairX+Side*18,191,Upper+76,5,42,14);
    }
    Part(ETint::Timber,73,191,Upper+75,125,88,6,true);
    for (double X : {48.,93.})
    {
        Shape(EShape::Cylinder,ETint::Ceramic,X,190,Upper+79,26,26,2);
        Shape(EShape::Cylinder,ETint::Clay,X,198,Upper+86,7.5,7.5,10);
    }
    Shape(EShape::Cylinder,ETint::Brass,-52,310,Upper+283,4,4,34);
    Shape(EShape::Sphere,ETint::Timber,-52,310,Upper+263,24,24,14);
    for (double Angle : {15.,135.,255.})
    {
        const FRotator Heading(0,Angle,0);
        const FVector Center=FVector(-52,310,Upper+265)+Heading.RotateVector(FVector(39,0,0));
        Part(ETint::Timber,Center.X,Center.Y,Center.Z,65,13,2,false,Heading);
    }
    for (double Y : {328.,542.})
    {
        Part(ETint::Brass,-347,Y,Upper+202,16,22,7);
        Shape(EShape::Cylinder,ETint::Paint,-340,Y,Upper+217,16,16,23);
    }

    // Skirting and picture rail close the upper room corners. A red oxide inset grounds the door.
    for (const double Z : {Upper+6.,Upper+307.})
    {
        for (const double X : {-353.,353.})
        {
            Part(ETint::Timber,X,258,Z,7,517,9);
            if (X<0. || Z>Upper+100.) Part(ETint::Timber,X,620,Z,7,209,9);
        }
        Part(ETint::Timber,0,723,Z,712,8,10);
        if (Z>Upper+100.) Part(ETint::Timber,0,8,Z,710,8,9);
    }

    // Original framed ink print of a river truss, visible from the bed towards the breakfast table.
    Part(ETint::Timber,352,282,Upper+158,6,167,107);
    Part(ETint::Paint,347.8,282,Upper+158,3,157,97);
    Part(ETint::Sandstone,345.8,282,Upper+158,1,132,68);
    Part(ETint::Ad,345.,282,Upper+142,.7,132,23);
    const double PrintX=344.2;
    for (int32 I=0; I<5; ++I)
    {
        const double Y=218.+I*26.;
        Member(ETint::Roof,{PrintX,Y,Upper+152},{PrintX,Y+13.,Upper+169.},1.7);
        Member(ETint::Roof,{PrintX,Y+13.,Upper+169.},{PrintX,Y+26.,Upper+152.},1.7);
    }
    Member(ETint::Roof,{PrintX,218,Upper+152},{PrintX,348,Upper+152},2.);
    Part(ETint::Timber,317,271,Upper+47,62,142,6,true);
    for (double Y : {213.,329.})
        Part(ETint::Timber,317,Y,Upper+22,5,6,43);
    Part(ETint::Roof,317,292,Upper+53,30,40,4,false,FRotator(0,7,0));
    Part(ETint::Clay,318,289,Upper+57,27,37,4,false,FRotator(0,-4,0));
    Shape(EShape::Cylinder,ETint::Clay,316,230,Upper+64,21,21,28);
    for (int32 I=0; I<9; ++I)
    {
        const double A=I*2.39996;
        const FVector Tip(316.+FMath::Cos(A)*16.,230.+FMath::Sin(A)*14.,Upper+82.+(I%4)*6.);
        Member(ETint::FoliageDark,{316,230,Upper+77},Tip,1.1);
        World.Shape(EShape::Plane,ETint::Leaf,At(Tip.X,Tip.Y,Tip.Z),FVector(15,26,100),false,
            FRotator(-34.+I%3*17.,A*180./PI,13.));
    }
    InteriorLight(World,At(-298,523,Upper+196),56,420,3400);
    InteriorLight(World,At(72,217,Upper+235),35,370,3800);
    World.AddPractical(At(58,344,Upper+291),-90);

    // Seen from the pavement: folded bolts on shelves, fabric on a brass rail, and a clear aisle to checkout.
    Part(ETint::Timber,-100,590,ApartmentGroundZ+48,231,70,96,true);
    Part(ETint::FacadeStone,-100,590,ApartmentGroundZ+97,240,79,7);
    for (int32 I=0; I<15; ++I)
        Part(ETint::Timber,-207+I*15.2,553,ApartmentGroundZ+49,7,4,81);
    Part(ETint::Rail,-45,592,ApartmentGroundZ+102,27,23,3);
    Part(ETint::Brass,-160,593,ApartmentGroundZ+103,32,20,3);
    const ETint Weave[]={ETint::Ad,ETint::Canvas,ETint::Clay,ETint::Sail,ETint::Paint};

    // Flanking timber shelving defines separate bays. The central textile hangs free behind the counter.
    Part(ETint::Plaster,0,719,ApartmentGroundZ+169,223,12,273);
    Part(ETint::Timber,-7,705,ApartmentGroundZ+263,153,9,5);
    for (double X : {-80.,66.})
        Part(ETint::Brass,X,710,ApartmentGroundZ+264,4,19,12);
    for (const FVector2D Bay : {FVector2D(-227.,190.),FVector2D(208.,183.)})
    {
        Part(ETint::Timber,Bay.X,697,ApartmentGroundZ+22,Bay.Y+10,60,44);
        for (double Side : {-1.,1.})
            Part(ETint::WoodDark,Bay.X+Side*(Bay.Y*.5),699,ApartmentGroundZ+119,6,49,205);
        for (int32 L=0; L<3; ++L)
        {
            const double Z=ApartmentGroundZ+52.+L*56.;
            Part(ETint::Timber,Bay.X,693,Z,Bay.Y,58,6);
            Part(ETint::WoodDark,Bay.X,662,Z,Bay.Y+4,4,6);
        }
    }
    // Limewashed back to the cutting area over a timber dado.
    Part(ETint::Plaster,353,420,ApartmentGroundZ+206,5,608,224);
    Part(ETint::Timber,350,420,ApartmentGroundZ+98,7,607,6);
    Part(ETint::WoodDark,-113,575,ApartmentGroundZ+102,78,4,2);
    for (int32 I=0; I<16; ++I)
        Part(ETint::Brass,-149+I*4.7,575,ApartmentGroundZ+103.2,.6,(I%5==0)?3.:1.5,.5);
    Part(ETint::Roof,-46,589,ApartmentGroundZ+108,18,13,2,false,FRotator(-10,0,0));
    for (double X : {67.,294.})
    {
        Shape(EShape::Cylinder,ETint::Brass,X,154,ApartmentGroundZ+80,4,4,160);
        Shape(EShape::Cylinder,ETint::Timber,X,154,ApartmentGroundZ+3,23,23,6);
    }
    Part(ETint::Brass,180,154,ApartmentGroundZ+158,229,5,5);
    Part(ETint::Timber,-330,200,ApartmentGroundZ+65,48,157,9,true);
    for (double Y : {143.,246.})
        Part(ETint::Rail,-330,Y,ApartmentGroundZ+32,5,7,64);
    for (int32 Roll=0; Roll<5; ++Roll)
        World.Shape(EShape::Cylinder,Weave[Roll],At(-329,147+24*Roll,ApartmentGroundZ+80),
            FVector(20,20,58),false,FRotator(0,0,90));
    Mirror(284,718,ApartmentGroundZ+146,76,173);
    InteriorLight(World,At(-93,500,222),67,545,3800);
    World.AddPractical(At(-100,579,282),-90);

    FitTextiles(World);
    UE_LOG(LogTemp,Display,TEXT("Sabarmati morning: built Kesar Apartments 2B, twenty stair treads and Sutra Cloth House."));
}
