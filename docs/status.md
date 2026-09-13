# Current status

Last updated 2026-09-13 after Milestone 1B New Eden point-cloud work.

## Outcome

**PROVEN PENDING HUMAN VISUAL** for Milestone 1B (real New Eden known-space geometry).

Milestone 0 (red triangle) remains human-verified. Milestone 1A (synthetic TrinityAL starfield plus EO-Map-matching orbit/pan/zoom) remains human-verified. Automated 1B smoke loaded the pinned Contract A export, checked 5485 known-space rows and five hub anchors, created the vertex buffer, presented 60 frames, and exited 0. A human still has to look at the window and compare the cluster to EO-Map.

## Milestone 0 — triangle

**PROVEN**, including human pixel verification.

The frozen diagnostic target is unchanged: `eo-map-carbon-triangle`, `src/triangle_main.cpp`, the two original shaders, `.\scripts\run-triangle.ps1`.

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `cmake --preset triangle-debug` |
| B. compile | **pass** | triangle sources untouched |
| C. link | **pass** | existing `eo-map-carbon-triangle.exe` |
| D–F. smoke | **pass** | `--smoke` exit 0 after 1B work; `CreateDevice succeeded`; `first Present completed`; 30 frames |
| G. pixels | **pass (human)** | red triangle on dark blue-grey, previously confirmed |

## Milestone 1A — synthetic starfield

**PROVEN**, including human pixel and camera verification.

The frozen 1A host is unchanged: `eo-map-carbon-starfield`, `src/starfield_main.cpp`, synthetic `GenerateStars()`. New Eden data was not folded into it.

| Gate | Result | Evidence |
| --- | --- | --- |
| A–G. previous | **pass (human)** | see the 1A closeout |
| Re-smoke after 1B | **pass** | `--smoke` exit 0; `star count: 25000`; `first Present completed`; 60 frames |

## Milestone 1B — real New Eden geometry

**PROVEN PENDING HUMAN VISUAL**

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `.\scripts\build-neweden.ps1` / `cmake --preset triangle-debug` |
| B. compile | **pass** | `neweden_main.cpp.obj`, `new_eden_catalog.cpp.obj`; C5030 from v143 ATL on v141 is a warning |
| C. link | **pass** | `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden.exe` |
| D. catalogue | **pass** | loaded `new_eden_systems.bin`; 5485 known-space / 0 other-space; source sha256 `874262496556d933bbbc184cdec8f469660f6c1108b21e25f0481475b68eba81` |
| E. anchors | **pass** | Jita / Amarr / Dodixie / Rens / Hek scene coordinates match the EO-Map display transform |
| F. renderer init | **pass** | `adapter count: 3` then `TrinityAL CreateDevice succeeded` |
| G. present | **pass** | `first Present completed`; 60-frame smoke exit 0; 1 draw/frame |
| H. pixels / orientation | **pending human** | automated smoke cannot claim the cluster looks like EO-Map |

### New Eden smoke log

File: `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden-smoke.log` (gitignored)

```
eo-map-carbon-neweden starting
renderer: TrinityAL DX11
path: TOP_POINTS DrawPrimitive (one call)
draw calls per frame: 1
dataset: map_data_eo_3464040.db builder=1.5.0 SDE=3464040
dataset file: C:\dev\eo-map-carbon\.cmake-build-triangle-debug\bin\new_eden_systems.bin
dataset source sha256: 874262496556d933bbbc184cdec8f469660f6c1108b21e25f0481475b68eba81
system count: 5485 (known-space 5485, other-space 0)
anchor check: Jita/Amarr/Dodixie/Rens/Hek scene coordinates match EO-Map transform
adapter count: 3
window hwnd=00000000001B1456 1280x720
TrinityAL CreateDevice succeeded
Rendering 5485 New Eden systems via TOP_POINTS. Left-drag orbits, right-drag pans, wheel zooms. Close the window to exit.
first Present completed
camera eye=43.9,106.4,145.9 target=-9.1,-4.0,0.6 distance=190.0
smoke test reached 60 frames, exiting
smoke: frames=60 systems=5485 known_space=5485 other_space=0 draw_calls/frame=1 avg_frame_ms=0.31 avg_fps=3232.5 path=TrinityAL_DX11/TOP_POINTS dataset=SDE3464040
exiting after 60 frames
```

Process exit code: 0.

The 0.31 ms / 3232 fps figure is QPC around BeginScene through Present with `PRESENT_INTERVAL_IMMEDIATE` in `--smoke` only. It is not a vsync-capped interactive measurement.

## Architecture actually used (1B)

- Third WIN32 host. Triangle and synthetic starfield stay frozen diagnostics.
- Data: `data/new_eden_systems.bin`, exported from EO-Map Contract A `map_data_eo_3464040.db` (SDE 3464040, builder 1.5.0). Not a second SDE interpretation. No SQLite / ESI / network in the executable.
- Selection: 5,485 New Eden known-space systems (`id` 30xxxxxx, `hidden = 0`). The source table also has 2,604 W-space systems in a separate ~1,300 LY cluster; those are omitted so the first view is New Eden.
- Binary stores Contract A `position_x/y/z` (light years after builder remap `(raw.x, raw.z, raw.y) / 9.46e15`).
- Host display mapping matches live EO-Map `dbToScenePosition`: `scene = (db.x, -db.z, -db.y)`. No extra scale. Vertices are not recentred.
- Camera target is the measured New Eden AABB centre `(-9.103, -3.997, 0.613)` LY. Same orbit / pan / zoom signs as 1A.
- Draw: `SetTopology(TOP_POINTS)` then `DrawPrimitive(0, 5485)`. One call per frame. Same 1-pixel DX11 points as 1A.

## Known gaps

- 1B pixels and orientation vs EO-Map are not human-verified yet.
- Point size is not controllable through TrinityAL on DX11. If 5,485 1-pixel systems are too faint to read the cluster, that has to be demonstrated before any sprite-path rewrite.
- W-space is intentionally omitted from this first visual.
- Metal marks `TOP_POINTS` `validType=false`. This host is DX11-only.
- Resize is implemented from verified APIs but was not interactively exercised in smoke.
- Debug CRT is `/MD`. Global git `insteadOf` is still mutated by configure. Paths still assume `C:\dev\eo-map-carbon` and `C:\dev\carbon-upstream\trinity`.
- No labels, picking, jump gates, routing, security colours, regions, ESI, or UI (intentionally out of scope).

## Human smoke (1B — do this next)

```powershell
.\scripts\run-neweden.ps1
```

Compare the cluster to EO-Map's default 3D New Eden home (not the 2D schematic morph, not wormhole home). See the closeout for the visual checklist.
