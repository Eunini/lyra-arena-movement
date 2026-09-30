#include "Abilities/ArenaGameplayAbility_Dodge.h"

#include "AbilitySystemComponent.h"
#include "ArenaCharacterMovementComponent.h"
#include "ArenaMovementTags.h"

UArenaGameplayAbility_Dodge::UArenaGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	AbilityTags.AddTag(ArenaMovementTags::Ability_Type_Movement_Dodge);
}

UArenaCharacterMovementComponent* UArenaGameplayAbility_Dodge::GetArenaMovement(const FGameplayAbilityActorInfo* ActorInfo)
{
	return ActorInfo ? Cast<UArenaCharacterMovementComponent>(ActorInfo->MovementComponent.Get()) : nullptr;
}

bool UArenaGameplayAbility_Dodge::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UArenaCharacterMovementComponent* Movement = GetArenaMovement(ActorInfo);
	if (!Movement)
	{
		return false;
	}
	// Only the predicting client checks dodge availability. The server's view of the movement
	// state can lag a few moves behind, and the movement component validates the dodge anyway.
	if (ActorInfo->IsLocallyControlled() && !Movement->CanDodgeNow())
	{
		return false;
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UArenaGameplayAbility_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ActorInfo->IsLocallyControlled())
	{
		if (UArenaCharacterMovementComponent* Movement = GetArenaMovement(ActorInfo))
		{
			Movement->RequestDodge();
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
