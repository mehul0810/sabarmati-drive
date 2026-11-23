#include "RiverfrontHUD.h"
#include "RiverfrontTraffic.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RiverfrontCar.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontPlayerController.h"

void ARiverfrontHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner || !GEngine)
    {
        return;
    }

    const auto* Driver=Cast<ARiverfrontPlayerController>(PlayerOwner);
    if (!Driver || Driver->IsShowingMenu())
    {
        return;
    }

    const float Scale = FMath::Clamp(Canvas->ClipY / 900.f, .68f, 2.40f);
    const float Width = Canvas->ClipX / Scale;
    const float Height = Canvas->ClipY / Scale;
    constexpr float Margin = 24.f;
    const FLinearColor Paper(.96f, .92f, .85f, 1.f);
    const FLinearColor Amber(.88f, .68f, .43f, 1.f);
    const FLinearColor Dark(.019f, .024f, .024f, .88f);
    UFont* Font = GEngine->GetSmallFont();
    UFont* Headline = GEngine->GetMediumFont();

    const auto Box = [this, Scale](FLinearColor Color, float X, float Y, float W, float H)
    {
        DrawRect(Color, X * Scale, Y * Scale, W * Scale, H * Scale);
    };
    const auto Text = [this, Scale](const FString& Label, FLinearColor Color, float X, float Y, UFont* Face, float Size)
    {
        DrawText(Label, Color, X * Scale, Y * Scale, Face, Size * Scale);
    };

    const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    const bool bCity = Mode && Mode->IsCityMode();
    Box(Dark, Margin, Margin, 230.f, 42.f);
    Box(Amber, Margin, Margin, 3.f, 42.f);
    Text(bCity ? TEXT("AHMEDABAD / FREE ROAM") : TEXT("SABARMATI RIVERFRONT"),
        Paper, Margin + 13.f, Margin + 13.f, Font, 1.f);

    const APawn* Pawn = PlayerOwner->GetPawn();
    const ARiverfrontCar* Car = Cast<ARiverfrontCar>(Pawn);
    const bool bInCar = Car != nullptr;
    if (!bInCar)
    {
        const ARiverfrontCharacter* Character = Cast<ARiverfrontCharacter>(Pawn);
        Car = Mode && Character ? Mode->FindEnterableCar(*Character) : nullptr;
    }

    if (Pawn && Driver->MapVisible() && Mode && Mode->GetTraffic())
    {
        const float Side=Width<760.f?105.f:134.f;
        const float X=Width-Margin-Side,Y=Margin;
        Box(Dark,X,Y,Side,Side); Box(Amber,X,Y,3.f,Side);
        const FVector2D Centre(X+Side*.5,Y+Side*.5);
        const FVector P=Pawn->GetActorLocation();
        const auto Project=[&](const FVector& At) {
            return FVector2D(Centre.X-(At.Y-P.Y)*Side/24000.,Centre.Y-(At.X-P.X)*Side/24000.);
        };
        // Clip line segments to the minimap, including roads that cross the viewport with both ends outside.
        auto Segment=[&](FVector2D A,FVector2D B,FLinearColor Color,float Weight)
        {
            FVector2D V=B-A;
            double Lo=0,Hi=1;
            auto Clip=[&](double Dir,double Gap)
            {
                if (FMath::Abs(Dir)<1.e-7) return Gap>=0.;
                const double R=Gap/Dir;
                if (Dir<0) Lo=FMath::Max(Lo,R); else Hi=FMath::Min(Hi,R);
                return Lo<=Hi;
            };
            if (!Clip(-V.X,A.X-X-3) || !Clip(V.X,X+Side-A.X-2) || !Clip(-V.Y,A.Y-Y-2) || !Clip(V.Y,Y+Side-A.Y-2)) return;
            const FVector2D End=A+Hi*V; A+=Lo*V;
            DrawLine(A.X*Scale,A.Y*Scale,End.X*Scale,End.Y*Scale,Color,Weight*Scale);
        };
        for (const auto& Line:Mode->GetTraffic()->MapSegments())
            Segment(Project(Line.Key),Project(Line.Value),FLinearColor(.44,.47,.43),1.5f);
        if (!bCity)
        {
            for (const double Bank:{2300.,-24300.}) Segment(Project(FVector(-84000.,Bank,0)),Project(FVector(84000.,Bank,0)),FLinearColor(.18,.47,.52),2.4f);
            Segment(Project(FVector(-5350.,2800.,0)),Project(FVector(-5350.,-24700.,0)),FLinearColor(.48,.57,.51),1.f);
        }
        const FString Direction=Driver->NavigationText();
        if (!Direction.IsEmpty())
        {
            FVector2D Target=Project(Driver->NextWaypoint());
            const FVector2D Delta=Target-Centre;
            const double Normal=FMath::Max(FMath::Abs(Delta.X),FMath::Abs(Delta.Y));
            if (Normal>Side*.44) Target=Centre+Delta*(Side*.44/Normal);
            Segment(Centre,Target,Amber,1.3f); Box(Amber,Target.X-3,Target.Y-3,6,6);
            Box(Dark,X-70,Y+Side+2,Side+70,25);Text(Direction,Paper,X-64,Y+Side+7,Font,.80f);
            FVector2D Screen;
            if (PlayerOwner->ProjectWorldLocationToScreen(Driver->NextWaypoint()+FVector(0,0,135),Screen,true))
            {
                const float SX=Screen.X/Scale,SY=Screen.Y/Scale;
                if (SX>Margin+10 && SX<Width-Margin-10 && SY>Margin+28 && SY<Height-110)
                { Box(Amber,SX-4,SY-4,8,8); Box(Dark,SX-2,SY-2,4,4); }
            }
        }
        const FVector Facing=Pawn->GetActorForwardVector();
        Segment(Centre,Centre+FVector2D(-Facing.Y,-Facing.X)*9,Paper,2.3f);
        Box(Paper,Centre.X-2,Centre.Y-2,4,4);
        Text(TEXT("N"),Paper,X+7,Y+4,Font,.72f);
    }
    if (Driver->ControlsVisible())
    {
        const float Y=82;
        const TCHAR* Lines[]={TEXT("WASD  move or drive     /     E  interact or exit"),TEXT("O  front door     /     C  chase, cockpit, bonnet"),
            TEXT("Z / X  indicators     /     B  hazards     /     H  horn"),bCity?TEXT("N  destination     /     V  street travel     /     M  map"):TEXT("N  destination     /     M  map     /     G  river run"),
            TEXT("T  time     /     P  weather     /     Y  clock"),TEXT("F  service or visit     /     F3  timing     /     F1  close")};
        Box(Dark,Margin,Y,364.f,144.f);
        for (int32 I=0;I<UE_ARRAY_COUNT(Lines);++I) Text(Lines[I],I%2?Amber:Paper,Margin+10.f,Y+10.f+I*21.f,Font,.87f);
    }
    const float Base = Height - 76.f;
    const bool bCompact = Width < 760.f;
    Box(Dark, Margin, Base, bCompact ? 247.f : 373.f, 49.f);
    Text(bInCar ? TEXT("WASD  drive   /   Space  brake   /   E  exit")
        : TEXT("WASD  move   /   Shift  jog   /   Space  jump"),
        Paper, Margin + 12.f, Base + 7.f, Font, bCompact ? .83f : .96f);
    Text(bCity ? TEXT("Mouse  look   /   R  reset   /   Esc  pause")
        : TEXT("F1 controls   /   N waypoint   /   Esc pause"),
        Amber, Margin + 12.f, Base + 27.f, Font, bCompact ? .81f : .88f);

    if (bInCar)
    {
        const float SpeedX = Width - Margin - 153.f;
        Box(Dark, SpeedX, Base - 13.f, 153.f, 62.f);
        Text(FString::Printf(TEXT("%d"), FMath::RoundToInt(Car->GetSpeedKph())),
            Paper, SpeedX + 13.f, Base - 6.f, Headline, 1.58f);
        Text(TEXT("km/h"), Amber, SpeedX + 96.f, Base + 11.f, Font, .92f);
        Text(FString::Printf(TEXT("%s  ·  %s  ·  %.0f%% fuel"),Car->GetVehicleLabel(),
            Car->GetGear()<0?TEXT("R"):*FString::FromInt(Car->GetGear()),Car->GetFuel()*100),
            Paper,SpeedX+10.f,Base+29.f,Font,.74f);
    }
    if (bCity)
    {
        const float CreditX = Width - Margin - 187.f;
        Box(Dark, CreditX, Height - 23.f, 187.f, 17.f);
        Text(TEXT("© OpenStreetMap contributors"), Paper, CreditX + 7.f, Height - 21.f, Font, .77f);
    }

    const FString Notice=Driver->ActivityText();
    if (!Notice.IsEmpty())
    {
        const float W=FMath::Min(Width-2*Margin,440.f);
        Box(Dark,(Width-W)*.5f,Base-92,W,27);
        Text(Notice,Paper,(Width-W)*.5f+9,Base-86,Font,.83f);
    }
    const FString Place=Driver->PlacePrompt();
    const FString Hint = !Place.IsEmpty()?Place:bInCar ? Car->GetHint() : Car
        ? FString::Printf(TEXT("Open door  ·  %s"), Car->GetVehicleLabel()) : FString();
    if (!Hint.IsEmpty())
    {
        constexpr float HintWidth = 298.f;
        const float HintX = (Width - HintWidth) * .5f;
        const float HintY = Base - 56.f;
        Box(Dark, HintX, HintY, HintWidth, 36.f);
        if (!bInCar && Place.IsEmpty())
        {
            Box(Amber, HintX + 7.f, HintY + 7.f, 25.f, 22.f);
            Text(TEXT("E"), Dark.CopyWithNewOpacity(1.f), HintX + 15.f, HintY + 9.f, Font, .89f);
        }
        Text(Hint, Paper, HintX + (bInCar || !Place.IsEmpty() ? 10.f : 43.f), HintY + 9.f, Font, .92f);
    }
}
