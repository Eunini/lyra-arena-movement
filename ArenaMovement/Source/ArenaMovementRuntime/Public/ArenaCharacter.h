#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacter.h"
#include "ArenaCharacterMovementComponent.h"
#include "ArenaCharacter.generated.h"

/**
 * Lyra character using UArenaCharacterMovementComponent. Point a LyraPawnData asset at this
 * class (or a Blueprint child) to use arena movement in an experience.
 *
 * Dodges are simulated by the movement component on the owner and the server. Everyone else
 * learns about them through a replicated counter, so animation, audio and cues fire on all
 * machines without an extra multicast RPC.
 */
UCLASS()
class ARENAMOVEMENTRUNTIME_API AArenaCharacter : public ALyraCharacter
{
	GENERATED_BODY()

public:
	AArenaCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Arena|Movement")
	UArenaCharacterMovementComponent* GetArenaMovement() const;

protected:
	virtual void BeginPlay() override;

	/** Fires once per dodge on every machine: hook animation montages, sounds and camera kicks here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Arena|Movement", meta = (DisplayName = "On Dodged"))
	void K2_OnDodged(EArenaDodgeKind Kind);

private:
	void HandleLocalDodge(EArenaDodgeKind Kind);
	void SetDodgingTag(bool bDodging);

	UFUNCTION()
	void OnRep_DodgeCount();

	/** Incremented by the server per dodge; the owner already played its own effects, so it is skipped. */
	UPROPERTY(ReplicatedUsing = OnRep_DodgeCount)
	uint8 DodgeCount = 0;

	UPROPERTY(Replicated)
	EArenaDodgeKind LastDodgeKind = EArenaDodgeKind::None;
};
