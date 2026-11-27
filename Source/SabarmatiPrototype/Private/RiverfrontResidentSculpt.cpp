#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "HAL/IConsoleManager.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshAttributes.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"

namespace
{
const TCHAR* SculptVersion = TEXT("GrandCityResidentSculpt");

float Bell(float Offset, float Width)
{
    return FMath::Exp(-FMath::Square(Offset / Width));
}

FVector3f SculptPosition(const FVector3f& Point, bool bFemale)
{
    // Public outfit space is in centimetres, with +Y forward and +Z up.
    const float Side = FMath::Abs(Point.X);
    const float Height = Point.Z;
    FVector3f Result = Point;
    if (Height > 109.f && Height < 145.f)
    {
        const float Torso = FMath::SmoothStep(109.f, 117.f, Height)
            * (1.f - FMath::SmoothStep(136.f, 145.f, Height))
            * (1.f - FMath::SmoothStep(28.f, 36.f, Side));
        const float Shoulder = Bell(Height - 135.f, 10.f);
        const float Ribs = Bell(Height - 125.f, 11.f);
        Result.X += Point.X * Torso * (bFemale ? .055f * Shoulder - .045f * Ribs : .17f * Shoulder + .062f * Ribs);
        Result.Y += (Point.Y - 1.8f) * Torso * Ribs * (bFemale ? .055f : .11f);
    }
    if (Height > 143.f && Height < 164.f && Point.Y > 2.f)
    {
        // Keep the eyeball seats, ears, and the hairline at the original fit.
        const float Region = FMath::SmoothStep(143.f, 146.f, Height)
            * (1.f - FMath::SmoothStep(161.f, 164.f, Height))
            * FMath::SmoothStep(2.f, 8.f, Point.Y)
            * (1.f - FMath::SmoothStep(6.f, 8.2f, Side));
        const float Orbit = Bell(Side - 3.7f, 2.0f) * Bell(Height - 157.8f, 1.9f);
        const float Face = Region * (1.f - Orbit);
        const float Jaw = Bell(Height - 148.f, 3.3f);
        const float Cheek = Bell(Height - 154.f, 2.6f) * Bell(Side - 4.1f, 2.1f);
        const float Chin = Bell(Height - 147.7f, 1.9f) * Bell(Side, 2.8f);
        const float Tip = Bell(Height - 155.1f, 1.65f) * Bell(Side, 1.32f);
        Result.X += Face * Point.X * (bFemale ? -.12f * Jaw + .07f * Cheek : .22f * Jaw + .10f * Tip);
        Result.Y += Face * (bFemale ? .58f * Cheek + .29f * Tip - .29f * Chin
                                    : .71f * Chin + .59f * Tip - .25f * Cheek);
        Result.Z += Face * (bFemale ? .36f * Tip + .28f * Chin : -.34f * Chin);
    }
    return Result;
}

struct FShapeFrame
{
    FVector3f X;
    FVector3f Y;
    FVector3f Z;
};

float SculptDescription(FMeshDescription& Description, bool bFemale)
{
    auto Positions = Description.GetVertexPositions();
    FStaticMeshAttributes Attributes(Description);
    auto Normals = Attributes.GetVertexInstanceNormals();
    auto Tangents = Attributes.GetVertexInstanceTangents();
    TArray<FShapeFrame> Frames;
    Frames.SetNum(Description.Vertices().GetArraySize());
    float MaxChange = 0.f;
    for (const FVertexID Vertex : Description.Vertices().GetElementIDs())
    {
        const FVector3f Original = Positions[Vertex];
        const FVector3f New = SculptPosition(Original, bFemale);
        MaxChange = FMath::Max(MaxChange, (New - Original).Size());
        const auto Derivative = [&](const FVector3f& Axis)
        {
            constexpr float Step = .125f;
            return (SculptPosition(Original + Step * Axis, bFemale)
                - SculptPosition(Original - Step * Axis, bFemale)) / (2.f * Step);
        };
        Frames[Vertex.GetValue()] = {Derivative(FVector3f(1, 0, 0)),
            Derivative(FVector3f(0, 1, 0)), Derivative(FVector3f(0, 0, 1))};
        Positions[Vertex] = New;
    }

    // Carry the imported shading frame through the deformation, including UV splits.
    for (const FVertexInstanceID Instance : Description.VertexInstances().GetElementIDs())
    {
        const auto& Frame = Frames[Description.GetVertexInstanceVertex(Instance).GetValue()];
        const FVector3f N = Normals[Instance];
        const FVector3f T = Tangents[Instance];
        Normals[Instance] = (N.X * FVector3f::CrossProduct(Frame.Y, Frame.Z)
            + N.Y * FVector3f::CrossProduct(Frame.Z, Frame.X)
            + N.Z * FVector3f::CrossProduct(Frame.X, Frame.Y)).GetSafeNormal();
        Tangents[Instance] = (T.X * Frame.X + T.Y * Frame.Y + T.Z * Frame.Z).GetSafeNormal();
    }
    return MaxChange;
}

bool SaveSculpt(USkeletalMesh& Mesh)
{
    UPackage* Package = Mesh.GetPackage();
    if (Package->GetMetaData().HasValue(&Mesh, SculptVersion))
    {
        UE_LOG(LogTemp, Display, TEXT("Grand City sculpt already authored: %s"), *Mesh.GetPathName());
        return true;
    }

    const bool bFemale = Mesh.GetName().StartsWith(TEXT("SK_Female"));
    float MaxChange = 0.f;
    for (int32 LOD = 0; LOD < Mesh.GetAllSourceModels().Num(); ++LOD)
    {
        if (FMeshDescription* Description = Mesh.GetMeshDescription(LOD))
        {
            MaxChange = FMath::Max(MaxChange, SculptDescription(*Description, bFemale));
            if (!Mesh.CommitMeshDescription(LOD)) return false;
        }
    }
    if (MaxChange < .5f) return false;

    Mesh.Build();
    FSkinnedAssetCompilingManager::Get().FinishAllCompilation();
    Package->GetMetaData().SetValue(&Mesh, SculptVersion, TEXT("1"));
    Mesh.MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    if (!UPackage::SavePackage(Package, &Mesh, *File, Args)) return false;
    UE_LOG(LogTemp, Display, TEXT("Grand City sculpt saved: %s (%.2f cm)"), *Mesh.GetPathName(), MaxChange);
    return true;
}

bool SculptWardrobe()
{
    const TCHAR* Outfits[] = {
        TEXT("Everyday/SK_RiverEveryday"), TEXT("Tailored/SK_FittedKurta"),
        TEXT("Female/SK_FemaleEveryday"), TEXT("Female/SK_FemaleKurta"),
        TEXT("Foundation/SK_MaleFoundation"), TEXT("Foundation/SK_FemaleFoundation"),
        TEXT("Jersey/SK_MaleCrewTee"), TEXT("Jersey/SK_FemaleCrewTee"),
        TEXT("Western/SK_MaleTankFoundation"), TEXT("Western/SK_FemaleTankFoundation"),
        TEXT("Western/SK_MaleFineKnit"), TEXT("Western/SK_FemaleFineKnit"),
        TEXT("Western/SK_MaleQuilted"), TEXT("Western/SK_FemaleQuilted"),
        TEXT("Western/SK_MaleBlazer"), TEXT("Western/SK_FemaleBlazer")
    };
    TArray<USkeletalMesh*> Meshes;
    for (const TCHAR* Outfit : Outfits)
    {
        const FString Package = FString(TEXT("/Game/Sabarmati/Riverfront/People/Wardrobe/")) + Outfit;
        const FString Path = Package + TEXT(".") + FPackageName::GetShortName(Package);
        auto* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path);
        if (!Mesh || !Mesh->HasMeshDescription(0) || !Mesh->GetMorphTargets().IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("Missing unmodified source mesh geometry at %s"), *Path);
            return false;
        }
        Meshes.Add(Mesh);
    }
    for (USkeletalMesh* Mesh : Meshes)
        if (!SaveSculpt(*Mesh)) return false;
    UE_LOG(LogTemp, Display, TEXT("Grand City sculpt verified on %d outfits."), Meshes.Num());
    return true;
}

FAutoConsoleCommand AuthorResidentSculpts(TEXT("Sabarmati.BuildResidentSculpts"),
    TEXT("Bake the two resident sculpts into their fitted wardrobe meshes."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (!SculptWardrobe()) UE_LOG(LogTemp, Error, TEXT("Grand City sculpt batch failed."));
    }));
}
#endif
