#include "Core/ArenaMovementMath.h"

namespace ArenaMovement
{
	EDodgeKind EvaluateDodge(const DodgeState& State, const DodgeParams& Params, bool bOnGround, bool bFalling,
		const Vec3& DodgeDir, bool bHasWall, const Vec3& WallNormal)
	{
		if (DodgeDir.Size2D() < 1e-6)
		{
			return EDodgeKind::None;
		}
		if (bOnGround)
		{
			return State.Recovery <= 0.0 ? EDodgeKind::Ground : EDodgeKind::None;
		}
		if (bFalling && bHasWall && !State.bWallDodgeUsed && std::fabs(WallNormal.Z) <= Params.MaxWallNormalZ)
		{
			// Push off the wall: the dodge has to point away from it.
			const double Away = DodgeDir.SafeNormal2D().Dot(WallNormal.SafeNormal2D());
			return Away >= Params.MinWallAwayDot ? EDodgeKind::Wall : EDodgeKind::None;
		}
		return EDodgeKind::None;
	}

	Vec3 ApplyDodge(EDodgeKind Kind, DodgeState& State, const DodgeParams& Params, const Vec3& Velocity, const Vec3& DodgeDir)
	{
		if (Kind == EDodgeKind::None)
		{
			return Velocity;
		}
		const Vec3 Dir = DodgeDir.SafeNormal2D();
		const bool bWall = Kind == EDodgeKind::Wall;
		const double DodgeSpeed = bWall ? Params.WallSpeed : Params.GroundSpeed;
		const double Carried = Velocity.Flat().Dot(Dir);
		const Vec3 Horizontal = Dir * (Carried > DodgeSpeed ? Carried : DodgeSpeed);
		const double Lift = bWall ? Params.WallLift : Params.GroundLift;

		if (bWall)
		{
			State.bWallDodgeUsed = true;
		}
		// Recovery starts on landing; this blocks a second ground dodge before takeoff resolves.
		State.Recovery = Params.LandingRecovery;
		return {Horizontal.X, Horizontal.Y, bWall ? (Velocity.Z > Lift ? Velocity.Z : Lift) : Lift};
	}

	void TickDodgeState(DodgeState& State, double Dt)
	{
		State.Recovery = State.Recovery > Dt ? State.Recovery - Dt : 0.0;
	}

	void OnLanded(DodgeState& State, const DodgeParams& Params)
	{
		State.bWallDodgeUsed = false;
		State.Recovery = Params.LandingRecovery;
	}

	Vec3 AirAccelerate(const Vec3& Velocity, const Vec3& WishDir, const AirStrafeParams& Params, double Dt)
	{
		const Vec3 Wish = WishDir.SafeNormal2D();
		if (Wish.Size2D() < 1e-6)
		{
			return Velocity; // no air friction, momentum is kept
		}
		const double WishSpeed = Params.MaxWishSpeed < Params.AirWishSpeedCap ? Params.MaxWishSpeed : Params.AirWishSpeedCap;
		const double CurrentSpeed = Velocity.Flat().Dot(Wish);
		const double AddSpeed = WishSpeed - CurrentSpeed;
		if (AddSpeed <= 0.0)
		{
			return Velocity;
		}
		double AccelSpeed = Params.AirAccelerate * Params.MaxWishSpeed * Dt;
		if (AccelSpeed > AddSpeed)
		{
			AccelSpeed = AddSpeed;
		}
		return Velocity + Wish * AccelSpeed;
	}
}
