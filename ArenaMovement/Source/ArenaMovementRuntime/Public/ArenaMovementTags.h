#pragma once

#include "NativeGameplayTags.h"

namespace ArenaMovementTags
{
	/** Bind this input tag to the dodge ability in a LyraInputConfig / LyraAbilitySet. */
	ARENAMOVEMENTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Dodge);

	/** Owned while a dodge is in flight (until landing). Useful for blocking abilities mid-dodge. */
	ARENAMOVEMENTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Status_Dodging);

	/** Ability tag for the dodge ability itself. */
	ARENAMOVEMENTRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Movement_Dodge);
}
