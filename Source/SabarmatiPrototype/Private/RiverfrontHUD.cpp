#include "RiverfrontHUD.h"

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

    if (const ARiverfrontPlayerController* Driver = Cast<ARiverfrontPlayerController>(PlayerOwner);
        Driver && Driver->IsShowingMenu())
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

    const float Base = Height - 76.f;
    const bool bCompact = Width < 760.f;
    Box(Dark, Margin, Base, bCompact ? 247.f : 373.f, 49.f);
    Text(bInCar ? TEXT("WASD  drive   /   Space  brake   /   E  exit")
        : TEXT("WASD  move   /   Shift  jog   /   Space  jump"),
        Paper, Margin + 12.f, Base + 7.f, Font, bCompact ? .83f : .96f);
    Text(bCity ? TEXT("Mouse  look   /   R  reset   /   Esc  pause")
        : TEXT("Mouse look   /   R reset   /   T time   /   Esc pause"),
        Amber, Margin + 12.f, Base + 27.f, Font, bCompact ? .81f : .88f);

    if (bInCar)
    {
        const float SpeedX = Width - Margin - 153.f;
        Box(Dark, SpeedX, Base - 13.f, 153.f, 62.f);
        Text(FString::Printf(TEXT("%d"), FMath::RoundToInt(Car->GetSpeedKph())),
            Paper, SpeedX + 13.f, Base - 6.f, Headline, 1.58f);
        Text(TEXT("km/h"), Amber, SpeedX + 96.f, Base + 11.f, Font, .92f);
        Text(Car->GetVehicleLabel(), Paper, SpeedX + 13.f, Base + 29.f, Font, .78f);
    }
    if (bCity)
    {
        const float CreditX = Width - Margin - 187.f;
        Box(Dark, CreditX, Height - 23.f, 187.f, 17.f);
        Text(TEXT("© OpenStreetMap contributors"), Paper, CreditX + 7.f, Height - 21.f, Font, .77f);
    }

    const FString Hint = bInCar ? Car->GetHint() : Car
        ? FString::Printf(TEXT("Open door  ·  %s"), Car->GetVehicleLabel()) : FString();
    if (!Hint.IsEmpty())
    {
        constexpr float HintWidth = 298.f;
        const float HintX = (Width - HintWidth) * .5f;
        const float HintY = Base - 56.f;
        Box(Dark, HintX, HintY, HintWidth, 36.f);
        if (!bInCar)
        {
            Box(Amber, HintX + 7.f, HintY + 7.f, 25.f, 22.f);
            Text(TEXT("E"), Dark.CopyWithNewOpacity(1.f), HintX + 15.f, HintY + 9.f, Font, .89f);
        }
        Text(Hint, Paper, HintX + (bInCar ? 10.f : 43.f), HintY + 9.f, Font, .92f);
    }
}
