#if WITH_DEV_AUTOMATION_TESTS

#include "RiverfrontLifeProfile.h"
#include "RiverfrontProfileSlot.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSabarmatiOfflineLife,
    "Sabarmati.FirstMorning.OfflineProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSabarmatiOfflineLife::RunTest(const FString& Parameters)
{
    const FString Slot = TEXT("SabarmatiQA-Life-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Fresh = Slot + TEXT("-New");
    const FString PendingFile = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".pending.sav"));
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    ON_SCOPE_EXIT
    {
        Files.DeleteDirectory(*PendingFile);
        UGameplayStatics::DeleteGameInSlot(Slot, 0);
        UGameplayStatics::DeleteGameInSlot(Fresh, 0);
    };

    const FString Fixture = FPaths::ProjectDir() / TEXT("Tests/Fixtures/riverfront-profile-v1.sav");
    TArray<uint8> LegacyBytes;
    if (!TestTrue(TEXT("read genuine pre-upgrade standalone save"), FFileHelper::LoadFileToArray(LegacyBytes, *Fixture))) return false;
    const auto* Old = Cast<URiverfrontLocalProfile>(UGameplayStatics::LoadGameFromMemory(LegacyBytes));
    if (!TestNotNull(TEXT("load V1 fixture"), Old)) return false;
    TestEqual(TEXT("omitted original schema defaults to V1"), Old->Schema, 1);
    TestEqual(TEXT("synthetic review profile"), Old->DisplayName, FString(TEXT("Systems Review")));
    TestTrue(TEXT("original profile contains discovery and best run"), Old->VisitedSites.Num() > 0 && Old->BestRiverRunSeconds > 0.f);
    if (!TestTrue(TEXT("seed isolated slot"), UGameplayStatics::SaveDataToSlot(LegacyBytes, Slot, 0))) return false;

    auto* Profile = RiverfrontLife::Load(Slot);
    if (!TestNotNull(TEXT("migrated save"), Profile)) return false;
    TestEqual(TEXT("current schema"), Profile->Schema, URiverfrontLocalProfile::CurrentSchema);
    TestTrue(TEXT("original visits"), Profile->VisitedSites == Old->VisitedSites);
    TestEqual(TEXT("original trial"), Profile->BestRiverRunSeconds, Old->BestRiverRunSeconds);
    TestEqual(TEXT("starting cash"), Profile->CashRupees, int64(100000));
    TestEqual(TEXT("starting bank"), Profile->BankRupees, int64(500000));
    TestFalse(TEXT("wake pending"), Profile->bFirstMorningSeen);
    TestTrue(TEXT("reference catalogue"), RiverfrontLife::Catalog().Num() >= 4);
    TestTrue(TEXT("unowned watch cannot be equipped"), !RiverfrontLife::Equip(Slot, TEXT("watch")).IsEmpty());

    TestTrue(TEXT("buy indigo shirt for cash"), RiverfrontLife::BuyAndEquip(Slot, TEXT("shirt-indigo"), false).IsEmpty());
    TestTrue(TEXT("repeat request"), RiverfrontLife::BuyAndEquip(Slot, TEXT("shirt-indigo"), false).IsEmpty());
    Profile = RiverfrontLife::Load(Slot);
    if (!TestNotNull(TEXT("reload purchase"), Profile)) return false;
    TestEqual(TEXT("cash debited once"), Profile->CashRupees, int64(98750));
    TestEqual(TEXT("bank untouched"), Profile->BankRupees, int64(500000));
    TestEqual(TEXT("shirt equipped"), Profile->EquippedOutfit, FName(TEXT("shirt-indigo")));
    TestTrue(TEXT("receipt"), Profile->LastReceipt.Contains(TEXT("1,250")));

    // Force the stage-file write to fail inside our own unique slot.
    if (!TestTrue(TEXT("create intentionally unavailable staging destination"), Files.CreateDirectory(*PendingFile))) return false;
    TestTrue(TEXT("failed purchase reported"), !RiverfrontLife::BuyAndEquip(Slot, TEXT("watch"), true).IsEmpty());
    Files.DeleteDirectory(*PendingFile);
    Profile = RiverfrontLife::Load(Slot);
    if (!TestNotNull(TEXT("old save survives failed purchase"), Profile)) return false;
    TestEqual(TEXT("failed purchase did not debit bank"), Profile->BankRupees, int64(500000));
    TestFalse(TEXT("failed purchase did not grant watch"), Profile->OwnedItems.Contains(TEXT("watch")));

    TestTrue(TEXT("retry bank purchase"), RiverfrontLife::BuyAndEquip(Slot, TEXT("watch"), true).IsEmpty());
    TestTrue(TEXT("set complexion"), RiverfrontLife::SetAppearanceTone(Slot, TEXT("umber")).IsEmpty());
    TestTrue(TEXT("choose cropped haircut"), RiverfrontLife::SetCroppedHair(Slot, true).IsEmpty());
    TestTrue(TEXT("save apartment checkpoint"), RiverfrontLife::SaveCheckpoint(Slot, TEXT("riverfront"), FVector(100, 200, 125), 70.f));
    TestTrue(TEXT("merge legacy exploration"), RiverfrontLife::MergeExploration(Slot, Old->DisplayName, {FName(TEXT("atelier"))}, 0.f));
    TestTrue(TEXT("no unearned delivery reward"), !RiverfrontLife::CompletePrintDelivery(Slot).IsEmpty());
    TestTrue(TEXT("start delivery"), RiverfrontLife::StartPrintDelivery(Slot).IsEmpty());
    Profile = RiverfrontLife::Load(Slot);
    if (!TestNotNull(TEXT("load parcel state"), Profile)) return false;
    TestTrue(TEXT("active parcel survives reopening"), Profile->bPrintDeliveryActive);
    TestTrue(TEXT("deliver and receive money"), RiverfrontLife::CompletePrintDelivery(Slot).IsEmpty());
    TestTrue(TEXT("cannot collect two payouts"), !RiverfrontLife::CompletePrintDelivery(Slot).IsEmpty());
    TestTrue(TEXT("completed order cannot restart"), !RiverfrontLife::StartPrintDelivery(Slot).IsEmpty());
    Profile = RiverfrontLife::Load(Slot);
    if (!TestNotNull(TEXT("load full life progress"), Profile)) return false;
    TestEqual(TEXT("watch and delivery net posted once"), Profile->BankRupees, int64(499050));
    TestEqual(TEXT("no second starting grant"), Profile->CashRupees, int64(98750));
    TestEqual(TEXT("delivery count"), Profile->CompletedPrintDeliveries, 1);
    TestEqual(TEXT("accessory persisted"), Profile->EquippedAccessory, FName(TEXT("watch")));
    TestTrue(TEXT("cropped hairstyle survives reopening"), Profile->bCroppedHair);
    TestEqual(TEXT("complexion persisted"), Profile->AppearanceTone, FName(TEXT("umber")));
    TestTrue(TEXT("wake complete and saved"), Profile->bFirstMorningSeen && Profile->bHasLifeCheckpoint);
    TestEqual(TEXT("resume position"), Profile->CheckpointLocation, FVector(100, 200, 125));
    TestTrue(TEXT("exploration survives independent shop and delivery saves"), Profile->VisitedSites.Contains(TEXT("atelier")));
    TestEqual(TEXT("original best survives all operations"), Profile->BestRiverRunSeconds, Old->BestRiverRunSeconds);

    TestTrue(TEXT("create a new player"), RiverfrontLife::Create(Fresh, TEXT("New Player")));
    TestFalse(TEXT("repeated creation cannot reset a save"), RiverfrontLife::Create(Fresh, TEXT("New Player")));
    Profile = RiverfrontLife::Load(Fresh);
    if (!TestNotNull(TEXT("reload new player"), Profile)) return false;
    TestEqual(TEXT("new player cash"), Profile->CashRupees, int64(100000));
    TestEqual(TEXT("new player bank"), Profile->BankRupees, int64(500000));
    Profile->CashRupees = 30;
    TestTrue(TEXT("low funds fixture"), UGameplayStatics::SaveGameToSlot(Profile, Fresh, 0));
    TestTrue(TEXT("insufficient balance fails purchase"), !RiverfrontLife::BuyAndEquip(Fresh, TEXT("shirt-indigo"), false).IsEmpty());
    Profile = RiverfrontLife::Load(Fresh);
    if (!TestNotNull(TEXT("low funds remain readable"), Profile)) return false;
    TestEqual(TEXT("exact funds retained on refusal"), Profile->CashRupees, int64(30));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSabarmatiFinderProfile,
    "Sabarmati.FirstMorning.FinderProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FSabarmatiFinderProfile::RunTest(const FString& Parameters)
{
    const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Slot = TEXT("SabarmatiQA-Finder-") + Id;
    // Inject a synthetic canonical folder so the test never opens a real user's save.
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FinderProfile") / Id;
    const FString SharedFile = Directory / (Slot + TEXT(".sav"));
    const FRiverfrontProfileSlot Finder{Slot, Directory};
    IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
    ON_SCOPE_EXIT
    {
        Files.DeleteDirectoryRecursively(*Directory);
        UGameplayStatics::DeleteGameInSlot(Slot, 0);
    };

    TestFalse(TEXT("QA is always isolated"), FRiverfrontProfileSlot::ForLaunch(Slot).DirectSaveDirectory.IsSet());
    TestFalse(TEXT("new Finder slot"), Finder.Exists());
    TArray<uint8> OldBytes;
    if (!TestTrue(TEXT("load legacy test fixture"), FFileHelper::LoadFileToArray(OldBytes,
        *(FPaths::ProjectDir() / TEXT("Tests/Fixtures/riverfront-profile-v1.sav"))))) return false;
    if (!TestTrue(TEXT("install old Finder V1 profile"), UGameplayStatics::SaveDataToSlot(OldBytes, Slot, 0))) return false;
    TestTrue(TEXT("old Finder profile is visible to title"), Finder.Exists());
    auto* Migrated = Finder.Load();
    if (!TestNotNull(TEXT("restore legacy Finder profile"), Migrated)) return false;
    TestEqual(TEXT("upgrade old file while promoting"), Migrated->Schema, URiverfrontLocalProfile::CurrentSchema);
    TestEqual(TEXT("one opening credit"), Migrated->CashRupees, int64(100000));
    TestFalse(TEXT("legacy hairstyle defaults to swept"), Migrated->bCroppedHair);
    TestTrue(TEXT("Finder promoted to shared folder"), Files.FileExists(*SharedFile));
    TArray<uint8> Original;
    TestTrue(TEXT("source still present"), UGameplayStatics::LoadDataFromSlot(Original, Slot, 0));
    TestTrue(TEXT("original V1 bytes unmodified"), OldBytes == Original);

    // A pre-existing V2 Finder save must be promoted even though no schema upgrade is needed.
    Migrated->CashRupees = 9100;
    Migrated->BankRupees = 7600;
    Migrated->bCroppedHair = true;
    if (!TestTrue(TEXT("seed existing V2 Finder progress"), UGameplayStatics::SaveGameToSlot(Migrated, Slot, 0))) return false;
    if (!TestTrue(TEXT("clear shared copy for second migration case"), Files.DeleteFile(*SharedFile))) return false;
    auto* Current = Finder.Load();
    if (!TestNotNull(TEXT("load and promote existing V2"), Current)) return false;
    TestTrue(TEXT("V2 copy materialized"), Files.FileExists(*SharedFile));
    TestEqual(TEXT("V2 cash preserved"), Current->CashRupees, int64(9100));
    TestEqual(TEXT("V2 bank preserved"), Current->BankRupees, int64(7600));
    TestTrue(TEXT("V2 equipped style preserved"), Current->bCroppedHair);

    Current->CashRupees = 350;
    if (!TestTrue(TEXT("write new canonical progress"), Finder.Write(*Current))) return false;
    auto* Continued = Finder.Load();
    if (!TestNotNull(TEXT("canonical progress readable"), Continued)) return false;
    TestEqual(TEXT("canonical wins over stale legacy"), Continued->CashRupees, int64(350));

    Current->Schema = URiverfrontLocalProfile::CurrentSchema + 1;
    if (!TestTrue(TEXT("write future fixture"), Finder.Write(*Current))) return false;
    TArray<uint8> FutureBefore, FutureAfter;
    TestTrue(TEXT("read future bytes before"), FFileHelper::LoadFileToArray(FutureBefore, *SharedFile));
    TestTrue(TEXT("future slot prevents creation"), Finder.Exists());
    TestNull(TEXT("future canonical slot is not replaced with old progress"), Finder.Load());
    TestTrue(TEXT("read future bytes after"), FFileHelper::LoadFileToArray(FutureAfter, *SharedFile));
    TestTrue(TEXT("future save stayed intact"), FutureBefore == FutureAfter);
    return true;
}

#endif
