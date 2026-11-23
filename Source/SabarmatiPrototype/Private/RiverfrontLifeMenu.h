#pragma once

#include "CoreMinimal.h"

class SWidget;
class UWorld;

enum class ERiverfrontLifeMenu : uint8 { Wallet, Wardrobe, Shop };

TSharedRef<SWidget> MakeRiverfrontLifeMenu(UWorld& World, ERiverfrontLifeMenu Page, const FString& ProfileSlot,
    FSimpleDelegate OnChanged, FSimpleDelegate OnClose,
    TFunction<void(FName Outfit, FName Accessory, FName Tone)> Preview = {});
