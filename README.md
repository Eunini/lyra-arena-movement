# ArenaMovement: UT/Quake-style movement for Lyra (UE 5.4, C++)

A Lyra Game Feature plugin that adds classic arena-shooter movement to Lyra's character, with full client-side prediction:

- **Dodge:** a burst in the input direction, triggered by a GAS ability on `InputTag.Ability.Dodge`. It never slows a player down: speed already carried in the dodge direction is kept.
- **Wall dodge:** in the air next to a wall, dodge away from it. One per jump, re-armed on landing.
- **Double jump:** Lyra's existing jump ability plus `JumpMaxCount = 2`, with a separate air-jump velocity that works while falling.
- **Air strafing:** Quake's `PM_AirAccelerate` replaces Unreal's `AirControl` while falling. Turning while strafing gains speed; holding forward does not.
- **Landing recovery:** a short delay after landing before the next dodge, which stops chain-dodging.

## Build status

- Compiles and links cleanly (0 errors, 0 warnings) as a plugin inside Lyra 5.4, built with `Build.sh LyraEditor Linux Development` in Epic's `unreal-engine:dev-slim-5.4.4` container.
- The movement rules pass their standalone tests (below).
- It has not been play-tested in a Lyra level yet. Lyra's content isn't part of the source repository, and the assets in *Using it in Lyra* still need creating in the editor.

## Why it's built this way

The movement runs **inside the character movement component**, not as a velocity change applied from the ability. A GAS ability that launches the character directly gives a mismatch under latency: the server applies the launch at a different point in the move stream than the client did, so every dodge gets rubber-banded. Instead:

1. The dodge ability (`LocalPredicted`) only calls `RequestDodge()` on the owning client.
2. The request travels as `FLAG_Custom_0` on the next saved move (`FSavedMove_Arena`), so the server applies it on exactly the same move as the client.
3. The dodge direction comes from that move's input acceleration, which client and server already share after quantisation, so no extra payload is sent.
4. The dodge state (landing recovery, wall-dodge used, in-flight) is saved with every move and restored in `PrepMoveFor`, so replays after a server correction reproduce the same dodges.
5. Dodge moves are never combined with neighbouring moves.

Other players see dodges through normal movement replication. Effects use a `COND_SkipOwner` replicated counter on `AArenaCharacter`, so `On Dodged` fires once on every machine (animation, audio, camera kick) without a multicast RPC. Replays never re-trigger effects (`bClientUpdating` guard).

## Layout

```
ArenaMovement/                         Drop into <Lyra>/Plugins/GameFeatures/
  Source/ArenaMovementRuntime/
    Public/Core/ArenaMovementMath.h    Engine-agnostic dodge rules + Quake air acceleration
    Public/ArenaCharacterMovementComponent.h   ULyraCharacterMovementComponent subclass, saved moves
    Public/ArenaCharacter.h            ALyraCharacter subclass, replicated dodge events, Status.Dodging tag
    Public/Abilities/ArenaGameplayAbility_Dodge.h   ULyraGameplayAbility that requests the dodge
    Public/ArenaMovementTags.h         InputTag.Ability.Dodge, Status.Dodging, Ability.Type.Movement.Dodge
Tests/                                 Standalone tests for the movement rules (no engine needed)
```

## Using it in Lyra

1. Copy `ArenaMovement/` into `Plugins/GameFeatures/` and regenerate project files.
2. Create a `LyraPawnData` asset whose Pawn Class is `ArenaCharacter` (or a Blueprint child for the `On Dodged` effects). Use it in your experience in place of the default hero pawn data.
3. Add `ArenaGameplayAbility_Dodge` to a `LyraAbilitySet` with input tag `InputTag.Ability.Dodge`, and add the ability set to the pawn data.
4. In the `LyraInputConfig`, map an input action (for example Left Shift) to `InputTag.Ability.Dodge`, and add the action to the input mapping context.
5. Tune dodge speeds, landing recovery, double-jump velocity and air acceleration on the character's movement component (`Arena|*` categories).

## Tests

```
cmake -S Tests -B build && cmake --build build && ./build/arena_tests
```

These cover:

- ground dodge speed and direction
- momentum kept when you are already faster than dodge speed
- landing recovery
- wall-dodge rules: away from the wall only, floors don't count, one per jump
- the air-speed cap when holding forward, and no air friction
- deterministic results
- a strafe-jumping simulation: with the default tuning, optimal strafing takes you from 600 to about 986 cm/s in one second

## Tuning notes

- The defaults feel like Quake 1 / QuakeWorld (`sv_airaccelerate 10` with a 30-unit wish-speed cap). For something closer to Quake 3, lower `AirAccelerate` to about 1 and raise `AirWishSpeedCap` to the walk speed.
- Dodge speeds are in Unreal units (cm/s) and assume Lyra's default 600 cm/s walk speed.

## Known limitations

- The server's position corrections don't carry the dodge state. If the client and server ever disagree on the landing-recovery timer, the client may predict a dodge the server rejects. It snaps back on the next correction, and the state re-syncs on landing. Adding the recovery timer to a custom `FCharacterNetworkMoveData` would close this gap.
- Air strafing assumes Lyra's default world-Z gravity (the 5.4 custom gravity direction is not handled).
- No ground-friction skip on landing, so bunny hopping keeps speed only within the limits of Unreal's braking.
