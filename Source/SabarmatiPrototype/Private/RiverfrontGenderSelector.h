#pragma once

#include "RiverfrontResidentAppearance.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

DECLARE_DELEGATE_OneParam(FOnRiverfrontGenderSelected, ERiverfrontGender);

// Profile creation holds the choice locally; the apartment wardrobe binds it to the save.
class SRiverfrontGenderSelector final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRiverfrontGenderSelector) : _Selected(ERiverfrontGender::Male) {}
        SLATE_ATTRIBUTE(ERiverfrontGender, Selected)
        SLATE_ARGUMENT(FName, MaleTag)
        SLATE_ARGUMENT(FName, FemaleTag)
        SLATE_EVENT(FOnRiverfrontGenderSelected, OnSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args)
    {
        Selected = Args._Selected;
        OnSelected = Args._OnSelected;
        const FLinearColor Saffron(FColor(243,187,114));
        const FLinearColor Ivory(FColor(246,241,232));
        RadioStyle = FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("RadioButton");
        RadioStyle.UncheckedImage.TintColor = Ivory.CopyWithNewOpacity(.75f);
        RadioStyle.UncheckedHoveredImage.TintColor = Ivory;
        RadioStyle.UncheckedPressedImage.TintColor = Saffron;
        RadioStyle.CheckedImage.TintColor = Saffron;
        RadioStyle.CheckedHoveredImage.TintColor = Ivory;
        RadioStyle.CheckedPressedImage.TintColor = Saffron;
        ChildSlot
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,18,0)
            [
                SNew(STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                .ColorAndOpacity(Saffron)
                .Text(NSLOCTEXT("SabarmatiAvatar", "GenderLabel", "AVATAR"))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)
            [Choice(ERiverfrontGender::Male, Args._MaleTag, NSLOCTEXT("SabarmatiAvatar", "Male", "Male"))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Choice(ERiverfrontGender::Female, Args._FemaleTag, NSLOCTEXT("SabarmatiAvatar", "Female", "Female"))]
        ];
    }

private:
    class SAvatarRadio final : public SCheckBox
    {
        const FSlateRoundedBoxBrush FocusBorder{FLinearColor::Transparent, 3.f, FLinearColor(FColor(243,187,114)), 1.5f};
        const FSlateBrush* GetFocusBrush() const override { return &FocusBorder; }
    };

    FCheckBoxStyle RadioStyle;
    TAttribute<ERiverfrontGender> Selected;
    FOnRiverfrontGenderSelected OnSelected;

    TSharedRef<SWidget> Choice(ERiverfrontGender Value, FName Tag, const FText& Name)
    {
        return SNew(SAvatarRadio)
            .Tag(Tag)
            .Style(&RadioStyle)
            .Padding(FMargin(6,5))
            .IsChecked_Lambda([this, Value]
            {
                return Selected.Get() == Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([this, Value](ECheckBoxState State)
            {
                if (State == ECheckBoxState::Checked)
                    OnSelected.ExecuteIfBound(Value);
            })
            [
                SNew(STextBlock).Text(Name).ColorAndOpacity(FLinearColor(FColor(246,241,232)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
            ];
    }
};
