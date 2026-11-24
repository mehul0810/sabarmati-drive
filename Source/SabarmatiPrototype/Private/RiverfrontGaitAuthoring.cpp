#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

namespace
{
// Editor authoring command. Both wardrobe skeletons use the same speed axis in cm/s.
bool SaveLocomotion(const FString& Folder)
{
    const FString Name = TEXT("BS_ResidentGait");
    const FString Path = Folder / Name;
    TArray<UAnimSequence*> Clips;
    for (const TCHAR* ClipName : {TEXT("A_Idle"), TEXT("A_Walk"), TEXT("A_Jog")})
    {
        const FString ClipPath = Folder / ClipName;
        auto* Clip = LoadObject<UAnimSequence>(nullptr, *(ClipPath + TEXT(".") + ClipName));
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
    const float Speeds[] = {0.f,145.f,335.f};
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
    Gait->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    const FString File = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Package, Gait, *File, Args);
}
FAutoConsoleCommand AuthorResidentGaits(TEXT("Sabarmati.BuildResidentGaits"),
    TEXT("Save native speed blend spaces for the public resident skeletons."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        const FString People = TEXT("/Game/Sabarmati/Riverfront/People");
        if (SaveLocomotion(People) && SaveLocomotion(People / TEXT("Kurta")))
        {
            UE_LOG(LogTemp, Display, TEXT("Sabarmati resident gait blend spaces verified and saved."));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to build resident gait blend spaces."));
        }
    }));
}
#endif
