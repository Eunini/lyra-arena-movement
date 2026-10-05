// Lyra character movement with UT/Quake arena movement:
//   * dodge / wall dodge  - requested by a GAS ability, carried to the server as a saved-move flag
//   * double jump         - Lyra's jump ability + JumpMaxCount, with its own air-jump velocity
//   * air strafing        - Quake-style air acceleration replacing UE's AirControl while falling
// Custom simulation state is saved with each local move and restored during replay.
// Server corrections omit that custom state; the README describes this limitation.
#pragma once

#include "CoreMinimal.h"
#include "Character/LyraCharacterMovementComponent.h"
#include "Core/ArenaMovementMath.h"
#include "ArenaCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EArenaDodgeKind : uint8
{
	None,
	Ground,
	Wall
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnArenaDodge, EArenaDodgeKind /*Kind*/);

UCLASS(Config = Game)
class ARENAMOVEMENTRUNTIME_API UArenaCharacterMovementComponent : public ULyraCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_Arena;

public:
	UArenaCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** Owning client (or listen-server host): dodge on the next move, in the current input direction. */
	void RequestDodge() { bWantsToDodge = true; }

	/** Local estimate used to gate ability activation. The movement simulation makes the final call. */
	bool CanDodgeNow() const;

	UFUNCTION(BlueprintPure, Category = "Arena|Movement")
	bool IsDodging() const { return bDodgeInFlight; }

	/** Fired on the server and the owning client when a dodge is performed (never during move replays). */
	FOnArenaDodge OnDodge;

	//~ UCharacterMovementComponent interface
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual bool DoJump(bool bReplayingMoves) override;
	virtual FVector GetFallingLateralAcceleration(float DeltaTime) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	//~ End of UCharacterMovementComponent interface

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Dodge")
	float GroundDodgeSpeed = 1300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Dodge")
	float GroundDodgeLift = 420.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Dodge")
	float WallDodgeSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Dodge")
	float WallDodgeLift = 520.f;

	/** Seconds after landing before another dodge is allowed. */
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Dodge")
	float DodgeLandingRecovery = 0.35f;

	/** How far beyond the capsule a wall can be for a wall dodge. */
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Dodge")
	float WallDodgeReach = 40.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Jump")
	float DoubleJumpZVelocity = 700.f;

	/** Replace UE air control with Quake air acceleration (strafe jumping). */
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Air")
	bool bQuakeAirStrafe = true;

	UPROPERTY(EditDefaultsOnly, Category = "Arena|Air", meta = (EditCondition = "bQuakeAirStrafe"))
	float AirAccelerate = 10.f;

	/** Cap on wanted speed along the input direction while airborne (Quake: 30 units, ~76 cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "Arena|Air", meta = (EditCondition = "bQuakeAirStrafe"))
	float AirWishSpeedCap = 76.f;

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	ArenaMovement::DodgeParams MakeDodgeParams() const;
	ArenaMovement::AirStrafeParams MakeAirParams() const;
	FVector GetDodgeDirection() const;
	bool FindDodgeWall(const FVector& DodgeDirection, FVector& OutWallNormal) const;
	void TryPerformDodge();

	bool bWantsToDodge = false;
	bool bDodgeInFlight = false;
	ArenaMovement::DodgeState DodgeState;
};
