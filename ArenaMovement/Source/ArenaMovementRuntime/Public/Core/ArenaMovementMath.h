// Engine-agnostic arena movement rules: UT-style dodging and Quake-style air acceleration.
// Plain C++ so the rules are verified outside Unreal and behave identically when the
// character movement component replays saved moves. Units match Unreal (cm, cm/s, s; Z up).
#pragma once

#include <cmath>

namespace ArenaMovement
{
	struct Vec3
	{
		double X = 0.0, Y = 0.0, Z = 0.0;

		constexpr Vec3() = default;
		constexpr Vec3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

		Vec3 operator+(const Vec3& O) const { return {X + O.X, Y + O.Y, Z + O.Z}; }
		Vec3 operator-(const Vec3& O) const { return {X - O.X, Y - O.Y, Z - O.Z}; }
		Vec3 operator*(double S) const { return {X * S, Y * S, Z * S}; }
		double Dot(const Vec3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		double Size2D() const { return std::sqrt(X * X + Y * Y); }
		Vec3 Flat() const { return {X, Y, 0.0}; }
		Vec3 SafeNormal2D() const
		{
			const double L = Size2D();
			return L > 1e-6 ? Vec3{X / L, Y / L, 0.0} : Vec3{};
		}
	};

	enum class EDodgeKind : unsigned char
	{
		None,
		Ground,
		Wall
	};

	struct DodgeParams
	{
		double GroundSpeed = 1300.0;       // horizontal speed a ground dodge launches at
		double GroundLift = 420.0;         // vertical speed of a ground dodge
		double WallSpeed = 1200.0;
		double WallLift = 520.0;
		double LandingRecovery = 0.35;     // seconds after landing before the next dodge
		double MinWallAwayDot = 0.1;       // dodge must point at least this much away from the wall
		double MaxWallNormalZ = 0.5;       // steeper surfaces count as floors, not walls
	};

	/** Per-character dodge state. Saved and restored with each move so replays are exact. */
	struct DodgeState
	{
		double Recovery = 0.0;
		bool bWallDodgeUsed = false;
	};

	/** Decides which dodge (if any) is legal right now. */
	EDodgeKind EvaluateDodge(const DodgeState& State, const DodgeParams& Params, bool bOnGround, bool bFalling,
		const Vec3& DodgeDir, bool bHasWall, const Vec3& WallNormal);

	/**
	 * Returns the post-dodge velocity and updates State. Dodging never slows a player down:
	 * speed already carried in the dodge direction is kept if it is higher than the dodge speed.
	 */
	Vec3 ApplyDodge(EDodgeKind Kind, DodgeState& State, const DodgeParams& Params, const Vec3& Velocity, const Vec3& DodgeDir);

	void TickDodgeState(DodgeState& State, double Dt);
	void OnLanded(DodgeState& State, const DodgeParams& Params);

	struct AirStrafeParams
	{
		double AirAccelerate = 10.0;       // Quake sv_airaccelerate
		double MaxWishSpeed = 600.0;       // ground max speed, used for the acceleration rate
		double AirWishSpeedCap = 76.0;     // Quake's 30 units/s cap on wanted air speed (~76 cm/s)
	};

	/**
	 * Quake PM_AirAccelerate on the horizontal velocity. Only the component of velocity along the
	 * wish direction is capped, which is what makes strafe jumping gain speed when turning.
	 */
	Vec3 AirAccelerate(const Vec3& Velocity, const Vec3& WishDir, const AirStrafeParams& Params, double Dt);
}
