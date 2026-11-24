using UnrealBuildTool;

public class SabarmatiPrototype : ModuleRules
{
    public SabarmatiPrototype(ReadOnlyTargetRules Target) : base(Target)
    {
        if (Target.Platform == UnrealTargetPlatform.Mac) PublicFrameworks.Add("Foundation");
        if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("AssetRegistry");
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "ProceduralMeshComponent", "RenderCore", "RHI", "Json", "MoviePlayer", "LevelSequence", "MovieScene", "MovieSceneTracks" });
    }
}
