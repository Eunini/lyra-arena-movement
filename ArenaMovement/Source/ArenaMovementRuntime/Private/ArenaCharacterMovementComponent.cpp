#include "ArenaCharacterMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	ArenaMovement::Vec3 ToCore(const FVector& V) { return {V.X, V.Y, V.Z}; }
	FVector FromCore(const ArenaMovement::Vec3& V) { return FVector(V.X, V.Y, V.Z); }

	EArenaDodgeKind ToUnreal(ArenaMovement::EDodgeKind Kind)
	{
		switch (Kind)
		{
		case ArenaMovement::EDodgeKind::Ground: return EArenaDodgeKind::Ground;
		case ArenaMovement::EDodgeKind::Wall:   return EArenaDodgeKind::Wall;
		default:                                return EArenaDodgeKind::None;
		}
	}
}

/** Saved move carrying the dodge request and the dodge state at the start of the move. */
class FSavedMove_Arena : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsToDodge = false;
		bSavedDodgeInFlight = false;
		SavedDodgeState = ArenaMovement::DodgeState();
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Flags = Super::GetCompressedFlags();
		if (bSavedWantsToDodge)
		{
			Flags |= FLAG_Custom_0;
		}
		return Flags;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		// Never merge a dodge into a neighbouring move: the server must see it on the exact move.
		const FSavedMove_Arena* Other = static_cast<const FSavedMove_Arena*>(NewMove.Get());
		if (bSavedWantsToDodge || Other->bSavedWantsToDodge)
		{
			return false;
		}
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
		if (const UArenaCharacterMovementComponent* Movement = Cast<UArenaCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			bSavedWantsToDodge = Movement->bWantsToDodge;
			bSavedDodgeInFlight = Movement->bDodgeInFlight;
			SavedDodgeState = Movement->DodgeState;
		}
	}

	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);
		if (UArenaCharacterMovementComponent* Movement = Cast<UArenaCharacterMovementComponent>(C->GetCharacterMovement()))
		{
			Movement->bWantsToDodge = bSavedWantsToDodge;
			Movement->bDodgeInFlight = bSavedDodgeInFlight;
			Movement->DodgeState = SavedDodgeState;
		}
	}

private:
	bool bSavedWantsToDodge = false;
	bool bSavedDodgeInFlight = false;
	ArenaMovement::DodgeState SavedDodgeState;
};

class FNetworkPredictionData_Client_Arena : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_Arena(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_Arena());
	}
};

UArenaCharacterMovementComponent::UArenaCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FNetworkPredictionData_Client* UArenaCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UArenaCharacterMovementComponent* MutableThis = const_cast<UArenaCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Arena(*this);
	}
	return ClientPredictionData;
}

void UArenaCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToDodge = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

ArenaMovement::DodgeParams UArenaCharacterMovementComponent::MakeDodgeParams() const
{
	ArenaMovement::DodgeParams Params;
	Params.GroundSpeed = GroundDodgeSpeed;
	Params.GroundLift = GroundDodgeLift;
	Params.WallSpeed = WallDodgeSpeed;
	Params.WallLift = WallDodgeLift;
	Params.LandingRecovery = DodgeLandingRecovery;
	return Params;
}

ArenaMovement::AirStrafeParams UArenaCharacterMovementComponent::MakeAirParams() const
{
	ArenaMovement::AirStrafeParams Params;
	Params.AirAccelerate = AirAccelerate;
	Params.MaxWishSpeed = MaxWalkSpeed;
	Params.AirWishSpeedCap = AirWishSpeedCap;
	return Params;
}

bool UArenaCharacterMovementComponent::CanDodgeNow() const
{
	if (bWantsToDodge)
	{
		return false;
	}
	if (IsMovingOnGround())
	{
		return DodgeState.Recovery <= 0.0;
	}
	return IsFalling() && !DodgeState.bWallDodgeUsed;
}

FVector UArenaCharacterMovementComponent::GetDodgeDirection() const
{
	// Acceleration is the input of the move being simulated. Clients and the server use the same
	// quantised value, so they agree on the direction without sending anything extra.
	const FVector Input = Acceleration.GetSafeNormal2D();
	if (!Input.IsNearlyZero())
	{
		return Input;
	}
	return UpdatedComponent ? UpdatedComponent->GetForwardVector().GetSafeNormal2D() : FVector::ZeroVector;
}

bool UArenaCharacterMovementComponent::FindDodgeWall(const FVector& DodgeDirection, FVector& OutWallNormal) const
{
	if (!CharacterOwner || !UpdatedPrimitive)
	{
		return false;
	}
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start - DodgeDirection * (Radius + WallDodgeReach);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArenaWallDodge), false, CharacterOwner);
	FCollisionResponseParams ResponseParams;
	UpdatedPrimitive->InitSweepCollisionParams(QueryParams, ResponseParams);

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, UpdatedPrimitive->GetCollisionObjectType(), QueryParams, ResponseParams))
	{
		OutWallNormal = Hit.ImpactNormal;
		return true;
	}
	return false;
}

void UArenaCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	ArenaMovement::TickDodgeState(DodgeState, DeltaSeconds);
	if (bWantsToDodge)
	{
		bWantsToDodge = false;
		TryPerformDodge();
	}
}

void UArenaCharacterMovementComponent::TryPerformDodge()
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	const FVector Direction = GetDodgeDirection();
	FVector WallNormal = FVector::ZeroVector;
	const bool bHasWall = IsFalling() && FindDodgeWall(Direction, WallNormal);

	const ArenaMovement::DodgeParams Params = MakeDodgeParams();
	const ArenaMovement::EDodgeKind Kind = ArenaMovement::EvaluateDodge(
		DodgeState, Params, IsMovingOnGround(), IsFalling(), ToCore(Direction), bHasWall, ToCore(WallNormal));
	if (Kind == ArenaMovement::EDodgeKind::None)
	{
		return;
	}

	Velocity = FromCore(ArenaMovement::ApplyDodge(Kind, DodgeState, Params, ToCore(Velocity), ToCore(Direction)));
	SetMovementMode(MOVE_Falling);
	bDodgeInFlight = true;

	// Replays re-run this code; only the first simulation of a move should trigger effects.
	if (!CharacterOwner->bClientUpdating)
	{
		OnDodge.Broadcast(ToUnreal(Kind));
	}
}

void UArenaCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Falling && IsMovingOnGround())
	{
		ArenaMovement::OnLanded(DodgeState, MakeDodgeParams());
		bDodgeInFlight = false;
	}
}

bool UArenaCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	const bool bAirJump = IsFalling();
	if (!Super::DoJump(bReplayingMoves))
	{
		return false;
	}
	if (bAirJump)
	{
		// Second jump replaces vertical speed, so it works while falling as well as rising.
		Velocity.Z = DoubleJumpZVelocity;
	}
	return true;
}

FVector UArenaCharacterMovementComponent::GetFallingLateralAcceleration(float DeltaTime)
{
	if (!bQuakeAirStrafe || HasAnimRootMotion())
	{
		return Super::GetFallingLateralAcceleration(DeltaTime);
	}
	// Full input acceleration; CalcVelocity applies the Quake rules instead of AirControl scaling.
	return FVector(Acceleration.X, Acceleration.Y, 0.f).GetClampedToMaxSize(GetMaxAcceleration());
}

void UArenaCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (bQuakeAirStrafe && IsFalling() && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		// PhysFalling zeroes Velocity.Z around this call, so this only touches horizontal speed.
		Velocity = FromCore(ArenaMovement::AirAccelerate(ToCore(Velocity), ToCore(Acceleration), MakeAirParams(), DeltaTime));
		return;
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}
