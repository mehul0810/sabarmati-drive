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
    const float Now = GetWorld()->GetTimeSeconds();
    if (WelcomeUntil < 0.f) WelcomeUntil = Now + 8.f;
    if (!Driver->ControlsVisible() && Now < WelcomeUntil)
    {
        const float Opacity = FMath::Clamp(WelcomeUntil - Now, 0.f, 1.f);
        Box(Dark.CopyWithNewOpacity(.84f * Opacity), Margin, Margin, 248.f, 55.f);
        Box(Amber.CopyWithNewOpacity(Opacity), Margin, Margin, 3.f, 55.f);
        Text(bCity ? TEXT("Ahmedabad") : TEXT("Sabarmati Riverfront"),
            Paper.CopyWithNewOpacity(Opacity), Margin+13, Margin+7, Headline, 1.05f);
        Text(TEXT("F1  controls     I  wallet     Esc  pause"),
            Amber.CopyWithNewOpacity(Opacity), Margin+13, Margin+33, Font, .86f);
    }

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
        const TCHAR* Lines[] = {
            TEXT("WASD  move or drive     /     Shift  jog"),
            TEXT("E  interact or exit     /     I  wallet"),
            TEXT("Space  jump or brake     /     O  car door"),
            TEXT("C  camera     /     Z, X  indicators     /     H  horn"),
            bCity ? TEXT("N  destination     /     V  street travel     /     M  map") :
                    TEXT("N  destination     /     G  river run     /     M  map"),
            TEXT("T  time     /     P  weather     /     Y  clock"),
            TEXT("B  hazards     /     F  service     /     F3  timing")
        };
        Box(Dark,Margin,Margin,348.f,216.f);
        Box(Amber,Margin,Margin,3.f,216.f);
        Text(TEXT("Controls"),Paper,Margin+13,Margin+9,Headline,1.1f);
        Text(TEXT("F1 to close"),Amber,Margin+255,Margin+12,Font,.8f);
        for (int32 I=0; I<UE_ARRAY_COUNT(Lines); ++I)
            Text(Lines[I],Paper,Margin+13,Margin+43+I*23,Font,.88f);
    }
    const float Base = Height - 76.f;
    if (bInCar)
    {
        const float SpeedX = Width - Margin - 150.f;
        Box(Dark, SpeedX, Base - 15.f, 150.f, 65.f);
        const FString Gear = Car->GetGear() < 0 ? TEXT("R") : FString::FromInt(Car->GetGear());
        Text(FString::FromInt(FMath::RoundToInt(Car->GetSpeedKph())), Paper,
            SpeedX + 12.f, Base - 9.f, Headline, 1.68f);
        Text(TEXT("km/h"), Amber, SpeedX + 91.f, Base + 10.f, Font, .93f);
        Text(FString::Printf(TEXT("Gear %s    ·    Fuel %.0f%%"), *Gear, Car->GetFuel()*100.f),
            Paper, SpeedX + 12.f, Base + 29.f, Font, .87f);
    }
    if (bCity)
    {
        const float CreditX = Width - Margin - 187.f;
        Box(Dark, CreditX, Height - 23.f, 187.f, 17.f);
        Text(TEXT("© OpenStreetMap contributors"), Paper, CreditX + 7.f, Height - 21.f, Font, .77f);
    }

    const auto Message = [&](const FString& Value, float Y, bool bAction)
    {
        if (Value.IsEmpty()) return;
        float W = 0, H = 0;
        GetTextSize(Value,W,H,Font,1.f);
        const float MaxWidth = FMath::Max(20.f,Width-2*Margin-28.f);
        const float Size = FMath::Min(.98f, MaxWidth / FMath::Max(1.f,W));
        const float MessageWidth = FMath::Min(MaxWidth,W*Size)+28.f;
        const float X = (Width-MessageWidth)*.5f;
        Box(Dark,X,Y,MessageWidth,36.f);
        if (bAction) Box(Amber,X,Y,3.f,36.f);
        Text(Value,bAction ? Paper : Amber,X+14.f,Y+9.f,Font,Size);
    };

    const FString Place = Driver->PlacePrompt();
    const FString Hint = !Place.IsEmpty() ? Place : bInCar ? Car->GetHint() : Car
        ? FString::Printf(TEXT("E   Open door  ·  %s"),Car->GetVehicleLabel()) : FString();
    const float HintY = bInCar ? Base-58.f : Height-Margin-40.f;
    Message(Driver->ActivityText(),Hint.IsEmpty() ? HintY : HintY-40.f,false);
    Message(Hint,HintY,true);
}
