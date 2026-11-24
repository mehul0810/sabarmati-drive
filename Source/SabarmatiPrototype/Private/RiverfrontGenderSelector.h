#pragma once

#include "RiverfrontResidentAppearance.h"
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
        ChildSlot
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,18,0)
            [
                SNew(STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                .ColorAndOpacity(FLinearColor(.92f, .74f, .48f))
                .Text(NSLOCTEXT("SabarmatiAvatar", "GenderLabel", "AVATAR"))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)
            [Choice(ERiverfrontGender::Male, Args._MaleTag, NSLOCTEXT("SabarmatiAvatar", "Male", "Male"))]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [Choice(ERiverfrontGender::Female, Args._FemaleTag, NSLOCTEXT("SabarmatiAvatar", "Female", "Female"))]
        ];
    }

private:
    TAttribute<ERiverfrontGender> Selected;
    FOnRiverfrontGenderSelected OnSelected;

    TSharedRef<SWidget> Choice(ERiverfrontGender Value, FName Tag, const FText& Name)
    {
        return SNew(SCheckBox)
            .Tag(Tag)
            .Style(FCoreStyle::Get(), "RadioButton")
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
                SNew(STextBlock).Text(Name).ColorAndOpacity(FLinearColor(.95f, .93f, .87f))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
            ];
    }
};
