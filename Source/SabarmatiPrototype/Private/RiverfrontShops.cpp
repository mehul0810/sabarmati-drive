#include "RiverfrontWorld.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
UProceduralMeshComponent* DisplayMesh(ARiverfrontWorld& Owner, UMaterialInterface* Material,
    const TArray<FVector>& Points, const TArray<int32>& Indices, const TArray<FVector2D>& UV,
    const TArray<FVector>& Normals)
{
    auto* Mesh = NewObject<UProceduralMeshComponent>(&Owner);
    Owner.AddInstanceComponent(Mesh);
    Mesh->SetupAttachment(Owner.GetRootComponent());
    Mesh->SetMobility(EComponentMobility::Static);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCanEverAffectNavigation(false);
    Mesh->CreateMeshSection_LinearColor(0, Points, Indices, Normals, UV, {}, {}, false);
    Mesh->SetMaterial(0, Material);
    Mesh->RegisterComponent();
    return Mesh;
}
}

void ARiverfrontWorld::FitShop(const FVector& Opening, const FRotator& Facing,
    double Width, double Depth, double Ceiling, bool IsTea)
{
    const auto At = [&](double X,double Y,double Z) { return Opening + Facing.RotateVector(FVector(X,Y,Z)); };
    const auto Part = [&](ETint Tint, double X,double Y,double Z,double W,double D,double H,bool Solid=false)
    {
        Box(Tint, At(X,Y,Z), FVector(W,D,H), Solid, Facing);
    };
    const double Half = Width*.5;
    // White tiled splashback, moulded dado, perforated spice lids and a menu board
    // break up the far wall where it catches light through the open frontage.
    if (IsTea)
    {
        for (int32 Row=0;Row<4;++Row)
            for (int32 Col=0;Col<16;++Col)
            {
                const double TileW=(Width-38.)/16.;
                Part(ETint::Ceramic,-Half+19+(Col+.5)*TileW,Depth-4,18+Row*24,TileW-1.1,3,23);
            }
    }
    // A real clear aisle extends from the shop entrance to the counter.
    const double CounterY = Depth - 145;
    const double CounterX = -Width*.15;
    const double CounterW = FMath::Min(386., Width*.53);
    Part(ETint::Ceramic, 0, Depth*.5, 1.3, Width-32, Depth, 2.6);
    Part(ETint::Timber, CounterX, CounterY, 47, CounterW, 71, 94, true);
    Part(ETint::FacadeStone, CounterX, CounterY, 95, CounterW+9, 81, 7);
    for (double X=CounterX-CounterW*.45; X<CounterX+CounterW*.46; X+=13)
    {
        Part(ETint::Timber, X, CounterY-37, 49, 7, 5, 73);
    }
    for (int32 Level=0; Level<3; ++Level)
    {
        const double Z=111 + Level*44;
        Part(ETint::Timber, 0, Depth-30, Z, Width-76, 46, 5);
        for (double X : {-Half*.69, 0., Half*.69})
        {
            Part(ETint::Brass, X, Depth-28, Z-9, 4, 37, 17);
        }
        for (int32 I=0; I<(IsTea ? 12 : 24); ++I)
        {
            double X=FMath::Lerp(-Half+59, Half-58, (I+.5)/(IsTea?12.:24.));
            const double H=12 + (I*17+Level*13)%18;
            if (IsTea)
            {
                Shape(EShape::Cylinder, (I+Level)%3==0 ? ETint::Brass : ETint::Ceramic,
                    At(X,Depth-32,Z+3+H*.5),FVector(14+(I%3)*2,14+(I%3)*2,H),false,Facing);
                Shape(EShape::Cylinder,ETint::Timber,At(X,Depth-32,Z+H+4),FVector(16,16,3),false,Facing);
            }
            else
            {
                const ETint Cover[] = {ETint::Ad,ETint::Clay,ETint::WoodLight,ETint::Shadow,ETint::Sandstone};
                const double W = 7 + I%4;
                Part(Cover[(I+Level)%5],X,Depth-39,Z+3+H*.5,W,21,H);
                Part(ETint::PaleStone,X,Depth-50,Z+7+H*.55,W*.65,1,1.6);
            }
        }
    }
    const double SignW = FMath::Min(Width-38, 668.);
    const double SignH = SignW*288./1536.;
    const double SignY = -249;
    const double SignZ = Ceiling+6;
    for (double Side : {-1.,1.})
    {
        Beam(ETint::Rail,At(Side*SignW*.40,-158,Ceiling+10),At(Side*SignW*.40,SignY,SignZ),4.5);
    }
    Part(ETint::Timber,0,SignY,SignZ,SignW+14,13,SignH+12);
    const TCHAR* Asset = IsTea ? TEXT("/Game/Sabarmati/Riverfront/M_NadiChaiSign.M_NadiChaiSign")
        : TEXT("/Game/Sabarmati/Riverfront/M_KagazPressSign.M_KagazPressSign");
    if (auto* Mat=LoadObject<UMaterialInterface>(nullptr,Asset))
    {
        const double Y=SignY-7.5;
        const TArray<FVector> V={At(-SignW*.5,Y,SignZ-SignH*.5),At(SignW*.5,Y,SignZ-SignH*.5),
            At(SignW*.5,Y,SignZ+SignH*.5),At(-SignW*.5,Y,SignZ+SignH*.5)};
        const FVector Normal=Facing.RotateVector(FVector(0,-1,0));
        DisplayMesh(*this,Mat,V,{0,2,1,0,3,2},{{1,1},{0,1},{0,0},{1,0}},{Normal,Normal,Normal,Normal});
    }
    // Physical rods, a rolled hem and a pitched fabric canopy cast shadows into the shop.
    const double CanopyW = FMath::Min(Width-46, 750.);
    const double FlatY = -121;
    for (double Side : {-1.,1.})
    {
        Beam(ETint::Brass,At(Side*CanopyW*.48,-8,Ceiling-79),At(Side*CanopyW*.48,-222,Ceiling-108),3);
        Beam(ETint::Rail,At(Side*CanopyW*.48,-220,Ceiling-108),At(Side*CanopyW*.48,-14,Ceiling-159),3);
    }
    Box(ETint::Canvas,At(0,FlatY,Ceiling-91),FVector(CanopyW,213,3),false,
        (Facing.Quaternion()*FRotator(0,0,-8.3).Quaternion()).Rotator());
    Part(ETint::Canvas,0,-223,Ceiling-118,CanopyW,4,18);
    if (!IsTea) PressCounter=At(CounterX,CounterY-90,96);
    // The lathed urn is one indexed mesh with a foot, curved shoulder, rim and lid.
    if (IsTea)
    {
        TeaCounter=At(CounterX,CounterY-90,96);
        const FVector2D Profile[]={{8,0},{10,1},{11,2.4},{8.8,4},{8.7,8},{12,10},{15,14},
            {16,19},{16,35},{15.6,38},{13,42},{9,44},{6.5,44.6},{6.5,45.6},{9.5,46.2},
            {8.8,47},{6,48},{3,48.5},{2,51},{3,52.5},{2.5,54},{.1,54.5}};
        TArray<FVector> Vertices,Normals;
        TArray<int32> Triangles;
        TArray<FVector2D> UV;
        constexpr int32 Sides=32;
        for (int32 Ring=0; Ring<UE_ARRAY_COUNT(Profile); ++Ring)
        {
            const FVector2D Tangent=Profile[FMath::Min(Ring+1,static_cast<int32>(UE_ARRAY_COUNT(Profile))-1)]-Profile[FMath::Max(Ring-1,0)];
            for (int32 S=0; S<=Sides; ++S)
            {
                const double A=2*PI*S/Sides, C=FMath::Cos(A), Sn=FMath::Sin(A);
                Vertices.Add(At(CounterX-CounterW*.25+C*Profile[Ring].X,CounterY+Sn*Profile[Ring].X,99+Profile[Ring].Y));
                Normals.Add(Facing.RotateVector(FVector(C*Tangent.Y,Sn*Tangent.Y,-Tangent.X).GetSafeNormal()));
                UV.Emplace(double(S)/Sides,1.-Profile[Ring].Y/55.);
                if (Ring && S)
                {
                    const int32 P=Vertices.Num()-1;
                    Triangles.Append({P-Sides-2,P-1,P,P-Sides-2,P,P-Sides-1});
                }
            }
        }
        DisplayMesh(*this,SurfaceMaterials.FindRef(int32(ETint::Brass)),Vertices,Triangles,UV,Normals);
        Part(ETint::Brass,CounterX-CounterW*.25,CounterY-19,117,3,13,3);
        for (int32 I=0; I<7; ++I)
        {
            const double X=CounterX+9+(I%4)*12, Y=CounterY-15+(I/4)*15;
            Shape(EShape::Cylinder,ETint::Ceramic,At(X,Y,104),FVector(7.4,7.4,9),false,Facing);
            Shape(EShape::Cylinder,ETint::WoodDark,At(X,Y,108.7),FVector(5.9,5.9,.5),false,Facing);
        }
    }
    const double TableX=Half-85;
    Part(ETint::Timber,TableX,Depth*.43,74,108,75,6,true);
    for (double Side : {-1.,1.})
    {
        for (double Edge : {-1.,1.})
        {
            Part(ETint::Rail,TableX+Side*43,Depth*.43+Edge*26,37,5,5,74);
        }
        const double Y=Depth*.43+Side*68;
        Part(ETint::Timber,TableX,Y,43,41,42,5,true);
        for (double Edge : {-1.,1.})
        {
            Part(ETint::Rail,TableX+Edge*16,Y+Side*17,40,3,3,80);
            Part(ETint::Rail,TableX+Edge*16,Y-Side*17,21,3,3,42);
        }
        Part(ETint::Timber,TableX,Y+Side*18,72,39,5,14);
    }
}

void ARiverfrontWorld::BuildWestBankGarden()
{
    auto* Tree=StreetTreeMesh();
    FRandomStream Garden(84519);
    // Low planters sit away from the clear pedestrian path, vehicle doors and Atal landing.
    for (const FVector& Base : {FVector(-6900,3200,3),FVector(-5920,3210,3),FVector(-2330,3340,3),
        FVector(-650,3290,3),FVector(1020,3220,3),FVector(4970,3220,3),FVector(6740,3270,3)})
    {
        Box(ETint::Retaining,Base+FVector(0,0,22),FVector(267,91,44),true);
        Box(ETint::FacadeStone,Base+FVector(0,0,45),FVector(273,96,7));
        Box(ETint::Bark,Base+FVector(0,0,49),FVector(252,77,5));
        for (int32 K=0; K<40; ++K)
        {
            const FVector Stem=Base+FVector(Garden.FRandRange(-118,118),Garden.FRandRange(-32,32),51);
            const FVector Tip=Stem+FVector(Garden.FRandRange(-16,16),Garden.FRandRange(-17,17),Garden.FRandRange(18,54));
            Beam(ETint::FoliageDark,Stem,Tip,1.4);
            Shape(EShape::Plane,ETint::Leaf,Tip,FVector(23,36,100),false,FRotator(Garden.FRandRange(-48,45),Garden.FRandRange(-180,180),29));
            if (K%4==0)
            {
                for (int32 P=0; P<3; ++P)
                {
                    Shape(EShape::Sphere,ETint::Flower,Tip+FVector(P*2-2,P%2*2,2),FVector(4,4,3));
                }
            }
        }
    }
    // Layered planting and distinct, stepped roof silhouettes on the existing far-bank frontage.
    for (int32 I=0; I<37; ++I)
    {
        const double X=-25200 + I*1390 + Garden.FRandRange(-285,285);
        const double Y=-26500 - Garden.FRandRange(1500,3300);
        if (Tree && I%2==0) PlaceStreetTree(*Tree,FVector(X,Y,3),Garden.FRandRange(620,970),Garden);
        const double W=Garden.FRandRange(620,1490), D=Garden.FRandRange(660,1250);
        const int32 Floors=Garden.RandRange(4,10);
        const double H=Floors*303.;
        const double Setback=Y-Garden.FRandRange(4400,6500);
        const ETint Skin=I%3==0 ? ETint::Terracotta : I%3==1 ? ETint::FacadeStone : ETint::Plaster;
        Box(Skin,FVector(X,Setback,H*.5),FVector(W,D,H),true);
        for (int32 F=1; F<=Floors; ++F)
        {
            const double Z=F*303.;
            Box(ETint::Retaining,FVector(X+W*.12,Setback+D*.5,Z-19),FVector(W*.72,114,19));
            for (int32 B=0; B<4; ++B)
            {
                const double BayX=X+(B-1.5)*W*.218;
                Box(ETint::Glass,FVector(BayX,Setback+D*.5+3,Z-113),FVector(W*(B%2?.13:.16),7,141));
            }
        }
        Box(Skin,FVector(X-W*.16,Setback-D*.09,H+129),FVector(W*.59,D*.69,258));
        Shape(EShape::Cylinder,ETint::Roof,FVector(X+W*.22,Setback-D*.16,H+65),FVector(98,98,130));
    }
}
