#include "RiverfrontHUD.h"

#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontFirstMorningWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontPlayerController.h"
#include "RiverfrontWorld.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "GlobalRenderResources.h"

namespace RiverfrontInstruments
{
const FLinearColor Ivory(FColor(246,241,232));
const FLinearColor Gold(FColor(243,187,114));
const FLinearColor Graphite(FColor(17,25,22));
const FLinearColor Silver(FColor(179,195,182));
const FLinearColor Water(FColor(43,91,101));
const FLinearColor Road(FColor(153,167,143));
const FLinearColor Warning(FColor(248,120,83));
const FLinearColor Home(FColor(173,224,197));

FVector2D Radial(double Degrees)
{
    const double Angle = FMath::DegreesToRadians(Degrees);
    return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
}

// Use one set of design units and one transient triangle batch for the glass, street lines and dials.
struct FInstruments
{
    UCanvas& Canvas;
    const float Scale;
    const float Width;
    const float Height;
    TArray<FCanvasUVTri>& Triangles;

    FInstruments(UCanvas& InCanvas,TArray<FCanvasUVTri>& Scratch)
        : Canvas(InCanvas)
        , Scale(FMath::Min(InCanvas.ClipY / 900.f, InCanvas.ClipX / 810.f))
        , Width(InCanvas.ClipX / Scale), Height(InCanvas.ClipY / Scale), Triangles(Scratch)
    {
        Triangles.Reset();
        Triangles.Reserve(600);
    }

    void Triangle(FVector2D A, FVector2D B, FVector2D C, FLinearColor First, FLinearColor Second, FLinearColor Third)
    {
        auto& T = Triangles.AddDefaulted_GetRef();
        T.V0_Pos = A * Scale; T.V1_Pos = B * Scale; T.V2_Pos = C * Scale;
        T.V0_UV = T.V1_UV = T.V2_UV = FVector2D::ZeroVector;
        T.V0_Color = First; T.V1_Color = Second; T.V2_Color = Third;
    }
    void Fill(FVector2D A, FVector2D B, FVector2D C, FLinearColor Color)
    {
        Triangle(A,B,C,Color,Color,Color);
    }
    void Quad(FVector2D A, FVector2D B, FVector2D C, FVector2D D, FLinearColor Color)
    {
        Fill(A,B,C,Color); Fill(A,C,D,Color);
    }
    void Stroke(FVector2D A, FVector2D B, float Weight, FLinearColor Color)
    {
        const FVector2D Dir = (B-A).GetSafeNormal();
        const FVector2D Edge(-Dir.Y * Weight * .5, Dir.X * Weight * .5);
        Quad(A-Edge, B-Edge, B+Edge, A+Edge, Color);
    }
    void Arc(FVector2D Centre, float Radius, double From, double To, float Weight, FLinearColor Color)
    {
        if (To <= From) return;
        const int32 Count = FMath::CeilToInt((To-From)/6.);
        FVector2D Last = Radial(From);
        for (int32 I=1; I<=Count; ++I)
        {
            const FVector2D Next = Radial(FMath::Lerp(From,To,double(I)/Count));
            Quad(Centre+Last*(Radius-Weight),Centre+Last*Radius,Centre+Next*Radius,Centre+Next*(Radius-Weight),Color);
            Last = Next;
        }
    }
    void Disc(FVector2D Centre, float Radius, FLinearColor Inside, FLinearColor Outside)
    {
        FVector2D Last = Centre + FVector2D(Radius,0);
        for (int32 I=1; I<=64; ++I)
        {
            const FVector2D Next = Centre + Radial(I*360./64.)*Radius;
            Triangle(Centre, Last, Next, Inside, Outside, Outside);
            Last = Next;
        }
    }
    void Fade(double X,double Y,double W,double H,FLinearColor Left,FLinearColor Right)
    {
        const FVector2D A(X,Y), B(X+W,Y), C(X+W,Y+H), D(X,Y+H);
        Triangle(A,B,C,Left,Right,Right); Triangle(A,C,D,Left,Right,Left);
    }
    void Diamond(FVector2D P, float R, FLinearColor Color, float Weight)
    {
        const FVector2D Top(0,-R), Right(R,0);
        Stroke(P+Top,P+Right,Weight,Color); Stroke(P+Right,P-Top,Weight,Color);
        Stroke(P-Top,P-Right,Weight,Color); Stroke(P-Right,P+Top,Weight,Color);
    }
    void Flush()
    {
        if (Triangles.IsEmpty()) return;
        FCanvasTriangleItem Item(FVector2D::ZeroVector,FVector2D::ZeroVector,FVector2D::ZeroVector,GWhiteTexture);
        Item.TriangleList = MoveTemp(Triangles);
        Item.BlendMode = SE_BLEND_Translucent;
        Canvas.DrawItem(Item);
        Triangles = MoveTemp(Item.TriangleList);
        Triangles.Reset();
    }

    FSlateFontInfo Font(FName Face, float Points) const { return FSlateFontInfo(UEngine::GetMediumFont(), Points * Scale, Face); }
    double Measure(const FString& S, FName Face, float Points) const
    {
        return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, Font(Face,Points)).X / Scale;
    }
    void Label(const FString& S, FVector2D P, float Points, FName Face, FLinearColor Color, bool Center=false)
    {
        Flush();
        FCanvasTextStringViewItem Item(P * Scale, FStringView(S), Font(Face,Points), Color);
        Item.bCentreX = Center;
        Item.bCentreY = true;
        Item.EnableShadow(FLinearColor(0,0,0,Color.A*.92f), FVector2D(1,1) * Scale);
        Canvas.DrawItem(Item);
    }
};

// The source street segments can span the entire map even if both ends are outside the dial.
bool ClipToDisc(FVector2D& A,FVector2D& B,double Radius)
{
    const FVector2D Delta = B-A;
    const double LengthSq = Delta.SizeSquared();
    if (LengthSq < UE_SMALL_NUMBER) return false;
    const double T = -FVector2D::DotProduct(A,Delta)/LengthSq;
    const double SpanSq = Radius*Radius - (A+T*Delta).SizeSquared();
    if (SpanSq<=0.) return false;
    const double HalfT = FMath::Sqrt(SpanSq/LengthSq);
    const double Enter = FMath::Max(0.,T-HalfT), Leave = FMath::Min(1.,T+HalfT);
    if (Enter>=Leave) return false;
    B=A+Leave*Delta; A+=Enter*Delta;
    return true;
}

void House(FInstruments& Draw,FVector2D P,FLinearColor Ink)
{
    Draw.Stroke(P+FVector2D(-4,0),P+FVector2D(0,-4),1.6f,Ink);
    Draw.Stroke(P+FVector2D(0,-4),P+FVector2D(4,0),1.6f,Ink);
    Draw.Quad(P+FVector2D(-2.7,0),P+FVector2D(2.7,0),P+FVector2D(2.7,3),P+FVector2D(-2.7,3),Ink);
}

void PlaceSymbol(FInstruments& Draw,FVector2D P,ERiverSiteKind Kind)
{
    const FLinearColor Ink = Ivory.CopyWithNewOpacity(.86f);
    if (Kind==ERiverSiteKind::Wardrobe)
    {
        return; // Flat 2B has its own persistent marker.
    }
    else if (Kind==ERiverSiteKind::ClothesShop || Kind==ERiverSiteKind::Tea || Kind==ERiverSiteKind::Print)
    {
        Draw.Arc(P+FVector2D(0,-.4),2.5,180,360,1.2f,Ink);
        Draw.Quad(P+FVector2D(-3,-.5),P+FVector2D(3,-.5),P+FVector2D(3.6,3.5),P+FVector2D(-3.6,3.5),Ink);
    }
    else Draw.Diamond(P,3.5f,Ink,1.4f);
}

void StreetMap(FInstruments& Draw,const APawn& Pawn,const ARiverfrontPlayerController& Player,
    const ARiverfrontWorld& District,bool Driving,const FString& Destination)
{
    const bool City = District.IsCityMode();
    const float Radius = Driving ? 80.f : 72.f;
    const FVector2D Centre(32+Radius,Draw.Height-36-Radius);
    const double ViewRange = Driving ? 17000. : 11500.;
    const double MapScale = Radius / ViewRange;
    const FVector Here = Pawn.GetActorLocation();
    bool bVisibleRoad = false;
    auto Project = [&](const FVector& Point)
    {
        return FVector2D(Here.Y-Point.Y,Here.X-Point.X)*MapScale;
    };
    auto Segment = [&](FVector2D A,FVector2D B,float Weight,FLinearColor Color)
    {
        if (ClipToDisc(A,B,Radius-3.f)) Draw.Stroke(Centre+A,Centre+B,Weight,Color);
    };
    Draw.Disc(Centre,Radius+5,Graphite.CopyWithNewOpacity(.9f),Graphite.CopyWithNewOpacity(0));
    Draw.Disc(Centre,Radius,Graphite.CopyWithNewOpacity(.93f),Graphite.CopyWithNewOpacity(.86f));
    if (!City)
    {
        const double R = Radius-2.f;
        const double Left = FMath::Clamp((Here.Y-2300.)*MapScale,-R,R);
        const double Right = FMath::Clamp((Here.Y+24300.)*MapScale,-R,R);
        if (Right>Left)
        {
            // The two riverfront banks are parallel. Cut their water between the corresponding chords.
            double X = Left, Y = FMath::Sqrt(FMath::Max(0.,R*R-X*X));
            for (int32 I=1; I<=32; ++I)
            {
                const double NX = FMath::Lerp(Left,Right,I/32.);
                const double NY = FMath::Sqrt(FMath::Max(0.,R*R-NX*NX));
                Draw.Quad(Centre+FVector2D(X,-Y),Centre+FVector2D(NX,-NY),
                    Centre+FVector2D(NX,NY),Centre+FVector2D(X,Y),Water.CopyWithNewOpacity(.84f));
                X=NX; Y=NY;
            }
        }
        for (const double Bank : {2300.,-24300.})
            Segment(Project(FVector(-84000,Bank,0)),Project(FVector(84000,Bank,0)),1.4f,Silver);
        Segment(Project(FVector(-5350,2800,0)),Project(FVector(-5350,-24700,0)),2.f,Ivory);
    }
    District.VisitMapRoads(Here, ViewRange, [&](const FVector& A, const FVector& B, bool bMajor)
    {
        FVector2D From = Project(A), To = Project(B);
        if (!ClipToDisc(From, To, Radius-3.f)) return;
        bVisibleRoad = true;
        Draw.Stroke(Centre+From,Centre+To,bMajor ? 2.15f : 1.15f,Road.CopyWithNewOpacity(bMajor ? 1.f : .68f));
    });
    for (const auto& Place : Player.GetPlaces())
    {
        const FVector2D P = Project(Place.Point);
        if (P.SizeSquared()<FMath::Square(Radius-9.f)) PlaceSymbol(Draw,Centre+P,Place.Kind);
    }

    Draw.Arc(Centre,Radius+1,0,360,1.2f,Silver.CopyWithNewOpacity(.83f));
    for (int32 I=1; I<12; ++I)
    {
        const FVector2D Tick = Radial(-90+I*30);
        Draw.Stroke(Centre+Tick*(Radius-1),Centre+Tick*(Radius-(I%3==0?7:4)),1.1f,Ivory.CopyWithNewOpacity(.9f));
    }
    if (!Destination.IsEmpty())
    {
        const FVector2D To = Centre+Project(Player.NextWaypoint()).GetClampedToMaxSize(Radius-8.f);
        Draw.Stroke(Centre,To,1.4f,Gold.CopyWithNewOpacity(.82f));
        Draw.Diamond(To,5.4f,Graphite,4.f); Draw.Diamond(To,5.4f,Gold,2.f);
    }
    const FVector Forward = Pawn.GetActorForwardVector();
    const FVector2D Nose = FVector2D(-Forward.Y,-Forward.X).GetSafeNormal();
    const FVector2D Side(-Nose.Y,Nose.X);
    for (const float Size : {11.f,8.f})
    {
        const FLinearColor Color = Size>8.f ? Graphite : Ivory;
        const FVector2D Tip = Centre+Nose*Size, Tail = Centre-Nose*(Size*.28);
        Draw.Fill(Tip,Centre-Nose*(Size*.63)+Side*(Size*.69),Tail,Color);
        Draw.Fill(Tip,Tail,Centre-Nose*(Size*.63)-Side*(Size*.69),Color);
    }
    if (City)
    {
        if (!bVisibleRoad) Draw.Label(TEXT("No mapped streets"),Centre+FVector2D(0,23),11,TEXT("Medium"),Ivory,true);
        House(Draw, Centre+FVector2D(-64,-Radius-30), Home);
        Draw.Label(TEXT("Flat 2B · Pause to return"),Centre+FVector2D(4,-Radius-30),11,TEXT("Medium"),Home,true);
    }
    else
    {
        const FVector Entry = FRiverfrontFirstMorningWorld::StreetPoint();
        const FVector2D Offset = Project(Entry);
        // Leave space for the player chevron while standing at the entrance.
        const FVector2D Marker = Centre + (Offset.SizeSquared()<196. ? FVector2D(13,9)
            : Offset.GetClampedToMaxSize(Radius-11.));
        Draw.Disc(Marker,8,Graphite,Graphite);
        House(Draw,Marker,Home);
        const int32 Metres = FMath::RoundToInt(FVector::Dist2D(Here,Entry)/100.);
        Draw.Label(Metres<15 ? TEXT("Flat 2B") : FString::Printf(TEXT("Flat 2B  ·  %d m"),Metres),
            Centre+FVector2D(0,-Radius-30),11,TEXT("Medium"),Home,true);
    }
    Draw.Label(TEXT("N"),Centre+FVector2D(0,-Radius-11),12,TEXT("Bold"),Ivory,true);
    Draw.Label(City ? TEXT("Ahmedabad") : TEXT("Sabarmati Riverfront"),
        FVector2D(Centre.X,Draw.Height-19),12,TEXT("BoldCondensed"),Ivory,true);
}

void DriveCluster(FInstruments& Draw,const ARiverfrontCar& Car)
{
    const FVector2D Centre(Draw.Width-116,Draw.Height-106);
    Draw.Disc(Centre,84,Graphite.CopyWithNewOpacity(.94f),Graphite.CopyWithNewOpacity(0));
    const double Tach = FMath::Clamp(Car.GetEngineRpm()/6000.,0.,1.);
    Draw.Arc(Centre,71,155,385,3.5f,Silver.CopyWithNewOpacity(.44f));
    Draw.Arc(Centre,71,155,155+230*Tach,3.5f,Gold);
    for (int32 I=0; I<=24; ++I)
    {
        const FVector2D Tick = Radial(155+230.*I/24.);
        Draw.Stroke(Centre+Tick*74.,Centre+Tick*(I%4==0?81.:77.),I%4==0?1.8f:1.f,
            I>=20 ? Warning : Ivory.CopyWithNewOpacity(.85f));
    }
    Draw.Label(FString::FromInt(FMath::RoundToInt(Car.GetSpeedKph())),Centre+FVector2D(0,-7),
        47,TEXT("BoldCondensedItalic"),Ivory,true);
    Draw.Label(TEXT("km/h"),Centre+FVector2D(0,22),12,TEXT("Medium"),Ivory,true);
    const int32 Gear = Car.GetGear();
    const FString GearLabel = Gear<0 ? TEXT("R") : FString::Printf(TEXT("D%d"),Gear);
    const float Fuel = FMath::Clamp(Car.GetFuel(),0.f,1.f);
    Draw.Label(GearLabel,Centre+FVector2D(-36,46),15,TEXT("BoldCondensed"),Gold,true);
    Draw.Stroke(Centre+FVector2D(-14,37),Centre+FVector2D(-14,55),1.f,Silver);
    Draw.Label(FString::Printf(TEXT("%.0f%% fuel"),100.f*Fuel),Centre+FVector2D(-4,46),
        11,TEXT("Medium"),Fuel<.15f?Warning:Ivory);
    const FVector2D FuelStart=Centre+FVector2D(-4,58);
    Draw.Stroke(FuelStart,FuelStart+FVector2D(54,0),2.f,Silver.CopyWithNewOpacity(.32f));
    if (Fuel>0.f)
        Draw.Stroke(FuelStart,FuelStart+FVector2D(54*Fuel,0),2.f,Fuel<.15f?Warning:Gold);
    const int32 Signal = Car.GetSignal();
    for (const int32 Side : {-1,1})
    {
        if (Signal!=Side && Signal!=2) continue;
        const FVector2D Tip=Centre+FVector2D(Side*46,12);
        Draw.Stroke(Tip+FVector2D(-Side*6,-5),Tip,2.3f,Gold);
        Draw.Stroke(Tip,Tip+FVector2D(-Side*6,5),2.3f,Gold);
    }
}

void Guidance(FInstruments& Draw,const ARiverfrontPlayerController& Player,const FString& Destination)
{
    const double Centre = Draw.Width*.5;
    const double TextWidth = Draw.Measure(Destination,TEXT("Medium"),13);
    const double Half = TextWidth*.5 + 30.;
    Draw.Fade(Centre-Half-22,23,22,30,Graphite.CopyWithNewOpacity(0),Graphite.CopyWithNewOpacity(.77f));
    Draw.Fade(Centre-Half,23,Half*2,30,Graphite.CopyWithNewOpacity(.77f),Graphite.CopyWithNewOpacity(.77f));
    Draw.Fade(Centre+Half,23,22,30,Graphite.CopyWithNewOpacity(.77f),Graphite.CopyWithNewOpacity(0));
    Draw.Diamond(FVector2D(Centre-TextWidth*.5-9,38),5.f,Gold,1.6f);
    Draw.Label(Destination,FVector2D(Centre+5,38),13,TEXT("Medium"),Ivory,true);
    FVector2D Screen;
    if (!Player.ProjectWorldLocationToScreen(Player.NextWaypoint()+FVector(0,0,135),Screen,true)) return;
    Screen/=Draw.Scale;
    if (Screen.X<35 || Screen.X>Draw.Width-35 || Screen.Y<65 || Screen.Y>Draw.Height-175) return;
    Draw.Diamond(Screen,9,Graphite,4.f);
    Draw.Diamond(Screen,9,Gold,2.f);
}

void Controls(FInstruments& Draw,bool City)
{
    const TCHAR* Keys[] = { TEXT("W A S D  /  SHIFT"), TEXT("E  /  I"), TEXT("SPACE  /  O"),
        TEXT("C  /  Z X  /  H"), City?TEXT("N  /  V  /  M"):TEXT("N  /  G  /  M"),
        TEXT("T  /  P  /  Y"), TEXT("B  /  F  /  F3"), TEXT("R  /  ESC") };
    const TCHAR* Actions[] = { TEXT("Move or drive  /  Run"), TEXT("Interact or exit  /  Wallet"),
        TEXT("Jump, vault, climb or brake  /  Door"), TEXT("Camera  /  Indicators  /  Horn"),
        City?TEXT("Destination  /  Street travel  /  Map"):TEXT("Destination  /  River run  /  Map"),
        TEXT("Time  /  Weather  /  Clock"), TEXT("Hazards  /  Service  /  Frame timing"),
        TEXT("Reset  /  Pause") };
    Draw.Fade(0,17,374,249,Graphite.CopyWithNewOpacity(.93f),Graphite.CopyWithNewOpacity(.90f));
    Draw.Fade(374,17,77,249,Graphite.CopyWithNewOpacity(.90f),Graphite.CopyWithNewOpacity(0));
    Draw.Label(TEXT("Controls"),FVector2D(32,39),21,TEXT("BoldCondensed"),Ivory);
    Draw.Label(TEXT("F1   Close"),FVector2D(32,66),11,TEXT("Medium"),Gold);
    for (int32 I=0; I<UE_ARRAY_COUNT(Keys); ++I)
    {
        Draw.Label(Keys[I],FVector2D(32,92+22*I),10,TEXT("BoldCondensed"),Gold);
        Draw.Label(Actions[I],FVector2D(140,92+22*I),12,TEXT("Regular"),Ivory);
    }
}

FString ActionKey(const ARiverfrontPlayerController& Player,FName Action,const FString& Fallback)
{
    const UPlayerInput* Input=Player.PlayerInput;
    if (!Input) return Fallback;

    const FInputActionKeyMapping* Selected=nullptr;
    for (const FInputActionKeyMapping& Mapping : Input->GetKeysForAction(Action))
    {
        if (!Mapping.Key.IsValid()) continue;
        if (!Selected) Selected=&Mapping;
        if (!Mapping.Key.IsGamepadKey()) { Selected=&Mapping; break; }
    }
    if (!Selected) return Fallback;

    FString Label;
    if (Selected->bCtrl) Label+=TEXT("Ctrl + ");
    if (Selected->bAlt) Label+=TEXT("Alt + ");
    if (Selected->bShift) Label+=TEXT("Shift + ");
    if (Selected->bCmd) Label+=TEXT("Cmd + ");
    return Label+Selected->Key.GetDisplayName().ToString();
}

float Message(FInstruments& Draw,const FString& Value,const FString& Key,float Bottom,double MaxWidth,bool Action)
{
    if (Value.IsEmpty()) return Bottom;
    const FString& Body=Value;
    const float Points = Action ? 16.f : 13.f;
    const FName Face = Action ? FName(TEXT("Medium")) : FName(TEXT("Regular"));
    const double KeySpace = Key.IsEmpty()?0.:FMath::Max(40.,Draw.Measure(Key,TEXT("BoldCondensed"),13)+24.);
    TArray<FString> Lines;
    Lines.Add(Body);
    if (Draw.Measure(Body,Face,Points)>MaxWidth-KeySpace)
    {
        Lines.Reset();
        TArray<FString> Words;
        Body.ParseIntoArrayWS(Words);
        FString Current;
        for (const FString& Word : Words)
        {
            const FString Candidate=Current.IsEmpty()?Word:Current+TEXT(" ")+Word;
            if (!Current.IsEmpty() && Draw.Measure(Candidate,Face,Points)>MaxWidth-KeySpace)
            {
                Lines.Add(Current); Current=Word;
            }
            else Current=Candidate;
        }
        if (!Current.IsEmpty()) Lines.Add(Current);
    }
    double TextWidth=0.;
    for (const FString& Line : Lines) TextWidth=FMath::Max(TextWidth,Draw.Measure(Line,Face,Points));
    const double H=FMath::Max(36.,Lines.Num()*24.+12.);
    const double W=TextWidth+KeySpace+22., X=(Draw.Width-W)*.5, Y=Bottom-H;
    Draw.Fade(X-21,Y,21,H,Graphite.CopyWithNewOpacity(0),Graphite.CopyWithNewOpacity(.80f));
    Draw.Fade(X,Y,W,H,Graphite.CopyWithNewOpacity(.80f),Graphite.CopyWithNewOpacity(.80f));
    Draw.Fade(X+W,Y,21,H,Graphite.CopyWithNewOpacity(.80f),Graphite.CopyWithNewOpacity(0));
    if (!Key.IsEmpty())
    {
        const FVector2D P(X+9,Y+H*.5);
        const double KeyWidth=KeySpace-15.;
        Draw.Quad(P+FVector2D(0,-12),P+FVector2D(KeyWidth,-12),
            P+FVector2D(KeyWidth,12),P+FVector2D(0,12),Graphite.CopyWithNewOpacity(.96f));
        Draw.Stroke(P+FVector2D(0,-12),P+FVector2D(KeyWidth,-12),1.f,Gold);
        Draw.Stroke(P+FVector2D(KeyWidth,-12),P+FVector2D(KeyWidth,12),1.f,Gold);
        Draw.Stroke(P+FVector2D(KeyWidth,12),P+FVector2D(0,12),1.f,Gold);
        Draw.Stroke(P+FVector2D(0,12),P+FVector2D(0,-12),1.f,Gold);
        Draw.Label(Key,P+FVector2D(KeyWidth*.5,0),13,TEXT("BoldCondensed"),Ivory,true);
    }
    for (int32 I=0; I<Lines.Num(); ++I)
        Draw.Label(Lines[I],FVector2D(X+11+KeySpace,Y+H*.5+(I-(Lines.Num()-1)*.5)*24),
            Points,Face,Action?Ivory:Gold);
    return Y-5;
}
}

void ARiverfrontHUD::DrawHUD()
{
    Super::DrawHUD();
    const auto* Player = Cast<ARiverfrontPlayerController>(PlayerOwner);
    if (!Canvas || Canvas->ClipX<=0 || Canvas->ClipY<=0 || !Player || Player->IsShowingMenu()
        || !GEngine || !UEngine::GetMediumFont() || !FSlateApplication::IsInitialized()) return;
    const APawn* Pawn = Player->GetPawn();
    if (!Pawn) return;
    using namespace RiverfrontInstruments;
    FInstruments Draw(*Canvas,HUDTriangles);
    const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    const bool City = Mode && Mode->IsCityMode();
    const auto* Car = Cast<ARiverfrontCar>(Pawn);
    const FString Destination=Player->NavigationText();
    if (Player->MapVisible() && Mode && Mode->GetDistrict())
    {
        StreetMap(Draw,*Pawn,*Player,*Mode->GetDistrict(),Car!=nullptr,Destination);
    }
    if (!Destination.IsEmpty()) Guidance(Draw,*Player,Destination);
    if (Car) DriveCluster(Draw,*Car);
    if (City)
    {
        const FString Credit=TEXT("© OpenStreetMap contributors");
        Draw.Label(Credit,FVector2D(Draw.Width-26-Draw.Measure(Credit,TEXT("Regular"),10),Draw.Height-15),
            10,TEXT("Regular"),Ivory);
    }
    const float Now = GetWorld()->GetTimeSeconds();
    if (WelcomeUntil < 0) WelcomeUntil = Now+8;
    if (Player->ControlsVisible()) Controls(Draw,City);
    else if (Now<WelcomeUntil)
    {
        const float Opacity=FMath::Clamp(WelcomeUntil-Now,0.f,1.f);
        Draw.Fade(0,23,315,59,Graphite.CopyWithNewOpacity(.83f*Opacity),Graphite.CopyWithNewOpacity(0));
        Draw.Label(City?TEXT("Ahmedabad"):TEXT("Sabarmati Riverfront"),FVector2D(32,41),
            21,TEXT("BoldCondensed"),Ivory.CopyWithNewOpacity(Opacity));
        Draw.Label(TEXT("F1  Controls     I  Wallet     Esc  Pause"),FVector2D(33,67),
            11,TEXT("Medium"),Gold.CopyWithNewOpacity(Opacity));
    }
    FString Prompt, Key;
    const auto* Character=Cast<ARiverfrontCharacter>(Pawn);
    const auto* Nearby=Character && Mode ? Mode->FindEnterableCar(*Character) : nullptr;
    if (Nearby)
    {
        Prompt=FString::Printf(TEXT("Enter %s"),Nearby->GetVehicleLabel());
        Key=ActionKey(*Player,TEXT("Interact"),TEXT("E"));
    }
    else
    {
        Prompt=Player->PlacePrompt();
        if (Prompt.Len()>4 && Prompt.Mid(1,3)==TEXT("   "))
        {
            Key=ActionKey(*Player,Car?FName(TEXT("UsePlace")):FName(TEXT("Interact")),Prompt.Left(1));
            Prompt=Prompt.Mid(4);
        }
        if (Prompt.IsEmpty() && Car) Prompt=Car->GetHint();
    }
    const double MaxWidth=FMath::Min(620.,Draw.Width-390.);
    const float AbovePrompt=Message(Draw,Prompt,Key,Draw.Height-38,MaxWidth,true);
    Message(Draw,Player->ActivityText(),FString(),AbovePrompt,MaxWidth,false);
    Draw.Flush();
}
