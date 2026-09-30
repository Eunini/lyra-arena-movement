using UnrealBuildTool;

public class ArenaMovementRuntime : ModuleRules
{
	public ArenaMovementRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"LyraGame",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AIModule", // ALyraCharacter implements IGenericTeamAgentInterface
			"GameplayTasks",
			"NetCore",
		});
	}
}
