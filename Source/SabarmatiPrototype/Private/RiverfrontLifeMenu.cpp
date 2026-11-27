#include "RiverfrontLifeMenu.h"
#include "RiverfrontInterfaceStyle.h"
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
const FLinearColor& Paper=RiverfrontInterface::Text;
const FLinearColor& Muted=RiverfrontInterface::Muted;
const FLinearColor& Amber=RiverfrontInterface::Accent;
const FLinearColor Panel=RiverfrontInterface::Ink.CopyWithNewOpacity(.965f);
const FLinearColor Row=RiverfrontInterface::Surface;
const FSlateRoundedBoxBrush PanelBrush(FLinearColor::White,4.f,RiverfrontInterface::Line,1.2f);
constexpr float PanelWidth=420.f;

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
            if (Page != ERiverfrontLifeMenu::Wardrobe)
                CounterBell.Reset(LoadObject<USoundBase>(nullptr,
                    TEXT("/Game/Sabarmati/Riverfront/Audio/S_TeaBell.S_TeaBell")));
        }

        ButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
        ButtonStyle.SetNormal(FSlateRoundedBoxBrush(FLinearColor::White,1.f))
            .SetHovered(FSlateRoundedBoxBrush(FLinearColor::White,1.f,Amber,1.5f))
            .SetPressed(FSlateRoundedBoxBrush(FLinearColor::White,1.f,Paper,1.5f))
            .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));

        const bool bTrial=Page==ERiverfrontLifeMenu::FittingRoom;
        const bool bShop=Page==ERiverfrontLifeMenu::Shop || bTrial;
        const bool bWardrobe = Page == ERiverfrontLifeMenu::Wardrobe;
        const FString Title = bTrial ? TEXT("Fitting room") : bShop ? TEXT("Sutra Cloth House") : bWardrobe ? TEXT("Wardrobe") : TEXT("Wallet");
        const FString Location = bTrial ? TEXT("SUTRA CLOTH HOUSE") : bShop ? TEXT("RIVERFRONT MARKET") : bWardrobe ? TEXT("KESAR APARTMENTS  /  FLAT 2B") : TEXT("PERSONAL ACCOUNT");

        TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
        TSharedRef<SVerticalBox> Actions=SNew(SVerticalBox);
        TSharedRef<SHorizontalBox> Filters=SNew(SHorizontalBox);
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
            for (const auto Group : {EGroup::All,EGroup::Tops,EGroup::Bottoms,EGroup::Layers,EGroup::Gear})
                Filters->AddSlot().FillWidth(1.f).Padding(1,0)[FilterButton(Group)];
            for (auto WearSlot : {ERiverfrontWearSlot::Top,ERiverfrontWearSlot::Bottom,ERiverfrontWearSlot::InnerTop,
                ERiverfrontWearSlot::InnerBottom,ERiverfrontWearSlot::Socks,ERiverfrontWearSlot::Shoes,ERiverfrontWearSlot::Accessory})
            {
                const EGroup Group=GroupOf(WearSlot);
                TSharedRef<SVerticalBox> Section=SNew(SVerticalBox)
                    .Visibility_Lambda([this,Group]{ return Show(Filter==EGroup::All || Filter==Group); });
                Body->AddSlot().AutoHeight()[Section];
                if (WearSlot==ERiverfrontWearSlot::Top) AddCaption(Section,TEXT("Tops"));
                if (WearSlot==ERiverfrontWearSlot::Bottom) AddCaption(Section,TEXT("Bottoms"));
                if (WearSlot==ERiverfrontWearSlot::InnerTop) AddCaption(Section,TEXT("Inner tops"));
                if (WearSlot==ERiverfrontWearSlot::InnerBottom) AddCaption(Section,TEXT("Inner bottoms"));
                if (WearSlot==ERiverfrontWearSlot::Socks) AddCaption(Section,TEXT("Socks"));
                if (WearSlot==ERiverfrontWearSlot::Shoes) AddCaption(Section,TEXT("Footwear"));
                if (WearSlot==ERiverfrontWearSlot::Accessory) AddCaption(Section,TEXT("Accessories"));
                for (const auto& Entry : RiverfrontLife::Catalog())
                    if (Entry.Slot==WearSlot)
                        Section->AddSlot().AutoHeight().Padding(0,0,0,4)[ItemRow(Entry)];
            }
            Actions->AddSlot().AutoHeight().Padding(0,5,0,1)
            [
                SNew(STextBlock).Tag(TEXT("LifeSelected")).AutoWrapText(true).ColorAndOpacity(Paper)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold",15))
                    .Text_Lambda([this]{ return Copy(ItemLabel(Selected)); })
            ];
            Actions->AddSlot().AutoHeight().Padding(0,6,0,0)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)
                [ PayButton(false) ]
                + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)
                [ PayButton(true) ]
            ];
            Actions->AddSlot().AutoHeight().Padding(0,3)
            [
                SNew(SButton).Tag(TEXT("LifeDress")).ButtonStyle(&ButtonStyle).ButtonColorAndOpacity(Amber)
                .ContentPadding(FMargin(8,8)).HAlign(HAlign_Center)
                .Visibility_Lambda([this]{ return Show(Trial.IsValid() || Owned.Contains(Selected)); })
                .IsEnabled_Lambda([this]{ return !DressingTimer.IsValid() && (Held.IsNone() || Held==Selected) && Cover().IsNone(); })
                .OnClicked(this,&SRiverfrontLifeMenu::DressSelected)
                [SNew(STextBlock).Tag(TEXT("LifeDressAction")).ColorAndOpacity(Panel)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold",13)).Text_Lambda([this]{ return ActionText(); })]
            ];
            Actions->AddSlot().AutoHeight().Padding(0,3)
            [
                SNew(SButton).Tag(TEXT("LifePutBack")).ButtonStyle(&ButtonStyle).ButtonColorAndOpacity(Row)
                .Visibility_Lambda([this]{ return Show(!Held.IsNone()); })
                .IsEnabled_Lambda([this]{ return !DressingTimer.IsValid(); })
                .ContentPadding(FMargin(8,7)).HAlign(HAlign_Center)
                .OnClicked_Lambda([this]{ return FinishAction(EAction::PutBack,Held); })
                [SNew(STextBlock).ColorAndOpacity(Paper).Text_Lambda([this]{return Copy(TEXT("Put back ")+ItemLabel(Held));})]
            ];
            if (bWardrobe || bTrial)
            {
                Actions->AddSlot().AutoHeight().Padding(0,6,0,1)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)
                    [ ViewButton(EFittingView::Face,TEXT("LifePortrait"),TEXT("Inspect face")) ]
                    + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)
                    [ ViewButton(EFittingView::Tailoring,TEXT("LifeTailoring"),TEXT("Inspect fit")) ]
                ];
            }
            if (bWardrobe)
            {
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
        if (bTrial)
            Actions->AddSlot().AutoHeight().Padding(0,3)
            [
                SNew(SButton).Tag(TEXT("LifeKeepOutfit")).ButtonStyle(&ButtonStyle).ButtonColorAndOpacity(Row)
                    .IsEnabled_Lambda([this]{return Held.IsNone() && !DressingTimer.IsValid();})
                    .ContentPadding(FMargin(6,6)).HAlign(HAlign_Center)
                    .OnClicked_Lambda([this]{
                        const auto Error=RiverfrontLife::KeepFittingOutfit(Slot,*Entered.Get(),*Trial.Get());
                        if (!Error.IsEmpty()) { Feedback=Error; return FReply::Handled(); }
                        return Close();
                    })[SNew(STextBlock).Text(Copy(TEXT("Wear this outfit out"))).ColorAndOpacity(Paper)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold",13))]
            ];
        if (bTrial || Page==ERiverfrontLifeMenu::Shop)
            Actions->AddSlot().AutoHeight().Padding(0,4)[Label(Copy(bTrial
                ? TEXT("Try any garment. Esc restores your outfit. Purchases stay in your wardrobe.")
                : TEXT("Try clothes in the curtained fitting room to the right of the counter.")),12,Muted)];
        if (!bAvailable)
            Body->AddSlot().AutoHeight()[Label(Copy(TEXT("Your local profile could not be opened.")),14,Amber)];
        Actions->AddSlot().AutoHeight().Padding(0,3,0,0)
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
                + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0,20,23,20))
                [
                    SNew(SBox).Tag(TEXT("LifePanel")).WidthOverride(PanelWidth).MaxDesiredHeight(683.f)
                    [
                        SNew(SBorder).BorderImage(&PanelBrush).BorderBackgroundColor(Panel).Padding(FMargin(18,13,18,13))
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
                            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,9)
                            [
                                SNew(STextBlock).Text(Copy(Title)).ColorAndOpacity(Paper)
                                    .Font(FCoreStyle::GetDefaultFontStyle("BoldCondensed",35))
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,11)
                            [
                                SNew(SBox).HeightOverride(1)
                                [SNew(SBorder).Padding(0).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                    .BorderBackgroundColor(RiverfrontInterface::Line)]
                            ]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SHorizontalBox).Visibility(Show(bAvailable))
                                + SHorizontalBox::Slot().FillWidth(1.f).Padding(0,0,3,0)[Balance(false)]
                                + SHorizontalBox::Slot().FillWidth(1.f).Padding(3,0,0,0)[Balance(true)]
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0,4)[Filters]
                            + SVerticalBox::Slot().FillHeight(1.f)
                            [
                                SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
                                + SScrollBox::Slot()[Body]
                            ]
                            + SVerticalBox::Slot().AutoHeight()[Actions]
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
        if (Key.GetKey()==EKeys::Escape || Key.GetKey()==EKeys::Gamepad_FaceButton_Right) return Close();
        return SCompoundWidget::OnPreviewKeyDown(Geometry,Key);
    }

private:
    enum class EGroup : uint8 { All, Tops, Bottoms, Layers, Gear };
    EGroup Filter=EGroup::All;

    static EGroup GroupOf(ERiverfrontWearSlot Slot)
    {
        switch (Slot)
        {
        case ERiverfrontWearSlot::Top: return EGroup::Tops;
        case ERiverfrontWearSlot::Bottom: return EGroup::Bottoms;
        case ERiverfrontWearSlot::InnerTop:
        case ERiverfrontWearSlot::InnerBottom:
        case ERiverfrontWearSlot::Socks: return EGroup::Layers;
        default: return EGroup::Gear;
        }
    }

    TSharedRef<SWidget> FilterButton(EGroup Group)
    {
        const TCHAR* Names[]={TEXT("All"),TEXT("Tops"),TEXT("Bottoms"),TEXT("Layers"),TEXT("Gear")};
        const FString Name=Names[static_cast<uint8>(Group)];
        return SNew(SButton).Tag(FName(*(TEXT("LifeFilter.")+Name))).ButtonStyle(&ButtonStyle)
            .ContentPadding(FMargin(1,6)).HAlign(HAlign_Center)
            .IsEnabled_Lambda([this]{ return !DressingTimer.IsValid(); })
            .ButtonColorAndOpacity_Lambda([this,Group]{return FSlateColor(Group==Filter ? Amber : Row);})
            .OnClicked_Lambda([this,Group]
            {
                Filter=Group;
                const auto* Current=Item(Selected);
                if (Group!=EGroup::All && (!Current || GroupOf(Current->Slot)!=Group))
                    for (const auto& Entry:RiverfrontLife::Catalog())
                        if (GroupOf(Entry.Slot)==Group && Entry.Fits(Gender))
                            return Select(Entry.Id);
                return FReply::Handled();
            })
            [SNew(STextBlock).Text(Copy(Name)).Font(FCoreStyle::GetDefaultFontStyle("Bold",12))
                .ColorAndOpacity_Lambda([this,Group]{return FSlateColor(Group==Filter ? Panel : Paper);})];
    }

    ERiverfrontLifeMenu Page = ERiverfrontLifeMenu::Wallet;
    FString Slot, LastReceipt;
    FSimpleDelegate OnChanged, OnClose;
    using EAction=RiverfrontLife::EDressingAction;
    FButtonStyle ButtonStyle;
    TSharedPtr<SButton> CloseButton;
    TMap<FName,TSharedPtr<SButton>> Rows;
    TStrongObjectPtr<URiverfrontLocalProfile> Entered,Trial;
    TArray<FName> Owned;
    FName Selected, Outfit, Bottom, InnerTop, InnerBottom, Shoes, Socks, Accessory, Held, Tone;
    ERiverfrontGender Gender = ERiverfrontGender::Male;
    int64 Cash = 0, Bank = 0;
    int32 Deliveries = 0;
    bool bAvailable = false, bDeliveryActive = false;
    bool bCroppedHair = false;
    EFittingView ViewMode = EFittingView::Full;
    FText Feedback;
    TWeakObjectPtr<UWorld> AudioWorld;
    TWeakPtr<FActiveTimerHandle> DressingTimer;
    TWeakPtr<FActiveTimerHandle> InspectionTimer;
    double DressStageStart=0.;
    EAction PendingAction=EAction::Take;
    FName PendingItem;
    bool bTransferred=false;
    bool bChangingBottom=false;
    bool bChangingFeet=false;
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
        URiverfrontLocalProfile* Saved = RiverfrontLife::Load(Slot);
        bAvailable = Saved != nullptr;
        if (!Saved) return;
        Cash = Saved->CashRupees;
        Bank = Saved->BankRupees;
        Owned = Saved->OwnedItems;
        LastReceipt = Saved->LastReceipt;
        if (Page==ERiverfrontLifeMenu::FittingRoom)
        {
            if (!Trial.IsValid())
            {
                Entered.Reset(Saved);
                Trial.Reset(DuplicateObject<URiverfrontLocalProfile>(Saved,GetTransientPackage()));
            }
            Saved=Trial.Get();
        }
        Outfit = Saved->EquippedOutfit.IsNone() ? FName(TEXT("cotton-sand")) : Saved->EquippedOutfit;
        Bottom = Saved->EquippedBottom;
        InnerTop = RiverfrontLife::FittedItem(Saved->EquippedInnerTop,Saved->AvatarGender);
        InnerBottom = Saved->EquippedInnerBottom;
        Shoes = Saved->EquippedShoes;
        Socks = Saved->EquippedSocks;
        Accessory = Saved->EquippedAccessory;
        Tone = Saved->AppearanceTone;
        Held = Saved->HeldWardrobeItem;
        Gender = Saved->AvatarGender;
        if (const auto* SelectedItem=Item(Selected); SelectedItem && !SelectedItem->Fits(Gender)) Selected=RiverfrontLife::Briefs;
        bCroppedHair = Saved->bCroppedHair;
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
        const bool bWallet = Page == ERiverfrontLifeMenu::Wallet;
        const auto Name = Label(Copy(bBank ? TEXT("BANK") : TEXT("CASH")),11,Muted);
        const auto Amount = SNew(STextBlock).Tag(bBank ? TEXT("LifeBankBalance") : TEXT("LifeCashBalance"))
            .Text_Lambda([this,bBank]{ return Copy(Rupees(bBank ? Bank : Cash)); })
            .ColorAndOpacity(Paper).Font(FCoreStyle::GetDefaultFontStyle("Bold",bWallet ? 19 : 14));
        if (!bWallet)
        {
            return SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,8)[Name]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,0,8)[Amount];
        }
        return SNew(SBox).Padding(FMargin(0,2,4,5))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Name]
                + SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)[Amount]
            ];
    }

    bool Wearing(FName Id) const { return !Id.IsNone() && (Id == Outfit || Id == Bottom || Id==Shoes || Id==Socks || Id==InnerBottom || Id==InnerTop || Id==Accessory); }

    TSharedRef<SWidget> ItemRow(const FRiverfrontWearable& Entry)
    {
        const FName Id = Entry.Id;
        TSharedPtr<SButton> RowButton;
        SAssignNew(RowButton,SButton).Tag(FName(*(TEXT("LifeItem.")+Id.ToString()))).ButtonStyle(&ButtonStyle)
            .ContentPadding(FMargin(10,6)).HAlign(HAlign_Fill)
            .ButtonColorAndOpacity_Lambda([this,Id]{ return FSlateColor(Selected==Id ? RiverfrontInterface::Selection : Row); })
            .Visibility_Lambda([this,Id]{ return Show(!RiverfrontLife::FittedItem(Id,Gender).IsNone()); })
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
                        return Copy(Wearing(Id) ? (Trial.IsValid() && !Owned.Contains(Id) ? TEXT("Trying on") : TEXT("Wearing")) : Held==Id ? TEXT("In hand") : Owned.Contains(Id) ? TEXT("Owned") : Page==ERiverfrontLifeMenu::Wardrobe ? FString(TEXT("At Sutra · "))+Rupees(Price) : Rupees(Price));
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
            .Visibility_Lambda([this]{ return Show(Page != ERiverfrontLifeMenu::Wardrobe && !Selected.IsNone() && !Owned.Contains(Selected)); })
            .OnClicked_Lambda([this,bBank]{
                const FText Error = RiverfrontLife::Buy(Slot,Selected,bBank);
                return Complete(Error, FString(), true);
            })
            [
                SNew(STextBlock).ColorAndOpacity(bBank ? Paper : Panel)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold",14))
                .Text_Lambda([this,bBank]() -> FText {
                    const auto* Chosen = Item(Selected);
                    if (!Chosen) return FText::GetEmpty();
                    return Copy(FString::Printf(TEXT("%s · %s"), bBank ? TEXT("Bank") : TEXT("Cash"), *Rupees(Chosen->PriceRupees)));
                })
            ];
    }

    TSharedRef<SWidget> ViewButton(EFittingView Mode, FName Tag, FString LabelText)
    {
        const auto Target=[this,Mode]
        {
            const auto* Choice=Item(Selected);
            if (Mode!=EFittingView::Tailoring || !Choice) return Mode;
            if (Choice->Slot==ERiverfrontWearSlot::Shoes || Choice->Slot==ERiverfrontWearSlot::Socks) return EFittingView::Feet;
            return Choice->Slot==ERiverfrontWearSlot::Bottom || Choice->Slot==ERiverfrontWearSlot::InnerBottom ? EFittingView::Trousers : Mode;
        };
        return SNew(SButton).Tag(Tag).ButtonStyle(&ButtonStyle).ButtonColorAndOpacity(Row)
            .ContentPadding(FMargin(8,7)).HAlign(HAlign_Center)
            .OnClicked_Lambda([this,Target]
            {
                const EFittingView Next=Target()==ViewMode ? EFittingView::Full : Target();
                auto* Controller = Cast<ARiverfrontPlayerController>(UGameplayStatics::GetPlayerController(AudioWorld.Get(),0));
                if (Controller && Controller->SetFittingView(Next))
                {
                    ViewMode=Next;
                    if (Next!=EFittingView::Full && !InspectionTimer.IsValid())
                        InspectionTimer=RegisterActiveTimer(1.f/24.f,FWidgetActiveTimerDelegate::CreateSP(this,&SRiverfrontLifeMenu::InspectIdle));
                }
                else Feedback=Copy(TEXT("There isn't room for a close-up here."));
                return FReply::Handled();
            })
            [SNew(STextBlock).ColorAndOpacity(Paper).Font(FCoreStyle::GetDefaultFontStyle("Regular",13))
                .Text_Lambda([this,Target,LabelText]{ return Copy(Target()==ViewMode ? FString(TEXT("Full outfit")) : LabelText); })];
    }

    EActiveTimerReturnType InspectIdle(double, float DeltaSeconds)
    {
        if (ViewMode==EFittingView::Full)
        {
            InspectionTimer.Reset();
            return EActiveTimerReturnType::Stop;
        }
        if (!DressingTimer.IsValid())
            if (auto* Walker=Resident()) Walker->AdvanceFittingIdle(DeltaSeconds);
        return EActiveTimerReturnType::Continue;
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

    FName Cover() const
    {
        const auto* Choice=Item(Selected);
        return Choice ? RiverfrontLife::CoveringItem(Choice->Slot,Outfit,Bottom,Shoes) : NAME_None;
    }

    FName WornInSlot() const
    {
        const auto* Choice=Item(Selected);
        if (Choice && Choice->Slot==ERiverfrontWearSlot::Shoes) return Shoes;
        if (Choice && Choice->Slot==ERiverfrontWearSlot::Socks) return Socks;
        if (Choice && Choice->Slot==ERiverfrontWearSlot::InnerTop) return InnerTop;
        if (Choice && Choice->Slot==ERiverfrontWearSlot::InnerBottom) return InnerBottom;
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
        if (const FName Outer=Cover(); !Outer.IsNone()) return Copy(TEXT("Take off ")+ItemLabel(Outer)+TEXT(" first"));
        switch (NextAction())
        {
        case EAction::TakeOff: return Copy(TEXT("Take off ")+ItemLabel(WornInSlot()));
        case EAction::Take: return Copy(Page!=ERiverfrontLifeMenu::Wardrobe ? TEXT("Take from shelf") : TEXT("Take from wardrobe"));
        default: return Copy(TEXT("Put on ")+ItemLabel(Held));
        }
    }

    FReply FinishAction(EAction Action,FName Id)
    {
        const FString Name=ItemLabel(Id);
        const FText Error=Trial.IsValid() ? RiverfrontLife::TryOn(*Trial.Get(),Action,Id) : RiverfrontLife::Dress(Slot,Action,Id);
        const FString Message=Action==EAction::Take ? Name+TEXT(" in hand.") :
            Action==EAction::PutOn ? (Trial.IsValid() && !Owned.Contains(Id) ? TEXT("Trying on ") : TEXT("Wearing "))+Name+TEXT(".") : Name+TEXT(" returned to the shelf.");
        return Complete(Error,Message);
    }

    EActiveTimerReturnType AdvanceDressing(double Now,float)
    {
        auto* Walker=Resident();
        const float Time=FMath::Clamp(float((Now-DressStageStart)/((bChangingBottom || bChangingFeet) ? 3.25 : RiverfrontLife::IsPullover(PendingItem) ? 1.65 : 1.85)),0.f,1.f);
        if (Walker && Time<1.f)
        {
            const auto* ChangingItem=Item(PendingItem);
            if (bChangingBottom || bChangingFeet || (ChangingItem && ChangingItem->Slot==ERiverfrontWearSlot::InnerTop))
            {
                const float Undress=PendingAction==EAction::TakeOff ? Time : 1.f-Time;
                if (bChangingBottom) Walker->PoseChangingTrousers(PendingItem,Undress);
                else if (bChangingFeet) Walker->PoseChangingFootwear(PendingItem,Undress);
                else Walker->PoseChangingInnerTop(PendingItem,Undress);
                if (auto* Controller=Cast<ARiverfrontPlayerController>(Walker->GetController())) Controller->RefreshWardrobeMirror();
                return EActiveTimerReturnType::Continue;
            }
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
            Walker->PoseChangingSleeves(Lift,RiverfrontLife::IsPullover(PendingItem) ? 1.f : 0.f,SleevePass);
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
        if (DressingTimer.IsValid() || !Cover().IsNone() || (!Trial.IsValid() && !Owned.Contains(Selected)) || (!Held.IsNone() && Selected!=Held))
            return FReply::Handled();
        const auto Action=NextAction();
        const FName Id=Action==EAction::TakeOff ? WornInSlot() : Selected;
        const auto* Choice=Item(Id);
        auto* Walker=Resident();
        if (Action==EAction::Take || !Choice || Choice->Slot==ERiverfrontWearSlot::Accessory || !Walker || !Walker->SupportsClothingChange())
            return FinishAction(Action,Id);
        bChangingFeet=Choice->Slot==ERiverfrontWearSlot::Shoes || Choice->Slot==ERiverfrontWearSlot::Socks;
        bChangingBottom=Choice->Slot==ERiverfrontWearSlot::Bottom || Choice->Slot==ERiverfrontWearSlot::InnerBottom;
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
        if (Trial.IsValid())
        {
            if (auto* Walker=Resident())
            {
                Walker->ApplyProfileAppearance(*Trial.Get());
                Walker->PoseHeldWardrobeItem();
                if (auto* PC=Cast<ARiverfrontPlayerController>(Walker->GetController())) PC->RefreshWardrobeMirror();
            }
        }
        else OnChanged.ExecuteIfBound();
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
