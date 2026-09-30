#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "ArenaGameplayAbility_Dodge.generated.h"

class UArenaCharacterMovementComponent;

/**
 * Dodge / wall dodge. Grant it through a LyraAbilitySet with InputTag.Ability.Dodge.
 *
 * The ability only makes the request; the movement itself happens inside the character
 * movement component so it is predicted, sent to the server and replayed like any other
 * move. That keeps dodges correct under latency, which a velocity change applied directly
 * from a GAS ability would not be. Costs, cooldown effects and gameplay cues can still be
 * added to this ability as usual.
 */
UCLASS()
class ARENAMOVEMENTRUNTIME_API UArenaGameplayAbility_Dodge : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaGameplayAbility_Dodge(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
		FGameplayTagContainer* OptionalRelevantTags) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	static UArenaCharacterMovementComponent* GetArenaMovement(const FGameplayAbilityActorInfo* ActorInfo);
};
