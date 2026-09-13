# Current status

Last updated 2026-09-13 after human visual verification of Milestone 1C.

## Outcome

**PROVEN**

Milestone 0 (red triangle) is human-verified. Milestone 1A (synthetic TrinityAL starfield plus EO-Map-matching orbit/pan/zoom) is human-verified. Milestone 1B (real New Eden known-space geometry on the same TrinityAL path) is human-verified. Milestone 1C (real New Eden stargate graph on that same host) is human-verified.

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

**PROVEN**, including human pixel, orientation, and camera verification.

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `.\scripts\build-neweden.ps1` / `cmake --preset triangle-debug` |
| B. compile | **pass** | `neweden_main.cpp.obj`, `new_eden_catalog.cpp.obj`; C5030 from v143 ATL on v141 is a warning |
| C. link | **pass** | `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden.exe` |
| D. catalogue | **pass** | loaded `new_eden_systems.bin`; 5485 known-space / 0 other-space; source sha256 `874262496556d933bbbc184cdec8f469660f6c1108b21e25f0481475b68eba81` |
| E. anchors | **pass** | Jita / Amarr / Dodixie / Rens / Hek scene coordinates match the EO-Map display transform |
| F. renderer init | **pass** | `adapter count: 3` then `TrinityAL CreateDevice succeeded` |
| G. present | **pass** | `first Present completed`; 60-frame smoke exit 0; 1 draw/frame |
| H. pixels / orientation | **pass (human)** | recognisable New Eden cluster and orientation; no distant W-space blob |
| I. orbit / pan / zoom | **pass (human)** | left-drag orbit, right-drag pan, wheel zoom; remains responsive |

1-pixel points and a plain white/grey look are accepted for this milestone. They are not a 1B failure.

## Milestone 1C — New Eden stargate graph

**PROVEN**, including human pixel, topology, and camera verification.

Same host as 1B. Systems path is unchanged. Gates are a second static buffer.

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `.\scripts\build-neweden.ps1` / `cmake --preset triangle-debug` |
| B. compile | **pass** | `neweden_main.cpp.obj`, `new_eden_gates.cpp.obj`; C5030 from v143 ATL on v141 is a warning |
| C. link | **pass** | `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden.exe` |
| D. catalogues | **pass** | 5485 systems + 6989 undirected edges; source sha256 `874262496556d933bbbc184cdec8f469660f6c1108b21e25f0481475b68eba81` |
| E. graph checks | **pass** | every endpoint in the 1B catalogue; Jita/Amarr/Dodixie/Rens/Hek/Zarzakh adjacency; Jita→Amarr = 11; Niarja unreachable; Jita reachable = 5228 |
| F. renderer init | **pass** | `adapter count: 3` then `TrinityAL CreateDevice succeeded` |
| G. present | **pass** | `first Present completed`; 60-frame smoke exit 0; 2 draws/frame |
| H. pixels / 3D topology | **pass (human)** | recognisable New Eden plus a visibly correct stargate network; lines stay attached while rotating |
| I. orbit / pan / zoom | **pass (human)** | left-drag orbit, right-drag pan, wheel zoom; remains responsive |

### New Eden 1C smoke log

File: `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden-smoke.log` (gitignored)

```
eo-map-carbon-neweden starting
renderer: TrinityAL DX11
path: TOP_LINES + TOP_POINTS DrawPrimitive (two calls)
draw calls per frame: 2
dataset: map_data_eo_3464040.db builder=1.5.0 SDE=3464040
dataset file: C:\dev\eo-map-carbon\.cmake-build-triangle-debug\bin\new_eden_systems.bin
dataset source sha256: 874262496556d933bbbc184cdec8f469660f6c1108b21e25f0481475b68eba81
system count: 5485 (known-space 5485, other-space 0)
stargate file: C:\dev\eo-map-carbon\.cmake-build-triangle-debug\bin\new_eden_stargates.bin
connection count: 6989 (undirected; source directed rows=13978)
anchor check: Jita/Amarr/Dodixie/Rens/Hek scene coordinates match EO-Map transform
graph check: endpoints in catalogue, Jita/Amarr/Dodixie/Rens/Hek/Zarzakh adjacency, Jita-Amarr hops=11, Niarja unreachable
adapter count: 3
window hwnd=0000000000420B9C 1280x720
TrinityAL CreateDevice succeeded
Rendering 5485 New Eden systems via TOP_POINTS and 6989 stargate connections via TOP_LINES. Left-drag orbits, right-drag pans, wheel zooms. Close the window to exit.
first Present completed
camera eye=43.9,106.4,145.9 target=-9.1,-4.0,0.6 distance=190.0
smoke test reached 60 frames, exiting
smoke: frames=60 systems=5485 connections=6989 known_space=5485 other_space=0 draw_calls/frame=2 point_draws=1 gate_draws=1 avg_frame_ms=0.25 avg_fps=4051.4 path=TrinityAL_DX11/TOP_LINES+TOP_POINTS dataset=SDE3464040
exiting after 60 frames
```

Process exit code: 0.

The 0.25 ms / 4051 fps figure is QPC around BeginScene through Present with `PRESENT_INTERVAL_IMMEDIATE` in `--smoke` only. It is not a vsync-capped interactive measurement. Re-smoke after human verification also exited 0 (5485 systems, 6989 connections, 60 frames). Frozen triangle (30 frames) and synthetic starfield (25k points) stayed green.

### Architecture actually used (1C)

- Same WIN32 host and camera as 1B. Triangle and synthetic starfield stay frozen.
- Data: `data/new_eden_stargates.bin` (`NEGATE1`), exported from the same pinned Contract A DB. Unique undirected pairs `source_id < dest_id`. No SQLite / ESI / network in the executable.
- Filtering: Contract A `stargates` is already known-space only (builder fails the build if a W-space id appears). Carbon additionally requires both ends in the 1B system set and drops the reciprocal duplicate.
- Draw: `SetTopology(TOP_LINES)` then `DrawPrimitive(0, 6989)` on a 13,978-vertex endpoint buffer, then the existing `TOP_POINTS` / `DrawPrimitive(0, 5485)`. Two calls per frame. Same `Starfield.vsh` / `StarColor.psh`; gate intensity is a constant 0.22 grey.
- Geometry: straight 3D segments between the 1B scene positions. No 2D layout, curves, bloom, or security colour.

### Carbon/Trinity APIs newly used

- `Tr2RenderContextEnum::TOP_LINES` (DX11 `D3D11_PRIMITIVE_TOPOLOGY_LINELIST`)
- `DrawPrimitive(startVertex, primitiveCount)` with `primitiveCount` = number of line segments (`ComputeVertexCount` returns `2 * primitiveCount`)

## Architecture actually used (1B)

- Third WIN32 host. Triangle and synthetic starfield stay frozen diagnostics.
- Data: `data/new_eden_systems.bin`, exported from EO-Map Contract A `map_data_eo_3464040.db` (SDE 3464040, builder 1.5.0). Not a second SDE interpretation. No SQLite / ESI / network in the executable.
- Selection: 5,485 New Eden known-space systems (`id` 30xxxxxx, `hidden = 0`). The source table also has 2,604 W-space systems in a separate ~1,300 LY cluster; those are omitted so the first view is New Eden.
- Binary stores Contract A `position_x/y/z` (light years after builder remap `(raw.x, raw.z, raw.y) / 9.46e15`).
- Host display mapping matches live EO-Map `dbToScenePosition`: `scene = (db.x, -db.z, -db.y)`. No extra scale. Vertices are not recentred.
- Camera target is the measured New Eden AABB centre `(-9.103, -3.997, 0.613)` LY. Same orbit / pan / zoom signs as 1A.
- Draw: `SetTopology(TOP_POINTS)` then `DrawPrimitive(0, 5485)`. One call per frame. Same 1-pixel DX11 points as 1A.

## Known gaps

- Point size is not controllable through TrinityAL on DX11. 1-pixel systems are accepted for 1B; later size needs the verified instanced-triangle path, not `RS_POINTSIZE`.
- W-space is intentionally omitted from this first visual.
- Metal marks `TOP_POINTS` `validType=false`. This host is DX11-only.
- Resize is implemented from verified APIs but was not interactively exercised in smoke.
- Debug CRT is `/MD`. Global git `insteadOf` is still mutated by configure. Paths still assume `C:\dev\eo-map-carbon` and `C:\dev\carbon-upstream\trinity`.
- No labels, picking, jump bridges, routing, security colours, regions, ESI, or UI (intentionally out of scope).
- 1-pixel systems remain faint versus EO-Map. That look is still deferred.

## Human smoke (already done)

Re-run if Carbon or the host changes:

```powershell
.\scripts\run-neweden.ps1
.\scripts\run-starfield.ps1
.\scripts\run-triangle.ps1
```

Milestone 1C is already human-verified. Re-run only if Carbon or the host changes.
