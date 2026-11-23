#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RiverfrontPlayerController.generated.h"

class SWidget;
class FReply;
class ACameraActor;

UCLASS()
class SABARMATIPROTOTYPE_API ARiverfrontPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    bool IsShowingMenu() const { return Screen != EScreen::Playing; }

    void AddYawInput(float Value) override;
    void AddPitchInput(float Value) override;

protected:
    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetupInputComponent() override;

private:
    enum class EScreen : uint8 { Title, Playing, Paused };

    EScreen Screen = EScreen::Title;
    TSharedPtr<SWidget> Overlay;
    TWeakObjectPtr<ACameraActor> TitleCamera;
    FTimerHandle OpeningMenuTimer;
    double LookReadyAt = 0.0;
    float MouseSensitivity = 0.30f;
    bool bSensitivityChanged = false;

    FString ProfileSlot = TEXT("SabarmatiPlayerProfileV1");
    FString LocalDisplayName;
    bool bUnreadableProfile = false;
    bool bTravelling = false;

    void SetMouseSensitivity(float Value);
    void LoadLocalProfile();
    FText CreateLocalProfile(const FString& RequestedName);
    bool HasLocalProfile() const { return !LocalDisplayName.IsEmpty(); }

    void ShowTitle();
#if !UE_BUILD_SHIPPING
    void RunRenderingBenchmark();
#endif
    void TogglePauseMenu();
    void CycleDistrictTime();
    void ShowMenu(EScreen Target);
    void ResumeDrive();
    FReply QuitDrive();
    void SaveLookPreference();
    void SwitchFreeRoamMap();
    void RemoveOverlay();
};
