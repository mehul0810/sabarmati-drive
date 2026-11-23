#include "RiverfrontWorld.h"
#include "Components/TextRenderComponent.h"

#if !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "Engine/GameViewportClient.h"
#include "Misc/Paths.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontPlayerController.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#endif

// An original mixed-use block. The structural wings, colonnade and cross-passage
// have separate solids so pedestrians and their camera can pass between them.
void ARiverfrontWorld::BuildArcadeBlock(const FVector& Plot, double Yaw, double PlotWidth,
    double PlotDepth, double Height)
{
    const FRotator Rotation(0, Yaw, 0);
    const auto WorldPoint = [&](double X, double Y, double Z)
    {
        return Plot + Rotation.RotateVector(FVector(X, Y, Z));
    };
    Box(ETint::Paving, WorldPoint(0, 0, 0), FVector(PlotWidth, PlotDepth, 12), true, Rotation);
    const auto Part = [&](ETint Tint, double X, double Y, double Z,
        double W, double D, double H, bool bSolid = false)
    {
        Box(Tint, WorldPoint(X, Y, 6 + Z), FVector(W, D, H), bSolid, Rotation);
    };

    const double HalfX = PlotWidth * .5 - 175;
    const double HalfY = PlotDepth * .5 - 175;
    const double Passage = FMath::Clamp(PlotWidth * .14, 320.0, 520.0);
    const double ShopFace = -HalfY + 198;
    const int32 Storeys = FMath::Clamp(FMath::RoundToInt(Height / 325.), 2, 5);
    const double Storey = Height / Storeys;

    // Facade coordinates run along the wall. The normal always points outdoors.
    const auto Window = [&](double X, double Y, double Angle, double Z, double W, double H, bool bDoor)
    {
        const FRotator Face(0, Yaw + Angle, 0);
        const FVector Center = WorldPoint(X, Y, 6 + Z);
        const auto Frame = [&](ETint Tint, double Across, double Out, double Up, double FW, double FD, double FH)
        {
            Box(Tint, Center + Face.RotateVector(FVector(Across, -Out, Up)), FVector(FW, FD, FH), false, Face);
        };
        Frame(ETint::Rail, 0, 3, 0, W + 12, 14, H + 12);
        Frame(ETint::Glass, 0, 11, 0, W, 5, H);
        Frame(ETint::Rail, 0, 15, 0, 5, 5, H);
        Frame(ETint::Rail, 0, 15, H * .21, W, 5, 4);
        if (bDoor)
        {
            Frame(ETint::PaleStone, -10, 19, -11, 3, 6, 37);
            Frame(ETint::PaleStone, 10, 19, -11, 3, 6, 37);
        }
        else
        {
            Frame(ETint::Retaining, 0, 12, -H * .5 - 6, W + 22, 28, 10);
        }
    };

    // Original 780 mm split-system condenser, pair of shelf brackets and exposed refrigerant line.
    const auto Condenser = [&](double X, double Y, double Z)
    {
        Part(ETint::Plaster, X, Y - 20, Z, 78, 38, 54);
        Part(ETint::Rail, X + 12, Y - 40, Z + 2, 40, 2.5, 40);
        for (int32 Slat = -3; Slat <= 3; ++Slat)
        {
            Part(ETint::Retaining, X + 12, Y - 42, Z + 2 + Slat * 4.8, 37, 2, 1.4);
        }
        for (const double Bracket : {-25., 25.})
        {
            Part(ETint::Rail, X + Bracket, Y - 18, Z - 29, 4, 42, 4);
            Part(ETint::Rail, X + Bracket, Y - 2, Z - 19, 4, 4, 26);
        }
        Part(ETint::Retaining, X - 40, Y - 9, Z - 21, 7, 7, 39);
        Part(ETint::Retaining, X - 47, Y - 6, Z - 40, 18, 6, 6);
    };

    for (const double Side : {-1.0, 1.0})
    {
        const double WingWidth = HalfX - Passage * .5;
        const double Mid = Side * (HalfX - WingWidth * .5);
        const double Back = HalfY - 18;
        const double UpperFace = -HalfY + 108;
        const double Room = FMath::Min(620., (Back - ShopFace) * .58);
        const double RearWall = ShopFace + Room;
        Part(ETint::Terracotta, Mid, (RearWall + Back) * .5, Storey * .5,
            WingWidth, Back - RearWall, Storey, true);
        for (double Edge : {-1., 1.})
        {
            Part(ETint::Plaster, Mid + Edge * (WingWidth*.5-8), ShopFace + Room*.5,
                Storey*.5, 16, Room, Storey, true);
        }
        FitShop(WorldPoint(Mid, ShopFace, 6), Rotation, WingWidth, Room, Storey, Side > 0.);
        Part(ETint::Plaster, Mid, (UpperFace + Back) * .5, (Height + Storey) * .5,
            WingWidth, Back - UpperFace, Height - Storey, true);
        const int32 Bays = FMath::Max(2, FMath::RoundToInt(WingWidth / 385.0));
        const double Bay = WingWidth / Bays;
        for (int32 Floor = 1; Floor <= Storeys; ++Floor)
        {
            const double Z = Storey * Floor;
            Part(ETint::Plaster, Mid, 0, Z, WingWidth + 12, 2 * HalfY, 18, true);
            Part(ETint::Retaining, Mid, -HalfY + 5, Z - 9, WingWidth + 14, 15, 6);
        }
        for (int32 I = 0; I <= Bays; ++I)
        {
            const double X = Mid - WingWidth * .5 + I * Bay;
            Part(ETint::Plaster, X, -HalfY + 23, Storey * .5, 34, 44, Storey, true);
            if (I == Bays)
            {
                continue;
            }
            const double Center = X + Bay * .5;
            const bool Entry = I == Bays / 2;
            if (Entry)
            {
                const bool Tea=Side>0.;
                auto& Visit=Tea?TeaVisit:PrintVisit;
                const FVector Counter=Tea?TeaCounter:PressCounter;
                Visit={WorldPoint(Center,ShopFace-247,6),WorldPoint(Center,ShopFace+104,8.6),
                    WorldPoint(Center,ShopFace+Room-230,8.6),FVector(Counter.X,Counter.Y,Plot.Z+8.6)};
#if !UE_BUILD_SHIPPING
                if (Tea)
                {
                    ShopPath=Visit;
                    for (FVector& P:ShopPath) P.Z=Plot.Z+101.;
                }
#endif
            }
            // Each glazed bay looks through into a real room. The central door opens
            // onto the sidewalk with at least a full standing-capsule width.
            Part(ETint::Timber, X + 7, ShopFace, 120, 14, 13, 240, true);
            if (!Entry)
            {
                Part(ETint::Terracotta, Center, ShopFace, 29, Bay - 12, 17, 58, true);
                Part(ETint::ClearGlass, Center, ShopFace, 152, Bay - 15, 3, 185, true);
                Part(ETint::Timber, Center, ShopFace - 3, 103, Bay - 12, 8, 4);
            }
            else
            {
                Part(ETint::Timber, X + 23, ShopFace + 45, 121, 9, 90, 241, true);
                Part(ETint::Brass, X + 16, ShopFace + 77, 103, 5, 21, 4);
                if (!IsCityMode())
                {
                    AddPractical(WorldPoint(Center, ShopFace + 58, Storey - 21), Yaw + 90.);
                }
            }
            Part(ETint::Timber, Center, ShopFace, 245, Bay, 16, 12, true);
            for (int32 Floor = 1; Floor < Storeys; ++Floor)
            {
                const double Z = Storey * Floor;
                Window(Center, UpperFace, 0, Z + 137, Bay * .72, 207, false);
                Window(Center, Back, 180, Z + 146, Bay * .42, 134, false);
                if ((I + 2 * Floor + (Side > 0 ? 0 : 2)) % 4 == 1)
                {
                    Condenser(Center + Bay * .29, UpperFace, Z + 44);
                }
                // A tiled gallery, slender balusters and a shaded reveal create real parallax.
                Part(ETint::Rail, Center, -HalfY + 8, Z + 107, Bay, 6, 6);
                Part(ETint::Rail, Center, -HalfY + 8, Z + 31, Bay, 5, 5);
                for (int32 Bar = 0; Bar < 5; ++Bar)
                {
                    Part(ETint::Rail, X + (Bar + .5) * Bay / 5, -HalfY + 8, Z + 68, 3, 4, 76);
                }
                if ((I + (Side > 0 ? 1 : 0)) % 3 == 0)
                {
                    // Vertical terracotta sun-breakers at selected bays, clear of the gallery rail.
                    for (int32 Fin = 0; Fin < 5; ++Fin)
                    {
                        Part(ETint::Terracotta, X + 20 + Fin * 22, -HalfY + 40,
                            Z + Storey * .5, 8, 49, Storey - 20);
                    }
                }
            }
            if (I % 3 == 0)
            {
                Window(Center, Back, 180, 110, 94, 204, true);
            }
            else
            {
                Window(Center, Back, 180, 156, Bay * .40, 112, false);
            }
        }
        // Glazed returns and openings into the cross-passage give all four sides a human scale.
        const int32 EndBays = FMath::Max(2, FMath::RoundToInt((Back - ShopFace) / 440.));
        for (int32 J = 0; J < EndBays; ++J)
        {
            const double Y = FMath::Lerp(ShopFace, Back, (J + .5) / EndBays);
            for (int32 Floor = 0; Floor < Storeys; ++Floor)
            {
                const double Z = Floor * Storey + 156;
                Window(Side * HalfX, Y, Side * 90, Z, 136, 131, false);
                Window(Side * Passage * .5, Y, -Side * 90, Z, 108, 131, false);
            }
        }
        Part(ETint::Plaster, Mid, -HalfY + 9, Height + 39, WingWidth, 18, 74);
        Part(ETint::Plaster, Mid, HalfY - 9, Height + 39, WingWidth, 18, 74);
        for (const double X : {Side * HalfX, Side * Passage * .5})
        {
            Part(ETint::Plaster, X, 0, Height + 39, 18, 2 * HalfY, 74);
        }
        const double StairWidth = FMath::Min(420.0, WingWidth * .36);
        Part(ETint::Terracotta, Mid + Side * WingWidth * .16, HalfY * .44,
            Height + 104, StairWidth, 255, 206);
        Part(ETint::Plaster, Mid + Side * WingWidth * .16, HalfY * .44,
            Height + 213, StairWidth + 26, 280, 17);
        Shape(EShape::Cylinder, ETint::Roof, WorldPoint(Mid, HalfY * .46 - 225, Height + 81),
            FVector(122, 122, 151), false, Rotation);
    }

    // Only the short end gallery crosses the alley. The rest is open to daylight.
    Part(ETint::Plaster, 0, -HalfY + 112, Storey, Passage + 26, 224, 22, true);
    Part(ETint::Terracotta, 0, -HalfY + 13, Storey + 60, Passage + 26, 23, 103);
    Part(ETint::Rail, 0, -HalfY + 215, Storey + 109, Passage + 14, 6, 6);

#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("SabarmatiArtReview")))
    {
        const double X = HalfX + 102, Y = HalfY + 102;
        ArcadeReviewRoute = {WorldPoint(0,-Y,102), WorldPoint(0,Y,102), WorldPoint(-X,Y,102),
            WorldPoint(-X,-Y,102), WorldPoint(X,-Y,102), WorldPoint(X,Y,102), WorldPoint(0,Y,102)};
        const FVector Corners[] = {WorldPoint(-X,-Y,102), WorldPoint(X,-Y,102),
            WorldPoint(X,Y,102), WorldPoint(-X,Y,102), WorldPoint(-X,-Y,102),
            WorldPoint(0,-Y,102), WorldPoint(0,Y,102)};
        int32 Clear = 0, Grounded = 0;
        for (int32 I = 1; I < UE_ARRAY_COUNT(Corners); ++I)
        {
            FHitResult Hit;
            Clear += !GetWorld()->SweepSingleByChannel(Hit, Corners[I-1], Corners[I], FQuat::Identity,
                ECC_Pawn, FCollisionShape::MakeCapsule(33,92));
            const int32 Steps = FMath::CeilToInt(FVector::Distance(Corners[I-1], Corners[I]) / 95.);
            bool bAllGround = true;
            for (int32 S = 0; S <= Steps; ++S)
            {
                const FVector At = FMath::Lerp(Corners[I-1], Corners[I], double(S) / Steps);
                bAllGround &= GetWorld()->LineTraceSingleByChannel(Hit, At, At - FVector(0,0,116), ECC_Visibility)
                    && FMath::Abs(Hit.ImpactPoint.Z - (Plot.Z + 6)) < 1.0;
            }
            Grounded += bAllGround;
        }
        FHitResult WallHit;
        const bool bWallBlocks = GetWorld()->LineTraceSingleByChannel(WallHit, WorldPoint(HalfX+80, 0, 110),
            WorldPoint(HalfX-80, 0, 110), ECC_Pawn);
        UE_LOG(LogTemp, Display, TEXT("Sabarmati arcade QA: perimeter and passage clear=%d/6, supported=%d/6, facade blocks=%d."),
            Clear, Grounded, bWallBlocks);
    }
#endif
}

#if !UE_BUILD_SHIPPING
void ARiverfrontWorld::ReviewArcadeWalk(ARiverfrontPlayerController& Driver)
{
    ARiverfrontCharacter* Character = Cast<ARiverfrontCharacter>(Driver.GetPawn());
    if (!Character || ArcadeReviewRoute.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati arcade walk: no playable route."));
        return;
    }
    const FString Prefix = IsCityMode() ? TEXT("city") : TEXT("riverfront");
    const auto Capture = [Prefix](const TCHAR* View)
    {
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ArtReview"),
            Prefix + TEXT("-") + View + TEXT(".png")), false, false);
    };
    const auto Sprint = [&Driver](EInputEvent Event)
    {
        UGameViewportClient* View = Driver.GetWorld()->GetGameViewport();
        Driver.InputKey(FInputKeyEventArgs(View ? View->Viewport : nullptr,
            FInputDeviceId::CreateFromInternalId(0), EKeys::LeftShift, Event, FPlatformTime::Cycles64()));
    };
    const FVector Entry = ArcadeReviewRoute[0];
    Driver.SetControlRotation(FRotator(6, (Entry - Character->GetActorLocation()).Rotation().Yaw, 0));
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this, Walker = TWeakObjectPtr<ARiverfrontCharacter>(Character), &Driver, Capture, Sprint,
            Elapsed = 0., Leg = 0, bOverview = false, bInPassage = false,
            bGrounded = true, Travel = 0., Previous = FVector::ZeroVector](float Dt) mutable
        {
            if (!Walker.IsValid() || Walker->GetController() != &Driver)
            {
                return false;
            }
            Elapsed += Dt;
            if (!bOverview && Elapsed >= 2.)
            {
                Capture(TEXT("arcade-street"));
                bOverview = true;
            }
            if (Elapsed < 3.5)
            {
                return true;
            }
            if (Leg == 0)
            {
                // Place the walker once at the entrance. CharacterMovement handles every leg afterwards.
                const FRotator Heading = (ArcadeReviewRoute[1] - ArcadeReviewRoute[0]).Rotation();
                if (!Walker->TeleportTo(ArcadeReviewRoute[0], Heading))
                {
                    UE_LOG(LogTemp, Warning, TEXT("Sabarmati arcade walk: entry blocked."));
                    return false;
                }
                Driver.SetControlRotation(Heading);
                Sprint(IE_Pressed);
                Previous = Walker->GetActorLocation();
                Leg = 1;
            }
            const FVector Here = Walker->GetActorLocation();
            Travel += FVector::Dist2D(Here, Previous);
            Previous = Here;
            bGrounded &= !Walker->GetCharacterMovement()->IsFalling() || Elapsed < 4.;
            const FVector To = ArcadeReviewRoute[Leg] - Here;
            if (Leg == 1 && !bInPassage && Travel > 530.)
            {
                Capture(TEXT("arcade-passage"));
                bInPassage = true;
            }
            if (To.Size2D() <= 36.)
            {
                ++Leg;
            }
            if (Leg >= ArcadeReviewRoute.Num() || Elapsed > 150.)
            {
                Sprint(IE_Released);
                UE_LOG(LogTemp, Display, TEXT("Sabarmati arcade walk: reached %d/%d legs on foot, distance %.1f m, grounded=%d."),
                    Leg - 1, ArcadeReviewRoute.Num() - 1, Travel / 100., bGrounded);
                return false;
            }
            const FVector Direction = (ArcadeReviewRoute[Leg] - Here).GetSafeNormal2D();
            Driver.SetControlRotation(FRotator(-3, Direction.Rotation().Yaw, 0));
            Walker->AddMovementInput(Direction);
            return true;
        }));
}
#endif
