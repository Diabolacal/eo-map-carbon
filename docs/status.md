# Current status

Last updated 2026-09-13 after the ISM volume fix on `feat/carbon-creator-mode`.

## Outcome

Milestones 0 / 1A / 1B / 1C remain **PROVEN** (human pixel verification).

The visual rendering lab has been extended into **Creator Mode**. Automated smoke proves init, persist parse, region sidecar, instanced stars, glow, sky, ISM, bloom on/off, creator-off composite, Present, numeric gate/chroma contracts, and a clean exit. It cannot claim pixels or aesthetics.

Human-tuned star / gate / bloom / sky / glow / ISM colour values are the checked-in `TuneParams` baseline. The ISM spatial model is a flared world-space disc, not the earlier 4-tap Y-slab.

## Creator Mode

**READY FOR HUMAN TUNING.** Not aesthetically proven.

Launch:

```powershell
.\scripts\run-creator-mode.ps1
```

Same as `.\scripts\run-visual-lab.ps1`. Opens the New Eden window titled **EO-Map Carbon New Eden Creator Mode (TrinityAL DX11)** and the **Creator / Visual lab** tool window.

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `.\scripts\build-neweden.ps1` |
| B. compile | **pass** | new shaders + host; C5030 ATL warning only |
| C. link | **pass** | `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden.exe` |
| D. catalogues | **pass** | 5485 / 6989 / 5485 temps / 70 regions; Jita 7305 K / region 10000002 |
| E. persist | **pass** | parse / clamp / unknown-key / roundtrip |
| F. renderer init | **pass** | `TrinityAL CreateDevice succeeded` |
| G. present | **pass** | first Present; 40 bloom-on (9 draws / 6 PP) then bloom-off creator (6 / 3) then creator-off (3 / 1) |
| H. pixels / look | **not claimed** | human must look and tune |

### Creator Mode smoke log

```
ism math: centre_env=1.000 far=0.00000 above=0.00000 ... default_ray=0.857 centre_tau=0.677 screen_hits=115/336 (34.2%) radius=32.0 thick=8.0 far_clip=155.3
persist check: parse/clamp/unknown-key/roundtrip ok
first Present completed
ism_qpc: default_steps=16 avg_ms=3.67 cinematic_steps=48 avg_ms=1.04 (immediate present; not a GPU profiler)
smoke: frames=60 ... bloom_on_draws=9 bloom_on_pp=6 bloom_off_creator_draws=6 bloom_off_creator_pp=3 creator_off_draws=3 creator_off_pp=1 ... avg_frame_ms=1.75 avg_fps=571.0
```

The QPC figures are BeginScene through Present with `PRESENT_INTERVAL_IMMEDIATE` in `--smoke` only. The 16-step average includes first-frame hitch; a warmed `--points` smoke measured 0.85 ms at 16 steps and 0.92 ms at 48 steps. This is not a vsync-capped interactive measurement or a GPU profiler. Frozen triangle (30 frames) and synthetic starfield (25k points) stayed green. `--points` smoke is 8 / 6 then 5 / 3 then 3 / 1 (no glow draw).

See [docs/creator-mode-port.md](creator-mode-port.md) and [docs/visual-rendering-plan.md](visual-rendering-plan.md).

## Milestone 0 — triangle

**PROVEN**, including human pixel verification.

The frozen diagnostic target is unchanged: `eo-map-carbon-triangle`, `src/triangle_main.cpp`, the two original shaders, `.\scripts\run-triangle.ps1`.

## Milestone 1A — synthetic starfield

**PROVEN**, including human pixel and camera verification.

The frozen 1A host is unchanged.

## Milestone 1B / 1C — New Eden geometry and gates

**PROVEN**, including human pixel, orientation, topology, and camera verification.

Geometry, camera signs, catalogues, and the unpremultiplied gate-grey contract are unchanged. Creator Mode draws extra passes around that proven path.

## Architecture actually used (Creator Mode)

- Same WIN32 host and camera as 1B/1C. Triangle and synthetic starfield stay frozen.
- Data: existing `NEDEN1B` / `NEGATE1` / `NESTAR1` plus new `NEREGN1` region sidecar. No SQLite / ESI / network in the executable.
- Sky: one fullscreen `DeepSpace.psh` into the HDR scene RT. Hash stars are view-direction only.
- ISM: half-res `IsmField.psh`, plane-centred 8/16/24/32/48-step world disc, composite multiplies scene/bloom by transmittance and adds emission.
- Glow / flare: extra `DrawIndexedInstanced` batches, additive, thresholded. Not one draw per star.
- Persist: INI next to the exe, loaded on interactive startup only.

## Carbon/Trinity APIs newly used

None beyond the already-verified TrinityAL RT / CB / resource-set / instanced-draw surface. New work is host shaders and extra passes.

## Known gaps

- Visual look is not proven. Human must open Creator Mode and tune.
- ISM is an analytic flared disc, not EO-Map's 96³ bake. Aesthetics still need a human look.
- Region tint is atlas modulo-11, not Creator highlight/dim/hide.
- Gate lines remain 1 px.
- W-space is intentionally omitted.
- Resize of the new RTs follows the existing recreate path but was not interactively exercised.
- Debug CRT is `/MD`. Global git `insteadOf` is still mutated by configure.
- No labels, picking, routing, security colours, ESI, or product UI.

## Human smoke

```powershell
.\scripts\run-creator-mode.ps1
```

Milestones 0 / 1A / 1B / 1C are already human-verified. Re-run those only if Carbon or the frozen hosts change. Creator Mode needs a human to look and tune.
