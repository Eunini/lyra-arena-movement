#include "ArenaCharacter.h"

#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "ArenaMovementTags.h"
#include "Net/UnrealNetwork.h"

AArenaCharacter::AArenaCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UArenaCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Ground jump + one air jump. Lyra's jump ability already checks CanJump(), which honours this.
	JumpMaxCount = 2;
}

void AArenaCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AArenaCharacter, DodgeCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AArenaCharacter, LastDodgeKind, COND_SkipOwner);
}

UArenaCharacterMovementComponent* AArenaCharacter::GetArenaMovement() const
{
	return Cast<UArenaCharacterMovementComponent>(GetCharacterMovement());
}

void AArenaCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UArenaCharacterMovementComponent* Movement = GetArenaMovement())
	{
		Movement->OnDodge.AddUObject(this, &AArenaCharacter::HandleLocalDodge);
	}
}

void AArenaCharacter::HandleLocalDodge(EArenaDodgeKind Kind)
{
	if (HasAuthority())
	{
		++DodgeCount;
		LastDodgeKind = Kind;
	}
	SetDodgingTag(true);
	K2_OnDodged(Kind);
}

void AArenaCharacter::OnRep_DodgeCount()
{
	SetDodgingTag(true);
	K2_OnDodged(LastDodgeKind);
}

void AArenaCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	SetDodgingTag(false);
}

void AArenaCharacter::SetDodgingTag(bool bDodging)
{
	// Loose tags are local to each machine; every machine sets it from the same events.
	if (ULyraAbilitySystemComponent* ASC = GetLyraAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(ArenaMovementTags::Status_Dodging, bDodging ? 1 : 0);
	}
}
