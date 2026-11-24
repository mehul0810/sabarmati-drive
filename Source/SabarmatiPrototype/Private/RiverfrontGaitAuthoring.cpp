#if WITH_EDITOR
#include "RiverfrontAvatarMotion.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
template <typename T>
T* LoadMotionAsset(const FString& Folder, const TCHAR* Name)
{
    return LoadObject<T>(nullptr, *(Folder / Name + TEXT(".") + Name));
}

bool SaveMotionAsset(UObject& Asset)
{
    Asset.MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    const FString File = FPackageName::LongPackageNameToFilename(Asset.GetPackage()->GetName(), FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Asset.GetPackage(), &Asset, *File, Args);
}

// The player uses the same speed axis in cm/s for a new rig.
bool SaveLocomotion(const FString& Folder, bool bAlternate = false)
{
    const FString Name = bAlternate ? TEXT("BS_ResidentAltGait") : TEXT("BS_ResidentGait");
    const FString Path = Folder / Name;
    TArray<UAnimSequence*> Clips;
    for (const TCHAR* ClipName : {TEXT("A_Idle"), bAlternate ? TEXT("A_WalkAlt") : TEXT("A_Walk"), TEXT("A_Jog")})
    {
        auto* Clip = LoadMotionAsset<UAnimSequence>(Folder, ClipName);
        if (!Clip || (!Clips.IsEmpty() && Clips[0]->GetSkeleton() != Clip->GetSkeleton())) return false;
        Clips.Add(Clip);
    }
    UPackage* Package = CreatePackage(*Path);
    Package->FullyLoad();
    auto* Gait = FindObject<UBlendSpace1D>(Package, *Name);
    if (!Gait)
    {
        Gait = NewObject<UBlendSpace1D>(Package, *Name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Gait);
    }
    Gait->SetSkeleton(Clips[0]->GetSkeleton());
    while (!Gait->GetBlendSamples().IsEmpty()) Gait->DeleteSample(Gait->GetBlendSamples().Num()-1);
    const float Speeds[] = {0.f,bAlternate ? 123.f : 145.f,335.f};
    Gait->ExpandRangeForSample(FVector(Speeds[2],0,0));
    for (int32 Index = 0; Index < Clips.Num(); ++Index)
        if (Gait->AddSample(Clips[Index], FVector(Speeds[Index],0,0)) == INDEX_NONE) return false;
    Gait->ResampleData();
    for (int32 Index = 0; Index < Clips.Num(); ++Index)
    {
        int32 Segment = INDEX_NONE;
        TArray<FBlendSampleData> Weights;
        if (!Gait->GetSamplesFromBlendInput(FVector(Speeds[Index],0,0), Weights, Segment, true) ||
            !Weights.ContainsByPredicate([&](const FBlendSampleData& Entry)
            { return Entry.Animation == Clips[Index] && Entry.GetClampedWeight() > .999f; })) return false;
    }
    return SaveMotionAsset(*Gait);
}

bool SaveAuthoredMotion(const FString& Folder)
{
    if (!SaveLocomotion(Folder)) return false;
    auto* Gait = LoadMotionAsset<UBlendSpace1D>(Folder, TEXT("BS_ResidentGait"));
    if (!Gait) return false;
    const TCHAR* ClipNames[] = {TEXT("A_Idle"), TEXT("MM_Jump"), TEXT("MM_Fall_Loop"),
        TEXT("MM_Land"), TEXT("A_FenceVault"), TEXT("A_WallMantle")};
    TArray<UAnimSequence*, TInlineAllocator<6>> Clips;
    for (const TCHAR* ClipName : ClipNames)
    {
        auto* Clip = LoadMotionAsset<UAnimSequence>(Folder, ClipName);
        if (!Clip || Clip->GetSkeleton() != Gait->GetSkeleton()) return false;
        Clips.Add(Clip);
    }

    const TCHAR* Name = TEXT("DA_AvatarMotion");
    UPackage* Package = CreatePackage(*(Folder / Name));
    Package->FullyLoad();
    auto* Motion = FindObject<URiverfrontAvatarMotion>(Package, Name);
    if (!Motion)
    {
        Motion = NewObject<URiverfrontAvatarMotion>(Package, Name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Motion);
    }
    Motion->Gait = Gait;
    Motion->FittingIdle = Clips[0];
    Motion->Takeoff = Clips[1];
    Motion->Fall = Clips[2];
    Motion->Landing = Clips[3];
    Motion->FenceVault = Clips[4];
    Motion->WallMantle = Clips[5];
    return SaveMotionAsset(*Motion);
}
FAutoConsoleCommand AuthorResidentGaits(TEXT("Sabarmati.BuildResidentGaits"),
    TEXT("Save native speed blend spaces for the public resident skeletons."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        const FString People = TEXT("/Game/Sabarmati/Riverfront/People");
        if (SaveLocomotion(People) && SaveLocomotion(People, true)
            && SaveLocomotion(People / TEXT("Kurta")) && SaveLocomotion(People / TEXT("Kurta"), true))
        {
            UE_LOG(LogTemp, Display, TEXT("Sabarmati resident gait blend spaces verified and saved."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to build resident gait blend spaces."));
        }
    }));

FAutoConsoleCommand AuthorAvatarMotion(TEXT("Sabarmati.BuildAuthoredAvatarMotion"),
    TEXT("Save the native avatar motion data asset from retargeted clips in the supplied /Game folder."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() == 1 && FPackageName::IsValidLongPackageName(Args[0]) && SaveAuthoredMotion(Args[0]))
        {
            UE_LOG(LogTemp, Display, TEXT("Sabarmati authored avatar motion saved at %s."), *Args[0]);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Unable to assemble avatar motion. Supply the /Game folder with the complete retargeted clips."));
        }
    }));
}
#endif
