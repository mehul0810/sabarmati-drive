#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RiverfrontResidentAppearance.h"
#include "RiverfrontPlayerController.generated.h"

class SWidget;
class FReply;
class ACameraActor;
class ALevelSequenceActor;
enum class ERiverfrontLifeMenu : uint8;
enum class EFittingView : uint8 { Full, Face, Tailoring, Trousers, Feet };
class ARiverfrontWorld;
class ARiverfrontPedestrian;

enum class ERiverSiteKind : uint8 { Visit, Tea, Print, Service, Wardrobe, ClothesShop, FittingRoom };
struct FRiverSite
{
    FName Id;
    FString Name, Action, Discovery;
    FVector Point;
    ERiverSiteKind Kind;
    float Reach;
    TOptional<double> StreetHeading;
};

struct FRiverfrontInteractionPrompt
{
    FString Action, Subject;
    FName Binding;
    TOptional<FVector> Anchor;
};

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    bool IsShowingMenu() const { return Screen != EScreen::Playing; }
    void RefreshWardrobeMirror();
    bool SetFittingView(EFittingView Mode);

    void InteractWithPlace();
    void Interact();
    void OpenFocusedVehicleDoor();
    void Drowned();
    void NotifyResidentReaction(const ARiverfrontPedestrian& Person, int32 Count);
    FString PlacePrompt() const;
    FRiverfrontInteractionPrompt UpdateInteractionPrompt();
    const TArray<FRiverSite>& GetPlaces() const { return Sites; }
    const FRiverSite* Destination() const;
    FVector NextWaypoint() const;
    bool IsWaking() const { return bWaking; }
    FString NavigationText() const;
    FString ActivityText() const;
    bool MapVisible() const { return bMiniMap; }
    bool ControlsVisible() const { return bControls; }
    int32 Discovered() const { return Visited.Num(); }
    virtual void PlayerTick(float DeltaSeconds) override;
#if !UE_BUILD_SHIPPING
    bool VerifyReviewProfile(float& RecordedTime) const;
#endif

    void AddYawInput(float Value) override;
    void AddPitchInput(float Value) override;

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetupInputComponent() override;

private:
    enum class EScreen : uint8 { Title, Playing, Paused, Life };

    EScreen Screen = EScreen::Title;
    TSharedPtr<SWidget> Overlay;
    TWeakObjectPtr<ACameraActor> TitleCamera;
    FTimerHandle OpeningMenuTimer;
    double LookReadyAt = 0.0;
    float MouseSensitivity = 0.30f, MasterVolume=1.f;
    void ApplyVolume(float V);
    bool bSensitivityChanged = false;

    FString ProfileSlot = TEXT("SabarmatiPlayerProfileV1");
    FString LocalDisplayName;
    bool bUnreadableProfile = false;
    bool bTravelling = false;

    TArray<FRiverSite> Sites;
    TArray<FName> Visited;
    TArray<FVector> Checkpoints;
    int32 SelectedSite=INDEX_NONE, NextCheckpoint=INDEX_NONE;
    bool bMiniMap=true, bControls=false, bCitySites=false;
    float BestTrial=0.f;
    double TrialStart=0., MessageUntil=0.;
    FString InteractionMessage;
    enum class EInteraction : uint8 { None, Bed, Place, Pedestrian, ParkedCar, Traffic };
    struct FInteractionTarget
    {
        EInteraction Kind = EInteraction::None;
        TWeakObjectPtr<AActor> Actor;
        int32 Site = INDEX_NONE;
        FVector Anchor = FVector::ZeroVector;

        bool Matches(const FInteractionTarget& Other) const
        {
            return Kind==Other.Kind && Actor==Other.Actor && Site==Other.Site;
        }
    };
    FInteractionTarget FindInteraction() const;
    FInteractionTarget ConfirmInteraction() const;
    FRiverfrontInteractionPrompt DescribeInteraction(const FInteractionTarget& Target) const;
    void UseInteraction(const FInteractionTarget& Target, bool bPlaceOnly);
    FInteractionTarget ShownInteraction;
    TWeakObjectPtr<APawn> ShownInteractionPawn;
    FVector ShownInteractionLocation=FVector::ZeroVector;
    uint64 ShownInteractionFrame=0;
    void BeginLifeSession();
    void FinishWakeup();
    void PlayWakeup();
    bool CanRestAtBed() const;
    void SaveLifeCheckpoint();
    void CommitLifeCheckpoint(const FVector& Location, float Yaw);
    void OpenWallet();
    void OpenLifeMenu(ERiverfrontLifeMenu Page);
    void CloseLifeMenu();
    void RefreshLifeAppearance();
    void PrepareFittingCamera();
    bool OpenWardrobeMirror(bool bAtShop=false);
    TWeakObjectPtr<class USceneCaptureComponent2D> MirrorCapture;
    FVector BeforeMirrorLocation = FVector::ZeroVector;
    EFittingView FittingView = EFittingView::Full;
    uint32 MirrorUpdates = 0;
    uint64 LastMirrorFrame = MAX_uint64;
    bool UpdatePrintDelivery(ERiverSiteKind Kind);
    void PointToChai();
    void GreetCompanion(ARiverfrontPedestrian& Person);
    TArray<TWeakObjectPtr<ARiverfrontPedestrian>> Neighbours;
    bool HasLifeCheckpoint() const;
    bool bLifeActive = false;
    bool bParcelInHand = false;
    bool bParcelDelivered = false;
    bool bWaking = false;
    bool bHasExperiencedWake = false;
    bool bHaveLastCheckpoint = false;
    FVector LastLifeLocation = FVector::ZeroVector;
    float LastLifeYaw = 0.f;
    FRotator BeforeFittingRotation = FRotator::ZeroRotator;
    FTransform FullOutfitView;
    FTimerHandle WakeupTimer, QuiltTimer, LifeSaveTimer;
    TWeakObjectPtr<ACameraActor> LifeCamera;
    TWeakObjectPtr<ALevelSequenceActor> WakeupSequence;
    void LoadSites();
    void CycleDestination();
    void TravelToDestination();
    void ToggleMap() { bMiniMap=!bMiniMap; }
    void ToggleControls() { bControls=!bControls; }
    void ToggleChallenge();
    bool SaveExploration();
    void Notice(const FString& Text);
    void SetMouseSensitivity(float Value);
    void LoadLocalProfile();
    FText CreateLocalProfile(const FString& RequestedName, ERiverfrontGender Gender = ERiverfrontGender::Male);
    bool HasLocalProfile() const { return !LocalDisplayName.IsEmpty(); }

    void ShowTitle();
    void ApplyGraphicsPreset(int32 Quality, bool bSave);
#if !UE_BUILD_SHIPPING
    void RunMorningReview();
    void RunAwarenessReview();
    void RunErrandReview();
    void RunTrafficFlowReview();
    void RunNavigationReview();
    void RunRenderingBenchmark();
    void RunDistrictReview();
    void RunGaitReview();
    void RunSystemsReview();
    void RunGrandCityReview();
    void RunRiverReview();
    void RunImpactReview();
    void RunNeighbourhoodReview();
    int32 ReviewGraphicsPreset = INDEX_NONE;
    double LastStreetTravelMs=0.;
#endif
    void TogglePauseMenu();
    void CycleDistrictTime();
    void CycleDistrictWeather();
    void ToggleDistrictClock();
    void ShowMenu(EScreen Target);
    void ResumeDrive();
    FReply QuitDrive();
    void SaveLookPreference();
    void SwitchFreeRoamMap();
    void ReturnHome();
    void RestartAfterDrowning();
    void LoadFreeRoamMap(FString Options);
    void RemoveOverlay();
};
