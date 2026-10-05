// Standalone checks for the arena movement rules.
// Build: cmake -S Checks -B build && cmake --build build && ./build/arena_checks
#include "../ArenaMovement/Source/ArenaMovementRuntime/Public/Core/ArenaMovementMath.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace ArenaMovement;

namespace
{
	int Failures = 0;

	void Check(bool bCondition, const char* What)
	{
		if (!bCondition)
		{
			std::printf("    FAIL: %s\n", What);
			++Failures;
		}
	}

	Vec3 Rotate2D(const Vec3& V, double Radians)
	{
		const double C = std::cos(Radians), S = std::sin(Radians);
		return {V.X * C - V.Y * S, V.X * S + V.Y * C, V.Z};
	}

	void GroundDodgeFromStandstill()
	{
		DodgeParams P;
		DodgeState S;
		const Vec3 Right{0, 1, 0};
		const EDodgeKind Kind = EvaluateDodge(S, P, true, false, Right, false, {});
		Check(Kind == EDodgeKind::Ground, "ground dodge allowed when recovered");
		const Vec3 V = ApplyDodge(Kind, S, P, {}, Right);
		Check(std::fabs(V.Y - P.GroundSpeed) < 1e-9 && std::fabs(V.X) < 1e-9, "launches at dodge speed in the input direction");
		Check(std::fabs(V.Z - P.GroundLift) < 1e-9, "applies dodge lift");
	}

	void DodgeNeverSlowsYouDown()
	{
		DodgeParams P;
		DodgeState S;
		const Vec3 Fast{2000, 0, 0};
		const Vec3 V = ApplyDodge(EDodgeKind::Ground, S, P, Fast, {1, 0, 0});
		Check(std::fabs(V.X - 2000) < 1e-9, "speed above dodge speed is kept");
		const Vec3 Side = ApplyDodge(EDodgeKind::Ground, S, P, Fast, {0, 1, 0});
		Check(std::fabs(Side.Y - P.GroundSpeed) < 1e-9 && std::fabs(Side.X) < 1e-9, "sideways dodge redirects momentum");
	}

	void RecoveryBlocksChainDodging()
	{
		DodgeParams P;
		DodgeState S;
		ApplyDodge(EDodgeKind::Ground, S, P, {}, {1, 0, 0});
		OnLanded(S, P);
		Check(EvaluateDodge(S, P, true, false, {1, 0, 0}, false, {}) == EDodgeKind::None, "no dodge straight after landing");
		for (int i = 0; i < 60 && S.Recovery > 0; ++i) TickDodgeState(S, 1.0 / 120.0);
		Check(EvaluateDodge(S, P, true, false, {1, 0, 0}, false, {}) == EDodgeKind::Ground, "dodge available after landing recovery");
	}

	void NoDodgeWithoutDirection()
	{
		DodgeParams P;
		DodgeState S;
		Check(EvaluateDodge(S, P, true, false, {}, false, {}) == EDodgeKind::None, "a dodge needs a direction");
		Check(EvaluateDodge(S, P, false, true, {1, 0, 0}, false, {}) == EDodgeKind::None, "no air dodge without a wall");
	}

	void WallDodgeRules()
	{
		DodgeParams P;
		DodgeState S;
		const Vec3 WallNormal{-1, 0, 0}; // wall ahead on +X, normal faces the player
		Check(EvaluateDodge(S, P, false, true, {-1, 0, 0}, true, WallNormal) == EDodgeKind::Wall, "dodging away from a wall is a wall dodge");
		Check(EvaluateDodge(S, P, false, true, {1, 0, 0}, true, WallNormal) == EDodgeKind::None, "cannot wall dodge into the wall");
		Check(EvaluateDodge(S, P, false, true, {-1, 0, 0}, true, {0, 0, 1}) == EDodgeKind::None, "floors are not walls");

		const Vec3 V = ApplyDodge(EDodgeKind::Wall, S, P, {300, 0, -400}, {-1, 0, 0});
		Check(std::fabs(V.X + P.WallSpeed) < 1e-9 && std::fabs(V.Z - P.WallLift) < 1e-9, "wall dodge launches away and up");
		Check(EvaluateDodge(S, P, false, true, {-1, 0, 0}, true, WallNormal) == EDodgeKind::None, "one wall dodge per jump");
		OnLanded(S, P);
		Check(!S.bWallDodgeUsed, "landing re-arms the wall dodge");
	}

	void AirAccelerateCapsForwardSpeed()
	{
		AirStrafeParams P;
		Vec3 V{600, 0, 0};
		for (int i = 0; i < 120; ++i) V = AirAccelerate(V, {1, 0, 0}, P, 1.0 / 120.0);
		Check(std::fabs(V.X - 600) < 1e-9, "holding forward in the air cannot add speed past the cap");

		Vec3 Still{};
		for (int i = 0; i < 120; ++i) Still = AirAccelerate(Still, {1, 0, 0}, P, 1.0 / 120.0);
		Check(Still.X > 70 && Still.X <= P.AirWishSpeedCap + 1e-9, "from rest, air control reaches the small wish-speed cap");

		const Vec3 NoInput = AirAccelerate({500, 200, 0}, {}, P, 1.0 / 120.0);
		Check(NoInput.X == 500 && NoInput.Y == 200, "no input keeps momentum (no air friction)");
	}

	// Strafe jumping: wish direction held just under 90 degrees from the velocity while turning.
	double StrafeForOneSecond(const AirStrafeParams& P)
	{
		Vec3 V{600, 0, 0};
		const double Dt = 1.0 / 120.0;
		for (int i = 0; i < 120; ++i)
		{
			const double Speed = V.Size2D();
			const double Accel = P.AirAccelerate * P.MaxWishSpeed * Dt;
			const double Cap = P.AirWishSpeedCap;
			// Best angle keeps the velocity component along the wish direction at Cap - Accel.
			const double CosAngle = std::fmax(-1.0, std::fmin(1.0, (Cap - Accel) / Speed));
			const Vec3 Wish = Rotate2D(V.SafeNormal2D(), std::acos(CosAngle));
			V = AirAccelerate(V, Wish, P, Dt);
		}
		return V.Size2D();
	}

	void StrafeJumpingGainsSpeed()
	{
		AirStrafeParams P;
		const double After = StrafeForOneSecond(P);
		std::printf("    600 cm/s -> %.0f cm/s after 1 s of optimal air strafing\n", After);
		Check(After > 720.0, "turning air strafe gains at least 20% speed in a second");
	}

	void Deterministic()
	{
		AirStrafeParams P;
		Check(StrafeForOneSecond(P) == StrafeForOneSecond(P), "same inputs give the same result (safe to replay)");
	}
}

int main()
{
	const std::vector<std::pair<const char*, std::function<void()>>> Checks = {
		{"ground dodge from standstill", GroundDodgeFromStandstill},
		{"dodge never slows you down", DodgeNeverSlowsYouDown},
		{"recovery blocks chain dodging", RecoveryBlocksChainDodging},
		{"no dodge without direction", NoDodgeWithoutDirection},
		{"wall dodge rules", WallDodgeRules},
		{"air accelerate caps forward speed", AirAccelerateCapsForwardSpeed},
		{"strafe jumping gains speed", StrafeJumpingGainsSpeed},
		{"deterministic", Deterministic},
	};
	for (const auto& [Name, Fn] : Checks)
	{
		const int Before = Failures;
		std::printf("%s\n", Name);
		Fn();
		std::printf("  %s\n", Failures == Before ? "ok" : "FAILED");
	}
	std::printf("\n%s (%d failure%s)\n", Failures == 0 ? "ALL PASSED" : "FAILURES", Failures, Failures == 1 ? "" : "s");
	return Failures == 0 ? 0 : 1;
}
