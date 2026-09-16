# Current status

Last updated 2026-09-16 after consolidating Creator Mode, the Jita-Amarr routing experiment, and the TypeSafe Jev experiments onto `main`.

## Outcome

Milestones 0 / 1A / 1B / 1C remain **PROVEN** (human pixel verification of triangle, starfield, New Eden cluster, and stargate topology).

The New Eden host is a **Creator Mode visual lab** plus an experimental Jita-Amarr route overlay. Automated smoke proves init, persist parse, region sidecar, instanced stars, glow, sky, ISM, bloom on/off, creator-off composite, Present, numeric gate/chroma contracts, Jita-Amarr = 11 hops, and a clean exit. It cannot claim pixels or aesthetics. The route overlay has not been separately human-verified.

TypeSafe Jev work is isolated under `experiments/typesafe/`. It is not part of the Carbon host.

## Creator Mode

**READY FOR HUMAN TUNING.** Not aesthetically proven.

Launch:

```powershell
.\scripts\run-creator-mode.ps1
```

Same as `.\scripts\run-visual-lab.ps1` / `.\scripts\run-neweden.ps1`. Opens the New Eden window titled **EO-Map Carbon New Eden Creator Mode (TrinityAL DX11)** and the **Creator / Visual lab** tool window.

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** (Creator Mode branch) | `.\scripts\build-neweden.ps1` |
| B. compile | **pass** (Creator Mode branch) | new shaders + host; C5030 ATL warning only |
| C. link | **pass** (Creator Mode branch) | `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden.exe` |
| D. catalogues | **pass** (Creator Mode branch) | 5485 / 6989 / 5485 temps / 70 regions; Jita 7305 K / region 10000002 |
| E. persist | **pass** (Creator Mode branch) | parse / clamp / unknown-key / roundtrip |
| F. renderer init | **pass** (Creator Mode branch) | `TrinityAL CreateDevice succeeded` |
| G. present | **pass** after consolidation | first Present; 40 bloom-on (10 draws / 6 PP) then bloom-off creator (7 / 3) then creator-off (4 / 1), including the extra route pass |
| H. pixels / look | **not claimed** | human must look and tune |
| I. Jita-Amarr overlay | **code + graph + smoke; pixels not claimed** | `ShortestRoute` 11 hops; second GateLine pass; smoke `route_draws=1` |

After consolidation the smoke expected draw counts include one extra route pass: bloom-on 10/6 (9/6 with `--points`), bloom-off creator 7/3 (6/3 with `--points`), creator-off 4/1. Confirmed on this tree: `route hops: 11` and `route_draws=1`.

See [docs/creator-mode-port.md](creator-mode-port.md) and [docs/visual-rendering-plan.md](visual-rendering-plan.md).

## Jita-Amarr routing experiment

Graph reconstruction and validation live in `src/new_eden_gates.cpp` (`ShortestRoute`, endpoint/hop/edge checks inside `ValidateGraph`). Independent Python BFS: `scripts/test-jita-amarr-route.py`.

Expected path (11 hops):

`Jita -> Ikuchi -> Ansila -> Hykkota -> Ahbazon -> Shera -> Gensela -> Dresi -> Aphend -> Romi -> Bhizheba -> Amarr`

The visual is a second `TOP_LINES` draw using the existing Creator Mode `GateLine` program with higher opacity and a lighter tint, depth-write disabled so overlapping gate segments do not z-fight. It is not the old grayscale three-draw host.

Jev preflight/postflight for that coding task: [experiments/typesafe/ROUTING_EXPERIMENT.md](../experiments/typesafe/ROUTING_EXPERIMENT.md).

## TypeSafe / Jev experiments

Not a renderer milestone. Not wired into any TrinityAL executable.

| Experiment | Report |
| --- | --- |
| Repo comprehension (37 gold questions, 5 live runs) | [experiments/typesafe/REPORT.md](../experiments/typesafe/REPORT.md) |
| Jita-Amarr coding-task preflight/postflight | [experiments/typesafe/ROUTING_EXPERIMENT.md](../experiments/typesafe/ROUTING_EXPERIMENT.md) |
| Synthetic tactical commander (220 live `jev-1.13.0` calls) | [experiments/typesafe/JEV_TACTICAL_COMMANDER.md](../experiments/typesafe/JEV_TACTICAL_COMMANDER.md), [experiments/typesafe/COMMANDER_BENCHMARK.md](../experiments/typesafe/COMMANDER_BENCHMARK.md) |

The commander test is Python / System One over scripted battlefield snapshots. There are no Carbon NPCs.

## Milestone 0 — triangle

**PROVEN**, including human pixel verification.

The frozen diagnostic target is unchanged: `eo-map-carbon-triangle`, `src/triangle_main.cpp`, the two original shaders, `.\scripts\run-triangle.ps1`.

## Milestone 1A — synthetic starfield

**PROVEN**, including human pixel and camera verification.

The frozen 1A host is unchanged.

## Milestone 1B / 1C — New Eden geometry and gates

**PROVEN**, including human pixel, orientation, topology, and camera verification.

Geometry, camera signs, and catalogues are unchanged. Creator Mode draws extra passes around that proven path. The Jita-Amarr overlay is additional and not part of the original 1C human proof.

## Architecture actually used (Creator Mode)

- Same WIN32 host and camera as 1B/1C. Triangle and synthetic starfield stay frozen.
- Data: existing `NEDEN1B` / `NEGATE1` / `NESTAR1` plus `NEREGN1` region sidecar. No SQLite / ESI / network in the executable.
- Sky: one fullscreen `DeepSpace.psh` into the HDR scene RT. Hash stars are view-direction only.
- ISM: half-res `IsmField.psh`, plane-centred 8/16/24/32/48-step world disc, composite multiplies scene/bloom by transmittance and adds emission.
- Glow / flare: extra `DrawIndexedInstanced` batches, additive, thresholded. Not one draw per star.
- Persist: INI next to the exe, loaded on interactive startup only.
- Route overlay: extra `TOP_LINES` buffer, same `GateLine` shaders, brighter constants.

## Carbon/Trinity APIs newly used

None beyond the already-verified TrinityAL RT / CB / resource-set / instanced-draw surface. New work is host shaders, extra passes, and graph parent-pointer BFS.

## Known gaps

- Visual look is not proven. Human must open Creator Mode and look. The route overlay needs the same look.
- ISM is an analytic flared disc, not EO-Map's 96³ bake.
- Region tint is atlas modulo-11, not Creator highlight/dim/hide.
- Gate lines remain 1 px. The route overlay is still 1 px, just brighter.
- W-space is intentionally omitted.
- Resize of the extra RTs follows the existing recreate path but was not interactively exercised after consolidation.
- Debug CRT is `/MD`. Global git `insteadOf` is still mutated by configure.
- No labels, picking, search, security colours, ESI, jump bridges, or product UI.
- Jev is not in the native process. Community API latency is not a production NPC loop.

## Human smoke

```powershell
.\scripts\run-creator-mode.ps1
```

Milestones 0 / 1A / 1B / 1C are already human-verified. Re-run those only if Carbon or the frozen hosts change. After consolidation, a human still needs to look at Creator Mode **and** confirm the Jita-Amarr overlay is visible and attached.
