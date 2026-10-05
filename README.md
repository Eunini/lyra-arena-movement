# Vector / Lyra Arena Movement

An Unreal Engine 5.4 C++ movement extension built on Lyra's character, character movement component and Gameplay Ability System.

The current revision adds a timed first-person course, a Blueprint runner, a pawn-data asset generator and a native recording path. This revision's engine build and native gameplay run are in progress. The earlier movement-only revision compiled inside Lyra 5.4.4.

## Movement and playable course

- Directional ground dodge with retained momentum.
- Wall dodge while airborne, re-armed on landing.
- A second jump with independent vertical velocity.
- Quake-style air acceleration and landing recovery.
- Four ordered checkpoints, a run timer, speed readout and restart action.
- A Blueprint On Dodged graph connected to the C++ presentation function.

The course uses an original geometric environment and Lyra's native character and ability types. It does not require ShooterCore's artwork or front-end experience.

## Build and launch

Use a fresh Lyra 5.4 sample copy and an Unreal Engine 5.4 installation:

```bash
python3 Tools/portfolio.py --engine /path/to/UE5.4 --lyra /path/to/Lyra --run
```

The helper installs the two project plugins into that copy, adds input mappings and an arena-specific startup configuration, then compiles LyraEditor and generates native assets. It disables the shooter game-feature plugins and their UI policy in that separate copy so the course can run without the shooter artwork. It adds `LYRAGAME_API` to LyraAssetManager and LyraGameData in the sample headers so the demo module can extend them. Use a separate sample copy to keep this integration isolated.

Add `--package` for a Development build or `--capture` for native viewport footage. Capture requires FFmpeg and a working graphics renderer.

Controls: WASD move, mouse look, Space jump twice, Shift dodge and R restart. To wall dodge, jump near a wall and dodge away from it.

## Prediction and integration

The GAS ability requests a dodge rather than applying velocity itself. The request travels with a saved movement flag; the movement component evaluates it during simulation. Custom saved moves retain recovery and wall-dodge state for replay. A replicated counter triggers presentation on other players without an extra multicast RPC.

To integrate the movement into another Lyra experience, use ArenaCharacter as the pawn class, add ArenaGameplayAbility_Dodge to its ability set and bind InputTag.Ability.Dodge. Tune speed, lift, air acceleration and recovery on the movement component.

Server position corrections currently omit the custom recovery state. A prediction disagreement may correct the client's position until landing resets that state. Air acceleration uses world-Z gravity. This revision does not claim native multiplayer validation.

## Gameplay checks

```bash
cmake -S Checks -B build
cmake --build build
./build/arena_checks
```

Eight scenarios cover ground and wall-dodge rules, momentum, recovery, air-speed capping, deterministic results and strafe acceleration. They verify the engine-independent movement rules. Native gameplay footage will be linked after the new scene has run in UE5.

Epic's Lyra source and sample content remain subject to Epic's license and are not redistributed in this repository.
