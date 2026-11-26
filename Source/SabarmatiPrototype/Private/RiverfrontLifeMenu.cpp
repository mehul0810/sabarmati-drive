#include "RiverfrontLifeMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "RiverfrontGenderSelector.h"
#include "RiverfrontPlayerController.h"
#include "RiverfrontCharacter.h"
#include "RiverfrontLifeProfile.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/StrongObjectPtr.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Colors/SSimpleGradient.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace RiverfrontFittingUI
{
const FLinearColor Paper(.97f,.94f,.87f);
const FLinearColor Muted(.66f,.70f,.69f);
const FLinearColor Amber(.99f,.66f,.28f);
const FLinearColor Panel(.012f,.020f,.024f,.95f);
const FLinearColor Row(1.f,1.f,1.f,.065f);
constexpr float PanelWidth = 404.f;

FText Copy(const FString& Text) { return FText::FromString(Text); }
EVisibility Show(bool bVisible) { return bVisible ? EVisibility::Visible : EVisibility::Collapsed; }

FString Rupees(int64 Amount)
{
    // The catalogue deals in whole rupees. ICU supplies Indian lakh/crore grouping.
    const FCulturePtr IndianEnglish = FInternationalization::Get().GetCulture(TEXT("en-IN"));
    return FString(TEXT("₹")) + FText::AsNumber(Amount, nullptr, IndianEnglish).ToString();
}

TSharedRef<STextBlock> Label(TAttribute<FText> Value, int32 Size, FLinearColor Color, bool bBold=false)
{
    return SNew(STextBlock).Text(Value).ColorAndOpacity(Color).AutoWrapText(true)
        .Font(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size));
}

const FRiverfrontWearable* Item(FName Id)
{
    return RiverfrontLife::Catalog().FindByPredicate([Id](const FRiverfrontWearable& Entry){ return Entry.Id == Id; });
}

class SRiverfrontLifeMenu final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRiverfrontLifeMenu) {}
        SLATE_ARGUMENT(UWorld*, AudioWorld)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args, ERiverfrontLifeMenu InPage, const FString& InSlot,
        FSimpleDelegate InChanged, FSimpleDelegate InClose)
    {
        Page = InPage;
        Slot = InSlot;
        OnChanged = MoveTemp(InChanged);
        OnClose = MoveTemp(InClose);
        Reload();
        Selected = !Held.IsNone() ? Held : Item(Outfit) ? Outfit : FName(TEXT("cotton-sand"));
        AudioWorld = Args._AudioWorld;
        if (bAvailable && Page != ERiverfrontLifeMenu::Wallet)
        {
            SelectionTick.Reset(LoadObject<USoundBase>(nullptr,
                TEXT("/Game/Sabarmati/Riverfront/Audio/S_Shift.S_Shift")));
            if (Page == ERiverfrontLifeMenu::Shop)
                CounterBell.Reset(LoadObject<USoundBase>(nullptr,
                    TEXT("/Game/Sabarmati/Riverfront/Audio/S_TeaBell.S_TeaBell")));
        }

        ButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        ButtonStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White,1.f))
            .SetHovered(FSlateRoundedBoxBrush(FLinearColor::White,1.f,Amber,1.5f))
            .SetPressed(FSlateRoundedBoxBrush(FLinearColor::White,1.f,Paper,1.5f))
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));

        const bool bShop = Page == ERiverfrontLifeMenu::Shop;
        const bool bWardrobe = Page == ERiverfrontLifeMenu::Wardrobe;
        const FString Title = bShop ? TEXT("Sutra Cloth House") : bWardrobe ? TEXT("Wardrobe") : TEXT("Wallet");
        const FString Location = bShop ? TEXT("RIVERFRONT MARKET") : bWardrobe ? TEXT("KESAR APARTMENTS  /  FLAT 2B") : TEXT("PERSONAL ACCOUNT");

        TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
        if (bAvailable && (bShop || bWardrobe))
        {
            if (bWardrobe)
            {
                Body->AddSlot().AutoHeight()
                [
                    SNew(SRiverfrontGenderSelector)
                    .MaleTag(TEXT("LifeGenderMale")).FemaleTag(TEXT("LifeGenderFemale"))
                    .Selected_Lambda([this]{ return Gender; })
                    .OnSelected_Lambda([this](ERiverfrontGender Choice)
                    {
                        if (Choice!=Gender) Complete(RiverfrontLife::SetAvatarGender(Slot,Choice),TEXT("Avatar saved."));
                    })
                ];
            }
            for (auto WearSlot : {ERiverfrontWearSlot::Top,ERiverfrontWearSlot::Bottom,ERiverfrontWearSlot::Accessory})
            {
                AddCaption(Body, WearSlot==ERiverfrontWearSlot::Top ? TEXT("Tops") : WearSlot==ERiverfrontWearSlot::Bottom ? TEXT("Bottoms") : TEXT("Accessories"));
                if (WearSlot==ERiverfrontWearSlot::Top)
                    Body->AddSlot().AutoHeight().Padding(0,0,0,4)
                    [SNew(STextBlock).ColorAndOpacity(Paper).Font(FCoreStyle::GetDefaultFontStyle("Regular",12))
                        .Visibility_Lambda([this]{ return Show(Outfit==RiverfrontLife::BaseLayer); })
                        .Text_Lambda([this]{ return Copy(Gender==ERiverfrontGender::Female
                            ? TEXT("Base layer · Sports bra") : TEXT("No top worn")); })];
                if (WearSlot==ERiverfrontWearSlot::Bottom)
                    Body->AddSlot().AutoHeight().Padding(0,0,0,4)
                    [SNew(STextBlock).ColorAndOpacity(Paper).Font(FCoreStyle::GetDefaultFontStyle("Regular",12))
                        .Visibility_Lambda([this]{ return Show(Bottom==RiverfrontLife::Underwear); })
                        .Text(Copy(TEXT("Base layer · Boxer briefs")))];
                for (const auto& Entry : RiverfrontLife::Catalog())
                    if (Entry.Slot==WearSlot)
                        Body->AddSlot().AutoHeight().Padding(0,0,0,4)[ItemRow(Entry)];
            }
            Body->AddSlot().AutoHeight().Padding(0,6,0,0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)
                [ PayButton(false) ]
                + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)
                [ PayButton(true) ]
            ];
            Body->AddSlot().AutoHeight().Padding(0,3)
            [
                SNew(SButton).Tag(TEXT("LifeDress")).ButtonStyle(&ButtonStyle).ButtonColorAndOpacity(Amber)
                .ContentPadding(FMargin(8,8)).HAlign(HAlign_Center)
                .Visibility_Lambda([this]{ return Show(Owned.Contains(Selected)); })
                .IsEnabled_Lambda([this]{ return !DressingTimer.IsValid() && (Held.IsNone() || Held==Selected); })
                .OnClicked(this,&SRiverfrontLifeMenu::DressSelected)
                [SNew(STextBlock).Tag(TEXT("LifeDressAction")).ColorAndOpacity(Panel)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold",13)).Text_Lambda([this]{ return ActionText(); })]
            ];
            Body->AddSlot().AutoHeight().Padding(0,3)
            [
                SNew(SButton).Tag(TEXT("LifePutBack")).ButtonStyle(&ButtonStyle).ButtonColorAndOpacity(Row)
                .Visibility_Lambda([this]{ return Show(!Held.IsNone()); })
                .IsEnabled_Lambda([this]{ return !DressingTimer.IsValid(); })
                .ContentPadding(FMargin(8,7)).HAlign(HAlign_Center)
                .OnClicked_Lambda([this]{ return FinishAction(EAction::PutBack,Held); })
                [SNew(STextBlock).ColorAndOpacity(Paper).Text_Lambda([this]{return Copy(TEXT("Put back ")+ItemLabel(Held));})]
            ];
            if (bWardrobe)
            {
                Body->AddSlot().AutoHeight().Padding(0,6,0,1)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)
                    [ ViewButton(EFittingView::Face,TEXT("LifePortrait"),TEXT("Inspect face")) ]
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)
                    [ ViewButton(EFittingView::Tailoring,TEXT("LifeTailoring"),TEXT("Inspect fit")) ]
                ];
                AddCaption(Body,TEXT("Skin tone"));
                Body->AddSlot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)[ToneButton(NAME_None,TEXT("Original"))]
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(2,0)[ToneButton(TEXT("warm"),TEXT("Warm"))]
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)[ToneButton(TEXT("umber"),TEXT("Umber"))]
                ];
                auto* Walker = Cast<ARiverfrontCharacter>(UGameplayStatics::GetPlayerCharacter(Args._AudioWorld,0));
                if (Walker && Walker->SupportsPublicHaircut())
                {
                    AddCaption(Body,TEXT("Hair"));
                    Body->AddSlot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)[HairButton(false)]
                        + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)[HairButton(true)]
                    ];
                }
            }
        }
        else if (bAvailable)
        {
            AddCaption(Body,TEXT("Last transaction"));
            Body->AddSlot().AutoHeight()[Label(Copy(LastReceipt.IsEmpty() ? TEXT("No transactions yet.") : LastReceipt),15,Paper)];
            if (bDeliveryActive || Deliveries > 0)
            {
                AddCaption(Body,TEXT("Print deliveries"));
                FString Work = bDeliveryActive ? TEXT("One order is out for delivery.") : TEXT("No open order.");
                if (Deliveries > 0) Work += FString::Printf(TEXT(" Completed: %d."), Deliveries);
                Body->AddSlot().AutoHeight()[Label(Copy(Work),15,Paper)];
            }
        }
        if (!bAvailable)
            Body->AddSlot().AutoHeight()[Label(Copy(TEXT("Your local profile could not be opened.")),14,Amber)];
        Body->AddSlot().AutoHeight().Padding(0,9,0,0)
        [
            SNew(STextBlock).Tag(TEXT("LifeReceipt"))
            .Visibility_Lambda([this]{ return Show(!Feedback.IsEmpty()); })
            .Text_Lambda([this]{ return Feedback; }).ColorAndOpacity(Amber).AutoWrapText(true)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular",14))
        ];

        ChildSlot
        [
            SNew(SDPIScaler).DPIScale(this,&SRiverfrontLifeMenu::PanelScale)
            [
                SNew(SOverlay)
                + SOverlay::Slot().HAlign(HAlign_Right)
                [
                    SNew(SHorizontalBox).Visibility(EVisibility::HitTestInvisible)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(80.f)
                        [ SNew(SSimpleGradient).Orientation(Orient_Horizontal)
                            .StartColor(Panel.CopyWithNewOpacity(0.f)).EndColor(Panel) ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        SNew(SBox).WidthOverride(PanelWidth + 14.f)
                        [ SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Panel) ]
                    ]
                ]
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0,12,14,12))
                [
                    SNew(SBox).Tag(TEXT("LifePanel")).WidthOverride(PanelWidth).MaxDesiredHeight(650.f)
                    [
                        SNew(SBox).Padding(FMargin(16,8,14,8))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,7)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Label(Copy(Location),10,Amber,true)]
                                + SHorizontalBox::Slot().AutoWidth()
                                [
                                    SAssignNew(CloseButton,SButton).Tag(TEXT("LifeClose")).ButtonStyle(&ButtonStyle)
                                    .ButtonColorAndOpacity(Row).ContentPadding(FMargin(9,5)).OnClicked(this,&SRiverfrontLifeMenu::Close)
                                    [SNew(STextBlock).Text(Copy(TEXT("Esc   Back"))).ColorAndOpacity(Paper)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Medium",11))]
                                ]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[Label(Copy(Title),32,Paper,true)]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SHorizontalBox).Visibility(Show(bAvailable))
                                + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)[Balance(false)]
                                + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)[Balance(true)]
                            ]
                            + SVerticalBox::Slot().FillHeight(1.f)
                            [
                                SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
                                + SScrollBox::Slot()[Body]
                            ]
                        ]
                    ]
                ]
            ]
        ];
    }

    bool SupportsKeyboardFocus() const override { return true; }
    FReply OnFocusReceived(const FGeometry&, const FFocusEvent&) override { return ReturnFocus(); }
    FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Key) override
    {
        if (Key.GetKey() == EKeys::Escape) return Close();
        return SCompoundWidget::OnPreviewKeyDown(Geometry,Key);
    }

private:
    ERiverfrontLifeMenu Page = ERiverfrontLifeMenu::Wallet;
    FString Slot, LastReceipt;
    FSimpleDelegate OnChanged, OnClose;
    using EAction=RiverfrontLife::EDressingAction;
    FButtonStyle ButtonStyle;
    TSharedPtr<SButton> CloseButton;
    TMap<FName,TSharedPtr<SButton>> Rows;
    TArray<FName> Owned;
    FName Selected, Outfit, Bottom, Accessory, Held, Tone;
    ERiverfrontGender Gender = ERiverfrontGender::Male;
    int64 Cash = 0, Bank = 0;
    int32 Deliveries = 0;
    bool bAvailable = false, bDeliveryActive = false;
    bool bCroppedHair = false;
    EFittingView ViewMode = EFittingView::Full;
    FText Feedback;
    TWeakObjectPtr<UWorld> AudioWorld;
    TWeakPtr<FActiveTimerHandle> DressingTimer;
    double DressStageStart=0.;
    EAction PendingAction=EAction::Take;
    FName PendingItem;
    bool bTransferred=false;
    TStrongObjectPtr<USoundBase> SelectionTick, CounterBell;

    float PanelScale() const
    {
        const UWorld* World = AudioWorld.Get();
        const auto* Viewport = World ? World->GetGameViewport() : nullptr;
        FVector2D Size = FVector2D::ZeroVector;
        if (Viewport) Viewport->GetViewportSize(Size);
        if (Size.X <= 0 || Size.Y <= 0) return 1.f;

        // Keep the same readable physical width when the normal viewport DPI shrinks.
        // On narrow windows the fitting view retains at least half of the screen.
        const float MinimumWidth = FMath::Min(330.f,float(Size.X)*.5f);
        const FIntPoint Pixels(FMath::RoundToInt(Size.X),FMath::RoundToInt(Size.Y));
        const float ViewportDPI = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Pixels);
        return FMath::Max(1.f,MinimumWidth/(PanelWidth*ViewportDPI));
    }

    void PlayFeedback(USoundBase* Wave, float Volume, float Pitch) const
    {
        if (Wave && AudioWorld.IsValid())
            UGameplayStatics::PlaySound2D(AudioWorld.Get(),Wave,Volume,Pitch);
    }

    void Reload()
    {
        const URiverfrontLocalProfile* Saved = RiverfrontLife::Load(Slot);
        bAvailable = Saved != nullptr;
        if (!Saved) return;
        Cash = Saved->CashRupees;
        Bank = Saved->BankRupees;
        Owned = Saved->OwnedItems;
        Outfit = Saved->EquippedOutfit.IsNone() ? FName(TEXT("cotton-sand")) : Saved->EquippedOutfit;
        Bottom = Saved->EquippedBottom;
        Accessory = Saved->EquippedAccessory;
        Tone = Saved->AppearanceTone;
        Held = Saved->HeldWardrobeItem;
        Gender = Saved->AvatarGender;
        bCroppedHair = Saved->bCroppedHair;
        LastReceipt = Saved->LastReceipt;
        bDeliveryActive = Saved->bPrintDeliveryActive;
        Deliveries = Saved->CompletedPrintDeliveries;
    }

    static void AddCaption(const TSharedRef<SVerticalBox>& Body, const FString& Caption)
    {
        Body->AddSlot().AutoHeight().Padding(0,13,0,5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(STextBlock).Text(Copy(Caption.ToUpper())).ColorAndOpacity(Amber)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold",11))]
            + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(10,0,0,0)
            [
                SNew(SBox).HeightOverride(1.f)
                [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(Paper.CopyWithNewOpacity(.19f))]
            ]
        ];
    }

    TSharedRef<SWidget> Balance(bool bBank)
    {
        return SNew(SBox).Padding(FMargin(0,2,4,5))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Label(Copy(bBank ? TEXT("BANK") : TEXT("CASH")),11,Muted)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)
                [
                    SNew(STextBlock).Tag(bBank ? TEXT("LifeBankBalance") : TEXT("LifeCashBalance"))
                    .Text_Lambda([this,bBank]{ return Copy(Rupees(bBank ? Bank : Cash)); })
                    .ColorAndOpacity(Paper).Font(FCoreStyle::GetDefaultFontStyle("Bold",19))
                ]
            ];
    }

    bool Wearing(FName Id) const { return !Id.IsNone() && (Id == Outfit || Id == Bottom || Id == Accessory); }

    TSharedRef<SWidget> ItemRow(const FRiverfrontWearable& Entry)
    {
        const FName Id = Entry.Id;
        TSharedPtr<SButton> RowButton;
        SAssignNew(RowButton,SButton).Tag(FName(*(TEXT("LifeItem.")+Id.ToString()))).ButtonStyle(&ButtonStyle)
            .ContentPadding(FMargin(10,6)).HAlign(HAlign_Fill)
            .ButtonColorAndOpacity_Lambda([this,Id]{ return FSlateColor(Selected == Id ? Amber.CopyWithNewOpacity(.15f) : Row); })
            .OnClicked_Lambda([this,Id]{ return Select(Id); })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,9,0)
                [
                    SNew(SBox).WidthOverride(4).HeightOverride(24)
                    [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Entry.Tint)]
                ]
                + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[Label(Copy(Entry.Label),14,Paper,true)]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0,0,0)
                [
                    SNew(STextBlock).ColorAndOpacity(Muted).Font(FCoreStyle::GetDefaultFontStyle("Regular",14))
                    .Text_Lambda([this,Id,Price=Entry.PriceRupees]{
                        return Copy(Wearing(Id) ? TEXT("Wearing") : Held==Id ? TEXT("In hand") : Owned.Contains(Id) ? TEXT("Owned") : Page==ERiverfrontLifeMenu::Wardrobe ? FString(TEXT("At Sutra · "))+Rupees(Price) : Rupees(Price));
                    })
                ]
            ];
        Rows.Add(Id,RowButton);
        return RowButton.ToSharedRef();
    }

    TSharedRef<SWidget> PayButton(bool bBank)
    {
        return SNew(SButton).Tag(bBank ? TEXT("LifeBuyBank") : TEXT("LifeBuyCash")).ButtonStyle(&ButtonStyle)
            .ButtonColorAndOpacity(bBank ? Row : Amber).ContentPadding(FMargin(5,8)).HAlign(HAlign_Center)
            .Visibility_Lambda([this]{ return Show(Page == ERiverfrontLifeMenu::Shop && !Selected.IsNone() && !Owned.Contains(Selected)); })
            .OnClicked_Lambda([this,bBank]{
                const FText Error = RiverfrontLife::Buy(Slot,Selected,bBank);
                return Complete(Error, FString(), true);
            })
            [
                SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(bBank ? Paper : Panel)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold",14))
                .Text_Lambda([this,bBank]() -> FText {
                    const auto* Chosen = Item(Selected);
                    if (!Chosen) return FText::GetEmpty();
                    return Copy(FString::Printf(TEXT("Pay %s %s"), *Rupees(Chosen->PriceRupees),
                        bBank ? TEXT("from bank") : TEXT("cash")));
                })
            ];
    }

    TSharedRef<SWidget> ViewButton(EFittingView Mode, FName Tag, FString LabelText)
    {
        return SNew(SButton).Tag(Tag).ButtonStyle(&ButtonStyle).ButtonColorAndOpacity(Row)
            .ContentPadding(FMargin(8,7)).HAlign(HAlign_Center)
            .OnClicked_Lambda([this,Mode]
            {
                const EFittingView Next = Mode==ViewMode ? EFittingView::Full : Mode;
                auto* Controller = Cast<ARiverfrontPlayerController>(UGameplayStatics::GetPlayerController(AudioWorld.Get(),0));
                if (Controller && Controller->SetFittingView(Next)) ViewMode=Next;
                else Feedback=Copy(TEXT("There isn't room for a close-up here."));
                return FReply::Handled();
            })
            [SNew(STextBlock).ColorAndOpacity(Paper).Font(FCoreStyle::GetDefaultFontStyle("Regular",13))
                .Text_Lambda([this,Mode,LabelText]{ return Copy(Mode==ViewMode ? FString(TEXT("Full outfit")) : LabelText); })];
    }

    TSharedRef<SWidget> ToneButton(FName Choice, const FString& Name)
    {
        const FString Key = Choice.IsNone() ? TEXT("default") : Choice.ToString();
        return SNew(SButton).Tag(FName(*(TEXT("LifeTone.")+Key))).ButtonStyle(&ButtonStyle).HAlign(HAlign_Center)
            .ButtonColorAndOpacity_Lambda([this,Choice]{ return FSlateColor(Tone == Choice ? FLinearColor(.16f,.18f,.14f) : Row); })
            .ContentPadding(FMargin(6,7)).OnClicked_Lambda([this,Choice]{
                return Complete(RiverfrontLife::SetAppearanceTone(Slot,Choice),TEXT("Appearance saved."));
            })[Label(Copy(Name),13,Paper)];
    }

    TSharedRef<SWidget> HairButton(bool bCropped)
    {
        return SNew(SButton).Tag(bCropped ? TEXT("LifeHair.cropped") : TEXT("LifeHair.swept"))
            .ButtonStyle(&ButtonStyle).HAlign(HAlign_Center).ContentPadding(FMargin(6,7))
            .ButtonColorAndOpacity_Lambda([this,bCropped]{
                return FSlateColor(bCroppedHair == bCropped ? FLinearColor(.16f,.18f,.14f) : Row);
            })
            .OnClicked_Lambda([this,bCropped]{
                if (bCroppedHair == bCropped) return FReply::Handled();
                return Complete(RiverfrontLife::SetCroppedHair(Slot,bCropped),TEXT("Hairstyle saved."));
            })[Label(Copy(bCropped ? TEXT("Cropped") : TEXT("Swept")),13,Paper)];
    }

    ARiverfrontCharacter* Resident() const
    {
        return AudioWorld.IsValid() ? Cast<ARiverfrontCharacter>(UGameplayStatics::GetPlayerCharacter(AudioWorld.Get(),0)) : nullptr;
    }

    void StopDressing()
    {
        if (auto Handle=DressingTimer.Pin()) UnRegisterActiveTimer(Handle.ToSharedRef());
        DressingTimer.Reset();
        if (auto* Walker=Resident()) Walker->EndChangingPose();
    }

    static FString ItemLabel(FName Id)
    {
        const auto* Choice=Item(Id);
        return Choice ? Choice->Label : FString();
    }

    FName WornInSlot() const
    {
        const auto* Choice=Item(Selected);
        if (Choice && Choice->Slot==ERiverfrontWearSlot::Bottom) return Bottom;
        return Choice && Choice->Slot==ERiverfrontWearSlot::Accessory ? Accessory : Outfit;
    }

    EAction NextAction() const
    {
        if (Held==Selected && !Held.IsNone()) return EAction::PutOn;
        return Item(WornInSlot()) ? EAction::TakeOff : EAction::Take;
    }

    FText ActionText() const
    {
        if (DressingTimer.IsValid()) return Copy(PendingAction==EAction::TakeOff ? TEXT("Taking off…") : TEXT("Putting on…"));
        if (!Held.IsNone() && Held!=Selected) return Copy(TEXT("Put the held item back first"));
        switch (NextAction())
        {
        case EAction::TakeOff: return Copy(TEXT("Take off ")+ItemLabel(WornInSlot()));
        case EAction::Take: return Copy(Page==ERiverfrontLifeMenu::Shop ? TEXT("Take from shelf") : TEXT("Take from wardrobe"));
        default: return Copy(TEXT("Put on ")+ItemLabel(Held));
        }
    }

    FReply FinishAction(EAction Action,FName Id)
    {
        const FString Name=ItemLabel(Id);
        const FText Error=RiverfrontLife::Dress(Slot,Action,Id);
        const FString Message=Action==EAction::Take ? Name+TEXT(" in hand.") :
            Action==EAction::PutOn ? TEXT("Wearing ")+Name+TEXT(".") : Name+TEXT(" returned to the shelf.");
        return Complete(Error,Message);
    }

    EActiveTimerReturnType AdvanceDressing(double Now,float)
    {
        auto* Walker=Resident();
        const float Time=FMath::Clamp(float((Now-DressStageStart)/(PendingItem==TEXT("kurta") ? 1.65 : 1.85)),0.f,1.f);
        if (Walker && Time<1.f)
        {
            if (!bTransferred && Time>=.5f)
            {
                const bool bRemoving=PendingAction==EAction::TakeOff;
                Walker->ApplyWardrobeLook(bRemoving ? RiverfrontLife::BaseLayer : PendingItem,Accessory,Tone);
                Walker->ShowHeldWardrobeItem(bRemoving ? PendingItem : NAME_None);
                bTransferred=true;
            }
            const float Lift=FMath::SmoothStep(0.f,.42f,Time)*(1.f-FMath::SmoothStep(.58f,1.f,Time));
            const float SleeveTime=PendingAction==EAction::TakeOff ? Time : 1.f-Time;
            const float SleevePass=FMath::Sin(2.f*PI*SleeveTime);
            Walker->PoseChangingSleeves(Lift,PendingItem==TEXT("kurta") ? 1.f : 0.f,SleevePass);
            if (auto* Controller=Cast<ARiverfrontPlayerController>(Walker->GetController())) Controller->RefreshWardrobeMirror();
            return EActiveTimerReturnType::Continue;
        }
        DressingTimer.Reset();
        if (Walker)
        {
            Walker->EndChangingPose();
            FinishAction(PendingAction,PendingItem);
        }
        return EActiveTimerReturnType::Stop;
    }

    FReply DressSelected()
    {
        if (DressingTimer.IsValid() || !Owned.Contains(Selected) || (!Held.IsNone() && Selected!=Held))
            return FReply::Handled();
        const auto Action=NextAction();
        const FName Id=Action==EAction::TakeOff ? WornInSlot() : Selected;
        const auto* Choice=Item(Id);
        auto* Walker=Resident();
        if (Action==EAction::Take || !Choice || Choice->Slot==ERiverfrontWearSlot::Accessory || !Walker || !Walker->SupportsClothingChange())
            return FinishAction(Action,Id);
        bTransferred=false;
        PendingAction=Action;
        PendingItem=Id;
        DressStageStart=FSlateApplication::Get().GetCurrentTime();
        DressingTimer=RegisterActiveTimer(1.f/30.f,FWidgetActiveTimerDelegate::CreateSP(this,&SRiverfrontLifeMenu::AdvanceDressing));
        return FReply::Handled();
    }

    FReply Select(FName Id)
    {
        if (DressingTimer.IsValid()) return FReply::Handled();
        if (Selected!=Id) PlayFeedback(SelectionTick.Get(),.13f,1.40f);
        Selected=Id;
        Feedback=FText::GetEmpty();
        return FReply::Handled();
    }

    FReply ReturnFocus()
    {
        if (const auto* Button = Rows.Find(Selected))
            return FReply::Handled().SetUserFocus(Button->ToSharedRef());
        return CloseButton ? FReply::Handled().SetUserFocus(CloseButton.ToSharedRef()) : FReply::Handled();
    }

    FReply Complete(const FText& Error, const FString& Success, bool bReceipt=false)
    {
        StopDressing();
        const int64 CashBefore = Cash, BankBefore = Bank;
        Reload();
        if (!bAvailable) { Feedback = Copy(TEXT("The saved profile could not be reopened.")); return FReply::Handled(); }
        Feedback = Error.IsEmpty() ? Copy(bReceipt ? LastReceipt : Success) : Error;
        if (bReceipt && (Cash < CashBefore || Bank < BankBefore))
            PlayFeedback(CounterBell.Get(),.19f,.96f);
        OnChanged.ExecuteIfBound();
        return ReturnFocus();
    }

    FReply Close()
    {
        StopDressing();
        OnClose.ExecuteIfBound();
        return FReply::Handled();
    }
};
}

TSharedRef<SWidget> MakeRiverfrontLifeMenu(UWorld& World, ERiverfrontLifeMenu Page, const FString& ProfileSlot,
    FSimpleDelegate OnChanged, FSimpleDelegate OnClose)
{
    return SNew(RiverfrontFittingUI::SRiverfrontLifeMenu,Page,ProfileSlot,MoveTemp(OnChanged),MoveTemp(OnClose))
        .AudioWorld(&World);
}
