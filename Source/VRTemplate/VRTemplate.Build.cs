// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class VRTemplate : ModuleRules
{
	public VRTemplate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" , "InputDevice" , "HeadMountedDisplay", "NavigationSystem", "AIModule",
            "UMG", "Slate", "SlateCore", "RenderCore", "ApplicationCore", "Paper2D", "LevelSequence", "ActorSequence" , "MovieScene", "PhysX" , "APEX",  "GameplayTasks"});

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "RenderCore", "HeadMountedDisplay", "SteamVR" });
	}
}
