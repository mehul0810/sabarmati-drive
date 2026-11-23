#include "RiverfrontPlayerController.h"
#include "RiverfrontWorld.h"
#include "RiverfrontGameMode.h"
#include "RiverfrontCar.h"
#include "RiverfrontLocalProfile.h"
#include "Kismet/GameplayStatics.h"

#include "Components/InputComponent.h"
#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontPedestrian.h"
#endif
#include "Brushes/SlateDynamicImageBrush.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/HUD.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RiverfrontMenu"

namespace
{
constexpr auto LookSection = TEXT("Sabarmati.Drive.Controls");
constexpr auto LookPreference = TEXT("MouseSensitivity");
constexpr float MinSensitivity = .10f;
constexpr float MaxSensitivity = 1.00f;

float ScaleMouseLook(float Rate, float Sensitivity, float MaxRate)
{
    return FMath::Clamp(Rate, -MaxRate, MaxRate) * Sensitivity;
}

#if !UE_BUILD_SHIPPING
void CheckCorridorRoutes(UWorld& World)
{
    const auto HitsFloor = [&World](double X, double Y, double Z)
    {
        FHitResult Hit;
        return World.LineTraceSingleByChannel(Hit, FVector(X, Y, 160), FVector(X, Y, -145),
            ECC_Visibility) && FMath::Abs(Hit.ImpactPoint.Z - Z) < 5.0;
    };
    int32 Road = 0;
    int32 Walk = 0;
    for (const double X : {-78500.0, -64400.0, -7960.0, -7700.0, 7700.0, 7920.0, 58600.0, 78500.0})
    {
        for (const double Y : {4090.0, -26010.0})
        {
            Road += HitsFloor(X, Y, 0.0);
        }
        for (const double Y : {2350.0, -24270.0})
        {
            Walk += HitsFloor(X, Y, -84.0);
        }
    }
    int32 Bridges = 0;
    for (const double X : {-64400.0, 59310.0})
    {
        for (const double Y : {4090.0, -1800.0, -10960.0, -19500.0, -26010.0})
        {
            Bridges += HitsFloor(X, Y, 0.0);
        }
    }
    int32 OpenSeams = 0;
    for (const double Y : {4090.0, -26010.0})
    {
        for (const double X : {-7750.0, 7750.0})
        {
            FHitResult Hit;
            OpenSeams += !World.SweepSingleByChannel(Hit, FVector(X - 340, Y, 65), FVector(X + 340, Y, 65),
                FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(FVector(164, 82, 38)));
        }
    }
    int32 OpenFootSeams = 0;
    for (const double Y : {2350.0, -24270.0})
    {
        for (const double X : {-7600.0, 7600.0})
        {
            FHitResult Hit;
            OpenFootSeams += !World.SweepSingleByChannel(Hit, FVector(X - 230, Y, 13), FVector(X + 230, Y, 13),
                FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(33, 92));
        }
    }
    UE_LOG(LogTemp, Display,
        TEXT("Sabarmati QA: expanded roads=%d/16, lower walks=%d/16, bridge decks=%d/10, road seams=%d/4, foot seams=%d/4."),
        Road, Walk, Bridges, OpenSeams, OpenFootSeams);
}
#endif

DECLARE_DELEGATE_RetVal_OneParam(FText, FCreateLocalProfile, const FString&);

class SRiverfrontMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRiverfrontMenu) {}
        SLATE_ARGUMENT(bool, IsPause)
        SLATE_ARGUMENT(float, Sensitivity)
        SLATE_ARGUMENT(FString, PlayerName)
        SLATE_ARGUMENT(bool, UnreadableProfile)
        SLATE_ARGUMENT(bool, IsCity)
        SLATE_ARGUMENT(bool, CityAvailable)
        SLATE_ARGUMENT(bool, CityFailed)
        SLATE_EVENT(FSimpleDelegate, OnSwitchMap)
        SLATE_EVENT(FCreateLocalProfile, OnCreateProfile)
        SLATE_EVENT(FOnFloatValueChanged, OnSensitivity)
        SLATE_EVENT(FSimpleDelegate, OnContinue)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        bIsPause = Args._IsPause;
        Continue = Args._OnContinue;
        LookSensitivity = Args._Sensitivity;
        PlayerName = Args._PlayerName;
        bUnreadableProfile = Args._UnreadableProfile;
        CreateProfile = Args._OnCreateProfile;
        bIsCity = Args._IsCity;
        bCityAvailable = Args._CityAvailable;
        bCityFailed = Args._CityFailed;
        SwitchMap = Args._OnSwitchMap;
        if (bUnreadableProfile && NeedsProfile())
        {
            ProfileError = LOCTEXT("ProfileUnreadable", "The saved profile could not be opened. No data was overwritten.");
        }
        SensitivityChanged = Args._OnSensitivity;
        const FLinearColor Paper(.96f, .92f, .85f, 1.f);
        const FLinearColor Muted(.76f, .72f, .67f, 1.f);
        const FLinearColor Accent(.88f, .68f, .43f, 1.f);
        const FString TitleArt = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectContentDir(), TEXT("TitleArtwork/SabarmatiDusk.png")));
        if (!bIsPause && IFileManager::Get().FileExists(*TitleArt))
        {
            Hero = MakeShared<FSlateDynamicImageBrush>(FName(*TitleArt), FVector2D(1672.f, 941.f));
        }
        ChildSlot
        [
            SNew(SOverlay).Clipping(EWidgetClipping::ClipToBoundsAlways)
            + SOverlay::Slot()
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
                .Visibility(Hero ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
                [
                    SNew(SImage).Image(Hero.Get())
                ]
            ]
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(.012f, .022f, .026f, bIsPause ? .54f : .16f))
            ]
            + SOverlay::Slot()
            .HAlign(bIsPause ? HAlign_Center : HAlign_Left).VAlign(VAlign_Center).Padding(38.f, 22.f)
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [
                    SNew(SBox).WidthOverride(540.f)
                    [
                        SNew(SBorder)
                        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .BorderBackgroundColor(FLinearColor(.021f, .025f, .025f, bIsPause ? .95f : .87f))
                        .Padding(FMargin(37.f, 33.f))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
                            [
                                SNew(STextBlock)
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
                                .ColorAndOpacity(Accent)
                                .Text(bIsCity ? LOCTEXT("CityPlace", "AHMEDABAD  /  CITY FREE ROAM")
                                    : LOCTEXT("Place", "AHMEDABAD  /  SABARMATI RIVERFRONT"))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
                            [
                                SNew(STextBlock)
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 44))
                                .ColorAndOpacity(Paper)
                                .Text(bIsPause ? LOCTEXT("Paused", "Drive paused") : LOCTEXT("Title", "Sabarmati Drive"))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 26.f)
                            [
                                SNew(STextBlock)
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                                .ColorAndOpacity(Muted)
                                .AutoWrapText(true)
                                .Text(bIsPause ? LOCTEXT("PausedHelp", "Pick up where you left off.")
                                    : bCityFailed ? LOCTEXT("CityUnavailable", "The offline city data could not be loaded. The riverfront is available.")
                                    : bIsCity ? LOCTEXT("CityHelp", "Drive Ahmedabad’s mapped roads. Scenery and terrain are simplified.")
                                    : LOCTEXT("TitleHelp", "Walk Atal Bridge. Drive the riverfront roads."))
                            ]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SVerticalBox)
                                .Visibility_Lambda([this]() { return NeedsProfile() ? EVisibility::Visible : EVisibility::Collapsed; })
                                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
                                [
                                    SNew(STextBlock)
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
                                    .ColorAndOpacity(Paper)
                                    .Text(LOCTEXT("ProfileLabel", "Create local player profile"))
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 9.f)
                                [
                                    SNew(STextBlock)
                                    .ColorAndOpacity(Muted)
                                    .AutoWrapText(true)
                                    .Text(LOCTEXT("ProfilePrivacy", "Choose a display name to unlock Start. It stays on this Mac. No email or password."))
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 9.f)
                                [
                                    SAssignNew(ProfileNameInput, SEditableTextBox)
                                    .IsEnabled(!bUnreadableProfile)
                                    .SelectAllTextWhenFocused(true)
                                    .Padding(FMargin(9.f, 7.f))
                                    .HintText(LOCTEXT("ProfileNameHint", "Display name (2–24 characters)"))
                                    .OnTextCommitted(this, &SRiverfrontMenu::OnNameCommitted)
                                ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
                                [
                                    SNew(STextBlock)
                                    .AutoWrapText(true)
                                    .ColorAndOpacity(FLinearColor(1.f, .6f, .48f))
                                    .Visibility_Lambda([this]() { return ProfileError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
                                    .Text_Lambda([this]() { return ProfileError; })
                                ]
                                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
                                [
                                    SNew(SButton)
                                    .IsEnabled(!bUnreadableProfile)
                                    .ContentPadding(FMargin(22.f, 10.f))
                                    .ButtonColorAndOpacity(FLinearColor(.46f, .29f, .13f))
                                    .OnClicked(this, &SRiverfrontMenu::OnCreateClicked)
                                    [
                                        SNew(STextBlock)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                                        .ColorAndOpacity(Paper)
                                        .Text(LOCTEXT("CreateProfileButton", "Create profile"))
                                    ]
                                ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
                            [
                                SNew(STextBlock)
                                .Visibility_Lambda([this]() { return !bIsPause && !PlayerName.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
                                .ColorAndOpacity(Accent)
                                .Text_Lambda([this]() { return FText::Format(LOCTEXT("LocalPlayerName", "Local player: {0}"), FText::FromString(PlayerName)); })
                            ]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
                            [
                                SAssignNew(PrimaryButton, SButton)
                                .Visibility_Lambda([this]() { return NeedsProfile() ? EVisibility::Collapsed : EVisibility::Visible; })
                                .ContentPadding(FMargin(32.f, 11.f))
                                .HAlign(HAlign_Center)
                                .ButtonColorAndOpacity(FLinearColor(.46f, .29f, .13f))
                                .ClickMethod(EButtonClickMethod::MouseDown)
                                .OnClicked(this, &SRiverfrontMenu::OnPrimaryClicked)
                                [
                                    SNew(STextBlock)
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
                                    .ColorAndOpacity(Paper)
                                    .Text(bIsPause ? LOCTEXT("Resume", "Resume") : LOCTEXT("Start", "Start"))
                                ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 10.f, 0.f, 0.f)
                            [
                                SNew(SButton)
                                .Visibility_Lambda([this]() { return bCityAvailable && !NeedsProfile() ? EVisibility::Visible : EVisibility::Collapsed; })
                                .ContentPadding(FMargin(12.f, 7.f))
                                .OnClicked_Lambda([this]()
                                {
                                    SwitchMap.ExecuteIfBound();
                                    return FReply::Handled();
                                })
                                [
                                    SNew(STextBlock)
                                    .ColorAndOpacity(Paper)
                                    .Text(bIsCity ? LOCTEXT("SwitchRiverfront", "Return to Riverfront")
                                        : LOCTEXT("SwitchCity", "Explore Ahmedabad"))
                                ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f, 0.f, 0.f)
                            [
                                SNew(STextBlock)
                                .Visibility(bIsCity ? EVisibility::Visible : EVisibility::Collapsed)
                                .ColorAndOpacity(Muted)
                                .Text(LOCTEXT("CityCredit", "© OpenStreetMap contributors  /  www.openstreetmap.org/copyright"))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 17.f, 0.f, 0.f)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 12.f, 0.f)
                                [
                                    SNew(STextBlock)
                                    .ColorAndOpacity(Paper)
                                    .Text(LOCTEXT("SensitivityLabel", "Mouse sensitivity"))
                                ]
                                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
                                [
                                    SNew(SSlider)
                                    .MinValue(MinSensitivity)
                                    .MaxValue(MaxSensitivity)
                                    .StepSize(.05f)
                                    .RequiresControllerLock(false)
                                    .ToolTipText(LOCTEXT("SensitivityHint", "Lower values slow mouse look."))
                                    .Value_Lambda([this]() { return LookSensitivity; })
                                    .OnValueChanged_Lambda([this](float Value)
                                    {
                                        LookSensitivity = Value;
                                        SensitivityChanged.ExecuteIfBound(Value);
                                    })
                                ]
                                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.f, 0.f, 0.f, 0.f)
                                [
                                    SNew(SBox).WidthOverride(43.f)
                                    [
                                        SNew(STextBlock)
                                        .ColorAndOpacity(Paper)
                                        .Text_Lambda([this]() { return FText::AsPercent(LookSensitivity); })
                                    ]
                                ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 0.f)
                            [
                                SNew(STextBlock)
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                                .ColorAndOpacity(Muted)
                                .Margin(FMargin(0.f, 18.f, 0.f, 0.f))
                                .Text(bIsPause ? LOCTEXT("ResumeHint", "Esc to resume")
                                    : LOCTEXT("StartHint", "WASD to move   |   Mouse to look   |   Esc to pause"))
                            ]
                        ]
                    ]
                ]
            ]
        ];
    }

    TSharedPtr<SWidget> WidgetToFocus() const
    {
        if (NeedsProfile())
        {
            return bUnreadableProfile ? nullptr : ProfileNameInput;
        }
        return PrimaryButton;
    }

    void ShowCreatedPlayer(const FString& Name)
    {
        PlayerName = Name;
        ProfileError = FText::GetEmpty();
        if (FSlateApplication::IsInitialized() && PrimaryButton)
        {
            FSlateApplication::Get().SetAllUserFocus(PrimaryButton.ToSharedRef(), EFocusCause::SetDirectly);
        }
    }

    FReply OnKeyDown(const FGeometry&, const FKeyEvent& Event) override
    {
        if (Event.GetKey() == EKeys::Escape)
        {
            if (bIsPause && !Event.IsRepeat())
            {
                Continue.ExecuteIfBound();
            }
            return FReply::Handled();
        }
        return FReply::Unhandled();
    }

private:
    TSharedPtr<FSlateDynamicImageBrush> Hero;

    bool NeedsProfile() const { return !bIsPause && PlayerName.IsEmpty(); }

    FReply OnCreateClicked()
    {
        if (NeedsProfile() && !bUnreadableProfile && ProfileNameInput && CreateProfile.IsBound())
        {
            const FString Proposed = ProfileNameInput->GetText().ToString();
            const FText Error = CreateProfile.Execute(Proposed);
            if (Error.IsEmpty())
            {
                ShowCreatedPlayer(URiverfrontLocalProfile::CleanDisplayName(Proposed));
            }
            else
            {
                ProfileError = Error;
            }
        }
        return FReply::Handled();
    }

    void OnNameCommitted(const FText&, ETextCommit::Type Commit)
    {
        if (Commit == ETextCommit::OnEnter)
        {
            OnCreateClicked();
        }
    }

    FReply OnPrimaryClicked()
    {
        Continue.ExecuteIfBound();
        return FReply::Handled();
    }

    bool bIsPause = false;
    float LookSensitivity = .30f;
    FOnFloatValueChanged SensitivityChanged;
    FSimpleDelegate Continue;
    TSharedPtr<SButton> PrimaryButton;
    TSharedPtr<SEditableTextBox> ProfileNameInput;
    FString PlayerName;
    FText ProfileError;
    FCreateLocalProfile CreateProfile;
    FSimpleDelegate SwitchMap;
    bool bIsCity = false;
    bool bCityAvailable = false;
    bool bCityFailed = false;
    bool bUnreadableProfile = false;
};
} // namespace

void ARiverfrontPlayerController::BeginPlay()
{
    Super::BeginPlay();
    float Saved = MouseSensitivity;
    if (GConfig && GConfig->GetFloat(LookSection, LookPreference, Saved, GGameUserSettingsIni) && FMath::IsFinite(Saved))
    {
        MouseSensitivity = FMath::Clamp(Saved, MinSensitivity, MaxSensitivity);
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati controls: look sensitivity=%.2f; 10 input units -> %.2f yaw, %.2f pitch."),
        MouseSensitivity, ScaleMouseLook(10.f, MouseSensitivity, 45.f), ScaleMouseLook(10.f, MouseSensitivity, 30.f));
    if (PlayerCameraManager)
    {
        PlayerCameraManager->ViewPitchMin = -60.0f;
        PlayerCameraManager->ViewPitchMax = 45.0f;
    }
    if (IsLocalController() && FApp::CanEverRender() && GetWorld()->GetGameViewport())
    {
#if !UE_BUILD_SHIPPING
        if (FParse::Param(FCommandLine::Get(), TEXT("SabarmatiArtReview")))
        {
            // Protected QA uses a fresh slot; never populate or touch the real player's profile.
            FGuid TestId;
            const TCHAR* Existing = GetWorld()->URL.GetOption(TEXT("SabarmatiQAProfile="), TEXT(""));
            if (!FGuid::ParseExact(Existing, EGuidFormats::Digits, TestId))
            {
                TestId = FGuid::NewGuid();
            }
            ProfileSlot = TEXT("SabarmatiQA-") + TestId.ToString(EGuidFormats::Digits);
        }
#endif
        LoadLocalProfile();
        // Let the standalone viewport and pawn finish initialization before capturing UI input.
        OpeningMenuTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ARiverfrontPlayerController::ShowTitle);
    }
    else
    {
        Screen = EScreen::Playing;
    }
}

void ARiverfrontPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(OpeningMenuTimer);
    RemoveOverlay();
    Super::EndPlay(Reason);
}

void ARiverfrontPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction(TEXT("PauseDrive"), IE_Pressed, this, &ARiverfrontPlayerController::TogglePauseMenu);
}

void ARiverfrontPlayerController::ShowTitle()
{
    if (const UGameViewportClient* View = GetWorld()->GetGameViewport(); View && View->Viewport)
    {
        IConsoleManager& Settings = IConsoleManager::Get();
        if (FParse::Param(FCommandLine::Get(), TEXT("Sabarmati4K")))
        {
            // This launch profile renders the full viewport, not a lower-resolution upscale.
            if (IConsoleVariable* Fraction = Settings.FindConsoleVariable(TEXT("r.ScreenPercentage")))
            {
                Fraction->Set(100.f, ECVF_SetByCommandline);
            }
            // A Retina-size drawable is intentional; Slate's scale is for UI,
            // not a request to render the scene at half the viewport resolution.
            if (IConsoleVariable* Secondary = Settings.FindConsoleVariable(
                    TEXT("r.SecondaryScreenPercentage.GameViewport")))
            {
                Secondary->Set(100.f, ECVF_SetByCommandline);
            }
            if (IConsoleVariable* Dynamic = Settings.FindConsoleVariable(TEXT("r.DynamicRes.OperationMode")))
            {
                Dynamic->Set(0, ECVF_SetByCommandline);
            }
        }
        const FIntPoint Output = View->Viewport->GetSizeXY();
        const IConsoleVariable* Fraction = Settings.FindConsoleVariable(TEXT("r.ScreenPercentage"));
        UE_LOG(LogTemp, Display, TEXT("Sabarmati video: viewport=%dx%d, screen percentage=%.0f."),
            Output.X, Output.Y, Fraction ? Fraction->GetFloat() : -1.f);
    }

    FVector ViewLocation(-3980, 2650, 356);
    FRotator ViewRotation(-7, -111, 0);
    if (const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>(); Mode && Mode->IsCityMode())
    {
        const FRotator Heading(0.0, Mode->GetSpawnYaw(), 0.0);
        ViewLocation = Mode->GetWalkSpawn() + Heading.RotateVector(FVector(-1000.0, -580.0, 690.0));
        ViewRotation = FRotator(-10.0, Mode->GetSpawnYaw() + 14.0, 0.0);
    }
    if (ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(ViewLocation, ViewRotation))
    {
        Camera->GetCameraComponent()->SetFieldOfView(72.f);
        TitleCamera = Camera;
    }
    ShowMenu(EScreen::Title);
    if (TitleCamera.IsValid() && Screen == EScreen::Title)
    {
        SetViewTarget(TitleCamera.Get());
    }
#if !UE_BUILD_SHIPPING
    // Opt-in Metal capture for asset review. Core ticker continues while the title pauses the world.
    if (Screen == EScreen::Title && FParse::Param(FCommandLine::Get(), TEXT("SabarmatiArtReview")))
    {
        const FString CaptureDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ArtReview"));
        IFileManager::Get().MakeDirectory(*CaptureDir, true);
        const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
        const bool bReviewCity = FParse::Param(FCommandLine::Get(), TEXT("SabarmatiCityReview"));
        const bool bCityWorld = Mode && Mode->IsCityMode();
        FVector CityDriveOrigin = FVector::ZeroVector;
        int32 Step = -2;
        TArray<TPair<TWeakObjectPtr<ARiverfrontPedestrian>, FVector>> CrowdAtTitle;
        FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateWeakLambda(this, [this, CaptureDir, Step, CrowdAtTitle, bReviewCity, bCityWorld, CityDriveOrigin](float) mutable
            {
                if (bCityWorld)
                {
                    const auto DriveKey = [this](EInputEvent Event)
                    {
                        UGameViewportClient* View = GetWorld()->GetGameViewport();
                        InputKey(FInputKeyEventArgs(View ? View->Viewport : nullptr,
                            FInputDeviceId::CreateFromInternalId(0), EKeys::W, Event, FPlatformTime::Cycles64()));
                    };
                    switch (Step++)
                    {
                    case -2:
                        UE_LOG(LogTemp, Display, TEXT("Sabarmati city QA: profile carried across travel=%d."),
                            HasLocalProfile() && IsPaused());
                        FScreenshotRequest::RequestScreenshot(
                            FPaths::Combine(CaptureDir, TEXT("10-city-title.png")), true, false);
                        return true;
                    case -1:
                        ResumeDrive();
                        return true;
                    case 0:
                        if (const APawn* Pawn = GetPawn())
                        {
                            const auto* GameMode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
                            const ARiverfrontCar* Car = GameMode ? GameMode->GetCar() : nullptr;
                            FHitResult Ground;
                            const FVector Location = Pawn->GetActorLocation();
                            const FVector CarLocation = Car ? Car->GetActorLocation() : FVector::ZeroVector;
                            const bool bWalkingGround = GetWorld()->LineTraceSingleByChannel(Ground,
                                Location, Location - FVector(0, 0, 200), ECC_Visibility);
                            const bool bCarGround = Car && GetWorld()->LineTraceSingleByChannel(Ground,
                                CarLocation, CarLocation - FVector(0, 0, 180), ECC_Visibility);
                            UE_LOG(LogTemp, Display, TEXT("Sabarmati city QA: pawn grounded=%d, car surface=%d, active=%d."),
                                bWalkingGround, bCarGround, Screen == EScreen::Playing);
                        }
                        FScreenshotRequest::RequestScreenshot(
                            FPaths::Combine(CaptureDir, TEXT("11-city-street.png")), true, false);
                        return true;
                    case 1:
                        TogglePauseMenu();
                        return true;
                    case 2:
                        FScreenshotRequest::RequestScreenshot(
                            FPaths::Combine(CaptureDir, TEXT("12-city-paused.png")), true, false);
                        return true;
                    case 3:
                        ResumeDrive();
                        if (auto* Character = Cast<ARiverfrontCharacter>(GetPawn()))
                        {
                            if (const auto* GameMode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>())
                            {
                                if (ARiverfrontCar* Car = GameMode->GetCar())
                                {
                                    CityDriveOrigin = Car->GetActorLocation();
                                    Car->Enter(*Character);
                                }
                            }
                        }
                        return true;
                    case 4:
                        DriveKey(IE_Pressed);
                        return true;
                    case 5:
                        DriveKey(IE_Released);
                        if (const auto* Car = Cast<ARiverfrontCar>(GetPawn()))
                        {
                            UE_LOG(LogTemp, Display, TEXT("Sabarmati city QA: drove %.1f m, speed=%.1f km/h."),
                                FVector::Dist2D(Car->GetActorLocation(), CityDriveOrigin) / 100.0, Car->GetSpeedKph());
                            FScreenshotRequest::RequestScreenshot(FPaths::Combine(CaptureDir,
                                TEXT("13-city-driving.png")), true, false);
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Sabarmati city QA: could not enter parked car."));
                        }
                        return true;
                    case 6:
                        if (const APawn* Pawn = GetPawn())
                        {
                            if (auto* HUD = GetHUD())
                            {
                                HUD->bShowHUD = false;
                            }
                            const FVector Birdseye = Pawn->GetActorLocation() + FVector(9000, -17000, 81500);
                            if (ACameraActor* View = GetWorld()->SpawnActor<ACameraActor>(Birdseye, FRotator(-84, 0, 0)))
                            {
                                View->GetCameraComponent()->SetFieldOfView(75.f);
                                SetViewTarget(View);
                            }
                        }
                        return true;
                    case 7:
                        FScreenshotRequest::RequestScreenshot(
                            FPaths::Combine(CaptureDir, TEXT("14-city-road-layout.png")), false, false);
                        return true;
                    case 8:
                        UE_LOG(LogTemp, Display, TEXT("Sabarmati city QA: review complete."));
                        return false;
                    default:
                        return false;
                    }
                }
                switch (Step++)
                {
                case -2:
                    ResumeDrive();
                    UE_LOG(LogTemp, Display, TEXT("Sabarmati profile QA: no-profile Start blocked=%d."),
                        Screen == EScreen::Title && IsPaused());
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("00-create-local-profile.png")), true, false);
                    return true;
                case -1:
                    {
                        const bool bReject = !CreateLocalProfile(TEXT("  ")).IsEmpty() && !HasLocalProfile();
                        const bool bCreated = CreateLocalProfile(TEXT("Test Driver")).IsEmpty();
                        const bool bNoOverwrite = !CreateLocalProfile(TEXT("Replacement")).IsEmpty();
                        LoadLocalProfile();
                        const bool bReloaded = LocalDisplayName == TEXT("Test Driver");
                        if (bReloaded && Overlay)
                        {
                            StaticCastSharedPtr<SRiverfrontMenu>(Overlay)->ShowCreatedPlayer(LocalDisplayName);
                        }
                        UE_LOG(LogTemp, Display, TEXT("Sabarmati profile QA: invalid rejected=%d, created=%d, reloaded=%d, no overwrite=%d."),
                            bReject, bCreated, bReloaded, bNoOverwrite);
                    }
                    return true;
                case 0:
                    if (UWorld* World = GetWorld())
                    {
                        constexpr double BridgeCenterX = 3150.0;
                        int32 ConnectedDeckSamples = 0;
                        for (const double Y : {4200.0, -1000.0, -10960.0, -21200.0, -26120.0})
                        {
                            FHitResult Hit;
                            if (World->LineTraceSingleByChannel(Hit, FVector(BridgeCenterX, Y, 100),
                                FVector(BridgeCenterX, Y, -80), ECC_Visibility) && FMath::Abs(Hit.ImpactPoint.Z) < 2.0)
                            {
                                ++ConnectedDeckSamples;
                            }
                        }
                        UE_LOG(LogTemp, Display, TEXT("Sabarmati QA: %d/5 deck surface samples hit."),
                            ConnectedDeckSamples);
                        int32 AtalDeckSamples = 0;
                        for (const double Y : {3220.0, 2510.0, 0.0, -10960.0, -22000.0, -25140.0})
                        {
                            FHitResult Hit;
                            if (World->LineTraceSingleByChannel(Hit, FVector(ARiverfrontWorld::AtalX, Y, 95),
                                FVector(ARiverfrontWorld::AtalX, Y, -25), ECC_Visibility) &&
                                FMath::Abs(Hit.ImpactPoint.Z - 23.0) < 1.0)
                            {
                                ++AtalDeckSamples;
                            }
                        }
                        FHitResult ApproachHit;
                        const bool bApproachBlocked = World->SweepSingleByChannel(ApproachHit,
                            FVector(ARiverfrontWorld::AtalX, 3550, 145),
                            FVector(ARiverfrontWorld::AtalX, 2400, 145), FQuat::Identity,
                            ECC_Pawn, FCollisionShape::MakeCapsule(33, 92));
                        FHitResult BollardHit;
                        const bool bWideVehicleBlocked = World->SweepSingleByChannel(BollardHit,
                            FVector(ARiverfrontWorld::AtalX, 3550, 82),
                            FVector(ARiverfrontWorld::AtalX, 3200, 82), FQuat::Identity,
                            ECC_Pawn, FCollisionShape::MakeBox(FVector(84, 185, 45)));
                        UE_LOG(LogTemp, Display,
                            TEXT("Sabarmati QA: Atal deck=%d/6, foot approach blocked=%d, car approach blocked=%d."),
                            AtalDeckSamples, bApproachBlocked, bWideVehicleBlocked);
                        CheckCorridorRoutes(*World);
                        for (TActorIterator<ARiverfrontPedestrian> It(World); It; ++It)
                        {
                            CrowdAtTitle.Emplace(TWeakObjectPtr<ARiverfrontPedestrian>(*It), It->GetActorLocation());
                        }
                    }
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("01-title.png")), true, false);
                    return true;
                case 1:
                    if (bReviewCity)
                    {
                        if (ARiverfrontWorld::HasCityData() && HasLocalProfile())
                        {
                            UE_LOG(LogTemp, Display, TEXT("Sabarmati city QA: switching via the profile-gated map action."));
                            SwitchFreeRoamMap();
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Sabarmati city QA: missing city map or profile."));
                        }
                        return false;
                    }
                    ResumeDrive();
                    return true;
                case 2:
                    {
                        int32 CrowdMoved = 0;
                        for (const auto& Entry : CrowdAtTitle)
                        {
                            if (const ARiverfrontPedestrian* Pedestrian = Entry.Key.Get())
                            {
                                CrowdMoved += FVector::DistSquared(Pedestrian->GetActorLocation(), Entry.Value) > FMath::Square(25.0);
                            }
                        }
                        const FRotator PriorRotation = RotationInput;
                        const float CurrentSensitivity = MouseSensitivity;
                        RotationInput = FRotator::ZeroRotator;
                        AddYawInput(10.f);
                        AddPitchInput(-10.f);
                        const FRotator SlowLook = RotationInput;
                        RotationInput = FRotator::ZeroRotator;
                        MouseSensitivity = 1.0f;
                        AddYawInput(10.f);
                        AddPitchInput(-10.f);
                        const FRotator OriginalLook = RotationInput;
                        MouseSensitivity = CurrentSensitivity;
                        RotationInput = PriorRotation;
                        UE_LOG(LogTemp, Display,
                            TEXT("Sabarmati QA: pedestrians moved=%d/%d; mouse yaw %.2f vs %.2f; pitch %.2f vs %.2f."),
                            CrowdMoved, CrowdAtTitle.Num(), SlowLook.Yaw, OriginalLook.Yaw, SlowLook.Pitch, OriginalLook.Pitch);
                    }
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("02-on-foot.png")), false, false);
                    return true;
                case 3:
                    SetControlRotation(FRotator(-7, 48, 0));
                    return true;
                case 4:
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("03-street.png")), false, false);
                    return true;
                case 5:
                    TogglePauseMenu();
                    return true;
                case 6:
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("04-paused.png")), true, false);
                    return true;
                case 7:
                    ResumeDrive();
                    if (AHUD* HUD = GetHUD())
                    {
                        HUD->bShowHUD = false;
                    }
                    if (ACameraActor* View = GetWorld()->SpawnActor<ACameraActor>(
                        FVector(-3650, 2355, 94), FRotator(7, -128, 0)))
                    {
                        View->GetCameraComponent()->SetFieldOfView(80.f);
                        SetViewTarget(View);
                    }
                    return true;
                case 8:
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("05-atal-promenade.png")), false, false);
                    return true;
                case 9:
                    if (ACameraActor* View = GetWorld()->SpawnActor<ACameraActor>(
                        FVector(ARiverfrontWorld::AtalX + 120, 3060, 197), FRotator(7, -92, 0)))
                    {
                        View->GetCameraComponent()->SetFieldOfView(76.f);
                        SetViewTarget(View);
                    }
                    return true;
                case 10:
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("06-atal-walkway.png")), false, false);
                    return true;
                case 11:
                    if (ACameraActor* View = GetWorld()->SpawnActor<ACameraActor>(
                        FVector(19600, 4720, 450), FRotator(-12, 80, 0)))
                    {
                        View->GetCameraComponent()->SetFieldOfView(77.f);
                        SetViewTarget(View);
                    }
                    return true;
                case 12:
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("07-flower-park.png")), false, false);
                    return true;
                case 13:
                    if (ACameraActor* View = GetWorld()->SpawnActor<ACameraActor>(
                        FVector(59800, 2800, 340), FRotator(0, -100, 0)))
                    {
                        View->GetCameraComponent()->SetFieldOfView(79.f);
                        SetViewTarget(View);
                    }
                    return true;
                case 14:
                    FScreenshotRequest::RequestScreenshot(
                        FPaths::Combine(CaptureDir, TEXT("08-ellis-bridge.png")), false, false);
                    return true;
                case 15:
                    if (UWorld* World = GetWorld())
                    {
                        for (TActorIterator<ARiverfrontPedestrian> It(World); It; ++It)
                        {
                            const FVector Person = It->GetActorLocation();
                            if (FMath::Abs(Person.X - ARiverfrontWorld::AtalX) > 270 ||
                                Person.Y > 2800 || Person.Y < -4200)
                            {
                                continue;
                            }
                            const FRotator Heading(0, It->GetActorRotation().Yaw - 90.0, 0);
                            const FVector Offset = Heading.RotateVector(FVector(415, 124, 143));
                            const FVector EyeLevel = Person + FVector(0, 0, 99);
                            if (ACameraActor* View = World->SpawnActor<ACameraActor>(Person + Offset,
                                (EyeLevel - Person - Offset).Rotation()))
                            {
                                View->GetCameraComponent()->SetFieldOfView(68.f);
                                SetViewTarget(View);
                                PlayerCameraManager->UpdateCamera(0.f);
                                FScreenshotRequest::RequestScreenshot(
                                    FPaths::Combine(CaptureDir, TEXT("09-pedestrian.png")), false, false);
                            }
                            break;
                        }
                    }
                    return true;
                case 16:
                    UE_LOG(LogTemp, Display, TEXT("Sabarmati art review: capture sequence complete."));
                    return false;
                default:
                    return false;
                }
            }), 4.0f);
    }
#endif
}

void ARiverfrontPlayerController::AddYawInput(float Value)
{
    if (!IsShowingMenu() && FPlatformTime::Seconds() >= LookReadyAt && FMath::IsFinite(Value))
    {
        Super::AddYawInput(ScaleMouseLook(Value, MouseSensitivity, 45.0f));
    }
}

void ARiverfrontPlayerController::AddPitchInput(float Value)
{
    if (!IsShowingMenu() && FPlatformTime::Seconds() >= LookReadyAt && FMath::IsFinite(Value))
    {
        Super::AddPitchInput(ScaleMouseLook(Value, MouseSensitivity, 30.0f));
    }
}

void ARiverfrontPlayerController::LoadLocalProfile()
{
    LocalDisplayName.Reset();
    bUnreadableProfile = false;
    if (!UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0))
    {
        return;
    }
    const auto* Saved = Cast<URiverfrontLocalProfile>(UGameplayStatics::LoadGameFromSlot(ProfileSlot, 0));
    if (IsValid(Saved) && Saved->Schema == URiverfrontLocalProfile::CurrentSchema)
    {
        LocalDisplayName = URiverfrontLocalProfile::CleanDisplayName(Saved->DisplayName);
    }
    bUnreadableProfile = LocalDisplayName.IsEmpty();
}

FText ARiverfrontPlayerController::CreateLocalProfile(const FString& RequestedName)
{
    if (bUnreadableProfile || HasLocalProfile() || UGameplayStatics::DoesSaveGameExist(ProfileSlot, 0))
    {
        return LOCTEXT("ProfileNotOverwritten", "A local profile already exists. No data was overwritten.");
    }
    const FString Clean = URiverfrontLocalProfile::CleanDisplayName(RequestedName);
    if (Clean.IsEmpty())
    {
        return LOCTEXT("ProfileInvalidName", "Enter a display name with 2 to 24 characters on one line.");
    }
    URiverfrontLocalProfile* NewProfile = NewObject<URiverfrontLocalProfile>();
    NewProfile->DisplayName = Clean;
    if (!UGameplayStatics::SaveGameToSlot(NewProfile, ProfileSlot, 0))
    {
        return LOCTEXT("ProfileSaveFailed", "Could not save the profile on this Mac. Please try again.");
    }
    LocalDisplayName = Clean;
    return FText::GetEmpty();
}

void ARiverfrontPlayerController::SetMouseSensitivity(float Value)
{
    if (FMath::IsFinite(Value))
    {
        MouseSensitivity = FMath::Clamp(Value, MinSensitivity, MaxSensitivity);
        bSensitivityChanged = true;
    }
}

void ARiverfrontPlayerController::TogglePauseMenu()
{
    if (Screen == EScreen::Playing)
    {
        ShowMenu(EScreen::Paused);
    }
}

void ARiverfrontPlayerController::ShowMenu(EScreen Target)
{
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport)
    {
        return;
    }
    if (!IsPaused() && !SetPause(true))
    {
        // Keep the profile gate and UI-only input even if the local pause request fails.
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati menu could not pause the local world."));
    }

    Screen = Target;
    UE_LOG(LogTemp, Display, TEXT("Sabarmati menu: %s; world paused=%d"),
        Screen == EScreen::Title ? TEXT("Title") : TEXT("Pause"), IsPaused());
    FlushPressedKeys();
    const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    const bool bCity = Mode && Mode->IsCityMode();
    const bool bCityFailed = Mode && Mode->CityFailed();
    TSharedRef<SRiverfrontMenu> Menu = SNew(SRiverfrontMenu)
        .IsPause(Target == EScreen::Paused)
        .PlayerName(LocalDisplayName)
        .IsCity(bCity)
        .CityAvailable(bCity || (!bCityFailed && ARiverfrontWorld::HasCityData()))
        .CityFailed(bCityFailed)
        .OnSwitchMap(FSimpleDelegate::CreateUObject(this, &ARiverfrontPlayerController::SwitchFreeRoamMap))
        .UnreadableProfile(bUnreadableProfile)
        .OnCreateProfile(FCreateLocalProfile::CreateUObject(this, &ARiverfrontPlayerController::CreateLocalProfile))
        .Sensitivity(MouseSensitivity)
        .OnSensitivity(FOnFloatValueChanged::CreateUObject(this, &ARiverfrontPlayerController::SetMouseSensitivity))
        .OnContinue(FSimpleDelegate::CreateUObject(this, &ARiverfrontPlayerController::ResumeDrive));
    Overlay = Menu;
    Viewport->AddViewportWidgetContent(Menu, 20);
    bShowMouseCursor = true;
    FInputModeUIOnly MenuInput;
    const TSharedPtr<SWidget> Focus = Menu->WidgetToFocus();
    if (Focus)
    {
        MenuInput.SetWidgetToFocus(Focus);
    }
    MenuInput.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(MenuInput);
    if (Focus && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetAllUserFocus(Focus.ToSharedRef(), EFocusCause::SetDirectly);
    }
}

void ARiverfrontPlayerController::ResumeDrive()
{
    if (Screen == EScreen::Playing || (Screen == EScreen::Title && !HasLocalProfile()))
    {
        return;
    }
    // Read world state, not just the return code (the world may already be unpaused).
    if (IsPaused())
    {
        SetPause(false);
    }
    if (IsPaused())
    {
        UE_LOG(LogTemp, Warning, TEXT("Sabarmati menu: resume requested but world remains paused."));
        return;
    }

    const bool bFromTitle = Screen == EScreen::Title;
    Screen = EScreen::Playing;
    if (bFromTitle && GetPawn())
    {
        SetViewTarget(GetPawn());
        SetControlRotation(FRotator(-7.0, GetPawn()->GetActorRotation().Yaw, 0.0));
    }
    if (TitleCamera.IsValid())
    {
        TitleCamera->Destroy();
        TitleCamera.Reset();
    }
    UE_LOG(LogTemp, Display, TEXT("Sabarmati menu: %s; world paused=%d"),
        bFromTitle ? TEXT("Start") : TEXT("Resume"), IsPaused());
    RemoveOverlay();
    bShowMouseCursor = false;
    if (GConfig && bSensitivityChanged)
    {
        GConfig->SetFloat(LookSection, LookPreference, MouseSensitivity, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
        bSensitivityChanged = false;
    }
    FlushPressedKeys();
    // Cursor recentering as Slate hands focus back can produce a spurious relative delta.
    LookReadyAt = FPlatformTime::Seconds() + 0.25;
    FInputModeGameOnly GameInput;
    GameInput.SetConsumeCaptureMouseDown(true);
    SetInputMode(GameInput);
}

void ARiverfrontPlayerController::SwitchFreeRoamMap()
{
    if (bTravelling || !HasLocalProfile() || !IsShowingMenu())
    {
        return;
    }
    const auto* Mode = GetWorld()->GetAuthGameMode<ARiverfrontGameMode>();
    const bool bCity = Mode && Mode->IsCityMode();
    if (!bCity && !ARiverfrontWorld::HasCityData())
    {
        return;
    }
    bTravelling = true;
    SetPause(false);
    FString Options = bCity ? TEXT("SabarmatiRiverfront") : TEXT("SabarmatiCity");
#if !UE_BUILD_SHIPPING
    if (ProfileSlot.StartsWith(TEXT("SabarmatiQA-")))
    {
        Options += TEXT("?SabarmatiQAProfile=") + ProfileSlot.Right(32);
    }
#endif
    UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true, Options);
}

void ARiverfrontPlayerController::RemoveOverlay()
{
    if (Overlay.IsValid())
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
        }
        Overlay.Reset();
    }
}

#undef LOCTEXT_NAMESPACE
