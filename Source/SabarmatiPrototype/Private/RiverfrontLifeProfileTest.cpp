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
    using RiverfrontLife::Dress;
    using EAction=RiverfrontLife::EDressingAction;
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
    TestEqual(TEXT("existing profile retains the original avatar"), Profile->AvatarGender, ERiverfrontGender::Male);
    TestFalse(TEXT("wake pending"), Profile->bFirstMorningSeen);
    TestTrue(TEXT("reference catalogue"), RiverfrontLife::Catalog().Num() >= 4);
    TestTrue(TEXT("unowned watch cannot be equipped"), !Dress(Slot, EAction::Take, TEXT("watch")).IsEmpty());

    TestFalse(TEXT("take shoes off before socks"),Dress(Slot,EAction::TakeOff,RiverfrontLife::CottonSocks).IsEmpty());
    TestTrue(TEXT("remove shoes"),Dress(Slot,EAction::TakeOff,RiverfrontLife::StreetShoes).IsEmpty());
    TestTrue(TEXT("remove socks"),Dress(Slot,EAction::TakeOff,RiverfrontLife::CottonSocks).IsEmpty());
    TestTrue(TEXT("take shoes while barefoot"),Dress(Slot,EAction::Take,RiverfrontLife::StreetShoes).IsEmpty());
    TestTrue(TEXT("put shoes over bare feet"),Dress(Slot,EAction::PutOn,RiverfrontLife::StreetShoes).IsEmpty());
    TestTrue(TEXT("socks stay off after reload"),RiverfrontLife::Load(Slot)->EquippedSocks.IsNone());
    TestTrue(TEXT("original denim is owned"), Profile->OwnedItems.Contains(RiverfrontLife::StarterBottom));
    TestEqual(TEXT("legacy player starts in their jeans"), Profile->EquippedBottom, RiverfrontLife::StarterBottom);
    TestFalse(TEXT("briefs are under the jeans"), Dress(Slot,EAction::TakeOff,RiverfrontLife::Briefs).IsEmpty());
    TestTrue(TEXT("remove jeans independently"), Dress(Slot,EAction::TakeOff,RiverfrontLife::StarterBottom).IsEmpty());
    TestEqual(TEXT("shirt survives jeans removal"), RiverfrontLife::Load(Slot)->EquippedOutfit,FName(TEXT("cotton-sand")));
    TestEqual(TEXT("underwear saved across reload"), RiverfrontLife::Load(Slot)->EquippedBottom,RiverfrontLife::Underwear);
    TestTrue(TEXT("remove inner layer"), Dress(Slot,EAction::TakeOff,RiverfrontLife::Briefs).IsEmpty());
    TestTrue(TEXT("empty inner slot survives reload"), RiverfrontLife::Load(Slot)->EquippedInnerBottom.IsNone());
    TestFalse(TEXT("cannot equip jeans directly"), Dress(Slot,EAction::PutOn,RiverfrontLife::StarterBottom).IsEmpty());
    TestTrue(TEXT("pick up jeans"), Dress(Slot,EAction::Take,RiverfrontLife::StarterBottom).IsEmpty());
    TestFalse(TEXT("one item in hand across slots"), Dress(Slot,EAction::TakeOff,TEXT("cotton-sand")).IsEmpty());
    TestTrue(TEXT("put jeans back on"), Dress(Slot,EAction::PutOn,RiverfrontLife::StarterBottom).IsEmpty());

    TestTrue(TEXT("replacing denim preserves empty inner slot"),RiverfrontLife::Load(Slot)->EquippedInnerBottom.IsNone());
    TestFalse(TEXT("cannot dress underneath worn jeans"),Dress(Slot,EAction::Take,RiverfrontLife::Briefs).IsEmpty());

    TestTrue(TEXT("buy indigo shirt for cash"), RiverfrontLife::Buy(Slot, TEXT("shirt-indigo"), false).IsEmpty());
    TestTrue(TEXT("repeat request"), RiverfrontLife::Buy(Slot, TEXT("shirt-indigo"), false).IsEmpty());
    Profile = RiverfrontLife::Load(Slot);
    if (!TestNotNull(TEXT("reload purchase"), Profile)) return false;
    TestEqual(TEXT("cash debited once"), Profile->CashRupees, int64(98750));
    TestEqual(TEXT("bank untouched"), Profile->BankRupees, int64(500000));
    TestEqual(TEXT("purchase retains the worn shirt"), Profile->EquippedOutfit, FName(TEXT("cotton-sand")));
    TestFalse(TEXT("cannot take another top before removing this one"), Dress(Slot,EAction::Take,TEXT("shirt-indigo")).IsEmpty());
    TestTrue(TEXT("remove starter top"), Dress(Slot,EAction::TakeOff,TEXT("cotton-sand")).IsEmpty());
    TestEqual(TEXT("base layer persists"), RiverfrontLife::Load(Slot)->EquippedOutfit,RiverfrontLife::BaseLayer);
    TestFalse(TEXT("cannot wear straight from the shelf"), Dress(Slot,EAction::PutOn,TEXT("shirt-indigo")).IsEmpty());
    TestTrue(TEXT("take indigo from the shelf"), Dress(Slot,EAction::Take,TEXT("shirt-indigo")).IsEmpty());
    TestTrue(TEXT("put indigo back"), Dress(Slot,EAction::PutBack,TEXT("shirt-indigo")).IsEmpty());
    TestTrue(TEXT("select and take it again"), Dress(Slot,EAction::Take,TEXT("shirt-indigo")).IsEmpty());
    TestFalse(TEXT("cannot wear another item from the same slot"), Dress(Slot,EAction::PutOn,TEXT("cotton-sand")).IsEmpty());
    TestEqual(TEXT("held top survives reopening"), RiverfrontLife::Load(Slot)->HeldWardrobeItem,FName(TEXT("shirt-indigo")));
    TestTrue(TEXT("put on the held shirt"), Dress(Slot,EAction::PutOn,TEXT("shirt-indigo")).IsEmpty());
    TestTrue(TEXT("receipt"), Profile->LastReceipt.Contains(TEXT("1,250")));

    // Force the stage-file write to fail inside our own unique slot.
    if (!TestTrue(TEXT("create intentionally unavailable staging destination"), Files.CreateDirectory(*PendingFile))) return false;
    TestTrue(TEXT("failed purchase reported"), !RiverfrontLife::Buy(Slot, TEXT("watch"), true).IsEmpty());
    Files.DeleteDirectory(*PendingFile);
    Profile = RiverfrontLife::Load(Slot);
    if (!TestNotNull(TEXT("old save survives failed purchase"), Profile)) return false;
    TestEqual(TEXT("failed purchase did not debit bank"), Profile->BankRupees, int64(500000));
    TestFalse(TEXT("failed purchase did not grant watch"), Profile->OwnedItems.Contains(TEXT("watch")));

    TestTrue(TEXT("retry bank purchase"), RiverfrontLife::Buy(Slot, TEXT("watch"), true).IsEmpty());
    TestTrue(TEXT("pick up watch"), Dress(Slot,EAction::Take,TEXT("watch")).IsEmpty());
    TestTrue(TEXT("put on watch"), Dress(Slot,EAction::PutOn,TEXT("watch")).IsEmpty());
    TestTrue(TEXT("set complexion"), RiverfrontLife::SetAppearanceTone(Slot, TEXT("umber")).IsEmpty());
    TestTrue(TEXT("change avatar in an existing profile"), RiverfrontLife::SetAvatarGender(Slot, ERiverfrontGender::Female).IsEmpty());
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
    TestEqual(TEXT("chosen avatar survives wardrobe edits and reopening"), Profile->AvatarGender, ERiverfrontGender::Female);
    TestTrue(TEXT("cropped hairstyle survives reopening"), Profile->bCroppedHair);
    TestEqual(TEXT("complexion persisted"), Profile->AppearanceTone, FName(TEXT("umber")));
    TestTrue(TEXT("wake complete and saved"), Profile->bFirstMorningSeen && Profile->bHasLifeCheckpoint);
    TestEqual(TEXT("resume position"), Profile->CheckpointLocation, FVector(100, 200, 125));
    TestTrue(TEXT("exploration survives independent shop and delivery saves"), Profile->VisitedSites.Contains(TEXT("atelier")));
    TestEqual(TEXT("original best survives all operations"), Profile->BestRiverRunSeconds, Old->BestRiverRunSeconds);

    TestTrue(TEXT("create a new player"), RiverfrontLife::Create(Fresh, TEXT("New Player"), ERiverfrontGender::Female));
    TestFalse(TEXT("repeated creation cannot reset a save"), RiverfrontLife::Create(Fresh, TEXT("New Player")));
    Profile = RiverfrontLife::Load(Fresh);
    if (!TestNotNull(TEXT("reload new player"), Profile)) return false;
    TestEqual(TEXT("avatar chosen at creation"), Profile->AvatarGender, ERiverfrontGender::Female);
    TestEqual(TEXT("new player cash"), Profile->CashRupees, int64(100000));
    TestEqual(TEXT("new player bank"), Profile->BankRupees, int64(500000));
    Profile->CashRupees = 30;
    TestTrue(TEXT("low funds fixture"), UGameplayStatics::SaveGameToSlot(Profile, Fresh, 0));
    TestTrue(TEXT("insufficient balance fails purchase"), !RiverfrontLife::Buy(Fresh, TEXT("shirt-indigo"), false).IsEmpty());
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

    // A player may have left the previous wardrobe holding a shirt. Only the jeans slot is new.
    Migrated->Schema=2;
    Migrated->EquippedBottom=NAME_None;
    Migrated->OwnedItems.Remove(RiverfrontLife::StarterBottom);
    Migrated->EquippedOutfit=RiverfrontLife::BaseLayer;
    Migrated->HeldWardrobeItem=TEXT("cotton-sand");
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
    TestEqual(TEXT("V2 worn top retained"),Current->EquippedOutfit,RiverfrontLife::BaseLayer);
    TestEqual(TEXT("V2 held garment retained"),Current->HeldWardrobeItem,FName(TEXT("cotton-sand")));
    TestEqual(TEXT("V2 denim restored"),Current->EquippedBottom,RiverfrontLife::StarterBottom);
    TestTrue(TEXT("V2 can remove original jeans"),Current->OwnedItems.Contains(RiverfrontLife::StarterBottom));

    Current->CashRupees = 350;
    if (!TestTrue(TEXT("write new canonical progress"), Finder.Write(*Current))) return false;
    auto* Continued = Finder.Load();
    if (!TestNotNull(TEXT("canonical progress readable"), Continued)) return false;
    TestEqual(TEXT("canonical wins over stale legacy"), Continued->CashRupees, int64(350));

    Current->Schema=3;
    Current->EquippedBottom=RiverfrontLife::Underwear;
    Current->OwnedItems.Remove(RiverfrontLife::Briefs);
    Current->OwnedItems.Remove(RiverfrontLife::SportsBra);
    Current->EquippedInnerTop=Current->EquippedInnerBottom=NAME_None;
    if (!TestTrue(TEXT("seed V3 innerwear migration"),Finder.Write(*Current))) return false;
    Current=Finder.Load();
    if (!TestNotNull(TEXT("V3 wardrobe reopens"),Current)) return false;
    TestEqual(TEXT("V3 empty outer bottom preserved"),Current->EquippedBottom,RiverfrontLife::Underwear);
    TestEqual(TEXT("previously visible briefs retained"),Current->EquippedInnerBottom,RiverfrontLife::Briefs);
    TestEqual(TEXT("previously visible sports bra retained"),Current->EquippedInnerTop,RiverfrontLife::SportsBra);
    TestTrue(TEXT("previously fixed layers now owned"),Current->OwnedItems.Contains(RiverfrontLife::Briefs) && Current->OwnedItems.Contains(RiverfrontLife::SportsBra));
    Current->Schema=4;
    Current->EquippedInnerBottom=Current->EquippedInnerTop=NAME_None;
    Current->EquippedShoes=Current->EquippedSocks=NAME_None;
    Current->OwnedItems.Remove(RiverfrontLife::CottonSocks);
    Current->OwnedItems.Remove(RiverfrontLife::StreetShoes);
    if (!Finder.Write(*Current) || !(Current=Finder.Load())) return false;
    TestTrue(TEXT("V4 keeps both inner layers off"),Current->EquippedInnerBottom.IsNone() && Current->EquippedInnerTop.IsNone());
    TestEqual(TEXT("V4 original shoes retained"),Current->EquippedShoes,RiverfrontLife::StreetShoes);
    TestEqual(TEXT("V4 original socks retained"),Current->EquippedSocks,RiverfrontLife::CottonSocks);
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
