#include "Modules/ModuleManager.h"
#include "DistrictLoadProgress.h"
#include "MoviePlayer.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
TAtomic<int32> ReadySteps(0);
constexpr int32 TotalSteps=6;
}

namespace DistrictLoadProgress
{
void Reached(int Completed) { ReadySteps.Store(FMath::Clamp(Completed,0,TotalSteps)); }
}

class FSabarmatiModule final : public FDefaultGameModuleImpl
{
    FDelegateHandle MapLoad;
public:
    void StartupModule() override
    {
        MapLoad=FCoreUObjectDelegates::PreLoadMap.AddRaw(this,&FSabarmatiModule::ShowLoad);
    }
    void ShutdownModule() override { FCoreUObjectDelegates::PreLoadMap.Remove(MapLoad); }
    void ShowLoad(const FString&)
    {
        DistrictLoadProgress::Reached(0);
        if (IsRunningCommandlet() || IsRunningDedicatedServer() || !IsMoviePlayerEnabled()) return;
        FLoadingScreenAttributes Screen;
        Screen.bAutoCompleteWhenLoadingCompletes=true;
        Screen.bWaitForManualStop=false;
        Screen.WidgetLoadingScreen=SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(.035,.043,.038)).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SBox).WidthOverride(354)
                [SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("Sabarmati Drive")))]
                    +SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Text_Lambda([]
                    {
                        const TCHAR* Stage[]={TEXT("Opening the local map"),TEXT("Building roads and crossings"),
                            TEXT("Fitting buildings and street furniture"),TEXT("Lighting the scene"),
                            TEXT("Preparing vehicles"),TEXT("Placing road traffic"),TEXT("Ready")};
                        const int32 Done=ReadySteps.Load();
                        return FText::FromString(FString::Printf(TEXT("%s    %d / %d"),Stage[Done],Done,TotalSteps));
                    })]
                    +SVerticalBox::Slot().AutoHeight().Padding(0,9,0,0)
                    [SNew(SBox).HeightOverride(5)[SNew(SProgressBar)
                        .Percent_Lambda([]{return TOptional<float>(float(ReadySteps.Load())/TotalSteps);})
                        .FillColorAndOpacity(FLinearColor(.76,.48,.25))]]]];
        GetMoviePlayer()->SetupLoadingScreen(Screen);
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FSabarmatiModule, SabarmatiPrototype, "SabarmatiPrototype");
