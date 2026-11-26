#include "RiverfrontLifeProfile.h"
#include "RiverfrontProfileSlot.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMutex.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Internationalization.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"

#if PLATFORM_MAC
FString RiverfrontMacSaveDirectory();
#endif

#define LOCTEXT_NAMESPACE "SabarmatiLife"

namespace
{
// The source launcher and the packaged app can open the same profile simultaneously.
UE::FPlatformSystemWideMutex LockWriter()
{
    // Unreal places named Mac locks in its settings folder. A fresh offline installation
    // can reach its first profile creation before any other engine system creates it.
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    Files.CreateDirectoryTree(FPlatformProcess::ApplicationSettingsDir());
    return UE::FPlatformSystemWideMutex(TEXT("SabarmatiDrive-ProfileWriter"), FTimespan::FromMilliseconds(50));
}
const FName StarterOutfit(TEXT("cotton-sand"));

URiverfrontLocalProfile* Supported(URiverfrontLocalProfile* Profile)
{
    if (!IsValid(Profile) || URiverfrontLocalProfile::CleanDisplayName(Profile->DisplayName).IsEmpty())
    {
        return nullptr;
    }
    return (Profile->Schema == 1 || Profile->Schema == URiverfrontLocalProfile::CurrentSchema) ? Profile : nullptr;
}

void StartLife(URiverfrontLocalProfile& Profile)
{
    Profile.CashRupees = 100000;
    Profile.BankRupees = 500000;
    Profile.OwnedItems = { StarterOutfit };
    Profile.EquippedOutfit = StarterOutfit;
    Profile.Schema = URiverfrontLocalProfile::CurrentSchema;
}

FText Update(const FString& Slot, TFunctionRef<FText(URiverfrontLocalProfile&)> Change)
{
    const auto Lock = LockWriter();
    if (!Lock.IsValid())
    {
        return LOCTEXT("Busy", "Your profile is busy. Try again.");
    }
    const FRiverfrontProfileSlot Storage = FRiverfrontProfileSlot::ForLaunch(Slot);
    URiverfrontLocalProfile* Profile = Storage.Read();
    if (!Profile)
    {
        return LOCTEXT("CouldNotRead", "Could not open your local profile.");
    }
    if (Profile->Schema == 1)
    {
        StartLife(*Profile);
    }
    FText Problem = Change(*Profile);
    if (!Problem.IsEmpty())
    {
        return Problem;
    }
    return Storage.Write(*Profile) ? FText::GetEmpty() : LOCTEXT("CouldNotWrite", "Could not save these changes.");
}

const FRiverfrontWearable* FindItem(FName Id)
{
    return RiverfrontLife::Catalog().FindByPredicate([Id](const FRiverfrontWearable& Item)
    {
        return Item.Id == Id && Item.PriceRupees >= 0;
    });
}

FText Price(int32 Rupees)
{
    return FText::AsNumber(Rupees, nullptr, FInternationalization::Get().GetCulture(TEXT("en-IN")));
}
}

const TArray<FRiverfrontWearable>& RiverfrontLife::Catalog()
{
    return GetDefault<URiverfrontWardrobeCatalog>()->Items;
}

FRiverfrontProfileSlot FRiverfrontProfileSlot::ForLaunch(const FString& Slot)
{
#if PLATFORM_MAC
    FString UserDir;
    if (Slot == TEXT("SabarmatiPlayerProfileV1") &&
        !FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDir))
    {
        return { Slot, RiverfrontMacSaveDirectory() };
    }
#endif
    return { Slot, {} };
}

bool FRiverfrontProfileSlot::Exists() const
{
    if (DirectSaveDirectory.IsSet())
    {
        const FString& Directory = DirectSaveDirectory.GetValue();
        if (Directory.IsEmpty() ||
            FPlatformFileManager::Get().GetPlatformFile().FileExists(*(Directory / (Name + TEXT(".sav"))))) return true;
    }
    return UGameplayStatics::DoesSaveGameExist(Name, 0);
}

URiverfrontLocalProfile* FRiverfrontProfileSlot::Read(bool* bFromLegacyDirectory) const
{
    if (bFromLegacyDirectory) *bFromLegacyDirectory = false;
    URiverfrontLocalProfile* Profile = nullptr;
    if (DirectSaveDirectory.IsSet())
    {
        const FString& Directory = DirectSaveDirectory.GetValue();
        if (Directory.IsEmpty()) return nullptr;
        const FString Canonical = Directory / (Name + TEXT(".sav"));
        if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*Canonical))
        {
            TArray<uint8> Bytes;
            if (!FFileHelper::LoadFileToArray(Bytes, *Canonical)) return nullptr;
            Profile = Cast<URiverfrontLocalProfile>(UGameplayStatics::LoadGameFromMemory(Bytes));
            return Supported(Profile);
        }
        if (bFromLegacyDirectory) *bFromLegacyDirectory = true;
    }
    Profile = Cast<URiverfrontLocalProfile>(UGameplayStatics::LoadGameFromSlot(Name, 0));
    return Supported(Profile);
}

bool FRiverfrontProfileSlot::Write(URiverfrontLocalProfile& Profile) const
{
    const FString Directory = DirectSaveDirectory.Get(FPaths::ProjectSavedDir() / TEXT("SaveGames"));
    if (Directory.IsEmpty()) return false;
    const FString Pending = Name + TEXT(".pending");
    const FString PendingFile = Directory / (Pending + TEXT(".sav"));
    const FString FinalFile = Directory / (Name + TEXT(".sav"));
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    bool bStaged;
    if (DirectSaveDirectory.IsSet())
    {
        TArray<uint8> Bytes;
        bStaged = Files.CreateDirectoryTree(*Directory) &&
            UGameplayStatics::SaveGameToMemory(&Profile, Bytes) && FFileHelper::SaveArrayToFile(Bytes, *PendingFile);
    }
    else
    {
        bStaged = UGameplayStatics::SaveGameToSlot(&Profile, Pending, 0);
    }
    // Both Mac entry points use the same writer lock and publish complete Unreal saves by rename.
    if (bStaged && Files.MoveFile(*FinalFile, *PendingFile)) return true;
    Files.DeleteFile(*PendingFile);
    return false;
}

bool RiverfrontLife::Exists(const FString& Slot)
{
    return FRiverfrontProfileSlot::ForLaunch(Slot).Exists();
}

URiverfrontLocalProfile* FRiverfrontProfileSlot::Load() const
{
    bool bFromLegacy = false;
    URiverfrontLocalProfile* Profile = Read(&bFromLegacy);
    if (!Profile || (!bFromLegacy && Profile->Schema == URiverfrontLocalProfile::CurrentSchema)) return Profile;
    // Check again under the writer lock. Another launch can migrate or spend money between reads.
    const auto Lock = LockWriter();
    if (!Lock.IsValid()) return nullptr;
    Profile = Read(&bFromLegacy);
    if (Profile && (bFromLegacy || Profile->Schema == 1))
    {
        if (Profile->Schema == 1) StartLife(*Profile);
        if (!Write(*Profile)) return nullptr;
    }
    return Profile;
}

URiverfrontLocalProfile* RiverfrontLife::Load(const FString& Slot)
{
    return FRiverfrontProfileSlot::ForLaunch(Slot).Load();
}

bool RiverfrontLife::Create(const FString& Slot, const FString& DisplayName, ERiverfrontGender Gender)
{
    const FString CleanName = URiverfrontLocalProfile::CleanDisplayName(DisplayName);
    const auto Lock = LockWriter();
    const FRiverfrontProfileSlot Storage = FRiverfrontProfileSlot::ForLaunch(Slot);
    if (CleanName.IsEmpty() || (Gender != ERiverfrontGender::Male && Gender != ERiverfrontGender::Female) ||
        !Lock.IsValid() || Storage.Exists())
    {
        return false;
    }
    auto* Profile = CastChecked<URiverfrontLocalProfile>(
        UGameplayStatics::CreateSaveGameObject(URiverfrontLocalProfile::StaticClass()));
    Profile->DisplayName = CleanName;
    Profile->AvatarGender = Gender;
    StartLife(*Profile);
    return Storage.Write(*Profile);
}

bool RiverfrontLife::MergeExploration(const FString& Slot, const FString& DisplayName, const TArray<FName>& Visits, float BestTime)
{
    return Update(Slot, [&](URiverfrontLocalProfile& Profile)
    {
        if (URiverfrontLocalProfile::CleanDisplayName(Profile.DisplayName) != DisplayName)
        {
            return LOCTEXT("WrongProfile", "This profile belongs to a different player.");
        }
        for (const FName Site : Visits) Profile.VisitedSites.AddUnique(Site);
        if (BestTime > 0.f && (Profile.BestRiverRunSeconds <= 0.f || BestTime < Profile.BestRiverRunSeconds))
        {
            Profile.BestRiverRunSeconds = BestTime;
        }
        return FText::GetEmpty();
    }).IsEmpty();
}

FText RiverfrontLife::Buy(const FString& Slot, FName Id, bool bPayFromBank)
{
    const FRiverfrontWearable* Item = FindItem(Id);
    if (!Item) return LOCTEXT("UnknownItem", "This item is unavailable.");
    return Update(Slot, [&](URiverfrontLocalProfile& Profile)
    {
        if (!Profile.OwnedItems.Contains(Id))
        {
            int64& Account = bPayFromBank ? Profile.BankRupees : Profile.CashRupees;
            if (Account < Item->PriceRupees)
            {
                return LOCTEXT("Funds", "Insufficient funds in the selected account.");
            }
            Account -= Item->PriceRupees;
            Profile.OwnedItems.Add(Id);
            const FText Format = bPayFromBank
                ? LOCTEXT("BankPurchase", "{0} · ₹{1} paid from bank")
                : LOCTEXT("CashPurchase", "{0} · ₹{1} paid in cash");
            Profile.LastReceipt = FText::Format(Format, FText::FromString(Item->Label), Price(Item->PriceRupees)).ToString();
        }
        return FText::GetEmpty();
    });
}

FText RiverfrontLife::Dress(const FString& Slot, EDressingAction Action, FName Id)
{
    const auto* Item = FindItem(Id);
    if (!Item) return LOCTEXT("UnknownItem", "This item is unavailable.");
    return Update(Slot, [&](URiverfrontLocalProfile& Profile)
    {
        if (!Profile.OwnedItems.Contains(Id)) return LOCTEXT("NotOwned", "Purchase this item first.");
        FName& Worn = Item->bAccessory ? Profile.EquippedAccessory : Profile.EquippedOutfit;
        // A missing outfit in an older save was displayed as the starter shirt.
        if (!Item->bAccessory && Worn.IsNone()) Worn=StarterOutfit;
        const FName Empty = Item->bAccessory ? NAME_None : BaseLayer;
        switch (Action)
        {
        case EDressingAction::TakeOff:
            if (Worn != Id) return LOCTEXT("NotWorn", "Select the item you are wearing.");
            Worn = Empty;
            break;
        case EDressingAction::Take:
            if (!Profile.HeldWardrobeItem.IsNone()) return LOCTEXT("HandsFull", "Put the item in your hand back first.");
            if (Worn != Empty) return LOCTEXT("SlotOccupied", "Take off the item you are wearing first.");
            Profile.HeldWardrobeItem = Id;
            break;
        case EDressingAction::PutOn:
            if (Profile.HeldWardrobeItem != Id) return LOCTEXT("NotHeld", "Take the item into your hand first.");
            if (Worn != Empty) return LOCTEXT("SlotOccupied", "Take off the item you are wearing first.");
            Worn = Id;
            Profile.HeldWardrobeItem = NAME_None;
            break;
        case EDressingAction::PutBack:
            if (Profile.HeldWardrobeItem != Id) return LOCTEXT("NotHeld", "Take the item into your hand first.");
            Profile.HeldWardrobeItem = NAME_None;
            break;
        }
        return FText::GetEmpty();
    });
}

FText RiverfrontLife::SetAvatarGender(const FString& Slot, ERiverfrontGender Gender)
{
    if (Gender != ERiverfrontGender::Male && Gender != ERiverfrontGender::Female)
        return LOCTEXT("UnknownGender", "That avatar is unavailable.");
    return Update(Slot, [Gender](URiverfrontLocalProfile& Profile)
    {
        Profile.AvatarGender = Gender;
        return FText::GetEmpty();
    });
}

FText RiverfrontLife::SetAppearanceTone(const FString& Slot, FName Tone)
{
    if (!Tone.IsNone() && Tone != TEXT("warm") && Tone != TEXT("umber"))
    {
        return LOCTEXT("UnavailableTone", "This complexion is unavailable.");
    }
    return Update(Slot, [Tone](URiverfrontLocalProfile& Profile)
    {
        Profile.AppearanceTone = Tone;
        return FText::GetEmpty();
    });
}

FText RiverfrontLife::SetCroppedHair(const FString& Slot, bool bCropped)
{
    return Update(Slot, [bCropped](URiverfrontLocalProfile& Profile)
    {
        Profile.bCroppedHair = bCropped;
        return FText::GetEmpty();
    });
}

bool RiverfrontLife::SaveCheckpoint(const FString& Slot, FName District, FVector Location, float Yaw, bool bFinishedWakeup)
{
    if (District.IsNone() || Location.ContainsNaN() || !FMath::IsFinite(Yaw)) return false;
    return Update(Slot, [&](URiverfrontLocalProfile& Profile)
    {
        Profile.CheckpointDistrict = District;
        Profile.CheckpointLocation = Location;
        Profile.CheckpointYaw = FRotator::NormalizeAxis(Yaw);
        Profile.bHasLifeCheckpoint = true;
        Profile.bFirstMorningSeen |= bFinishedWakeup;
        return FText::GetEmpty();
    }).IsEmpty();
}

FText RiverfrontLife::StartPrintDelivery(const FString& Slot)
{
    return Update(Slot, [](URiverfrontLocalProfile& Profile)
    {
        if (Profile.CompletedPrintDeliveries > 0) return LOCTEXT("AlreadyDelivered", "This print order has already been delivered.");
        Profile.bPrintDeliveryActive = true;
        return FText::GetEmpty();
    });
}

FText RiverfrontLife::CompletePrintDelivery(const FString& Slot)
{
    return Update(Slot, [](URiverfrontLocalProfile& Profile)
    {
        if (Profile.CompletedPrintDeliveries > 0) return LOCTEXT("AlreadyPaid", "This delivery has already been paid.");
        if (!Profile.bPrintDeliveryActive) return LOCTEXT("NoOrder", "Pick up the order at Kagaz Print House first.");
        Profile.BankRupees += 850;
        ++Profile.CompletedPrintDeliveries;
        Profile.bPrintDeliveryActive = false;
        Profile.LastReceipt = LOCTEXT("DeliveryReceipt", "Kagaz print delivery · ₹850 paid into bank").ToString();
        return FText::GetEmpty();
    });
}

#undef LOCTEXT_NAMESPACE
