# Current status

Last updated 2026-09-13 after the visual rendering lab landed on `feat/visual-rendering-lab`.

## Outcome

Milestones 0 / 1A / 1B / 1C remain **PROVEN** (human pixel verification).

The visual rendering lab is **READY FOR HUMAN TUNING**. Automated smoke proves init, instanced star draws, bloom on/off, Present, and a clean exit. It cannot claim pixels or aesthetics.

## Visual rendering lab

**READY FOR HUMAN TUNING.** Not aesthetically proven.

Same `eo-map-carbon-neweden` host. Geometry and camera are unchanged. Systems are instanced camera-facing discs coloured from Contract A `star_temperature`. Gates stay `TOP_LINES` with distance fade. Bloom is a host TrinityAL HDR extract/blur/composite, independently togglable. A Win32 slider panel is created only in interactive mode.

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `.\scripts\build-neweden.ps1` |
| B. compile | **pass** | new shaders + `neweden_main.cpp`; C5030 ATL warning only |
| C. link | **pass** | `.cmake-build-triangle-debug\bin\eo-map-carbon-neweden.exe` |
| D. catalogues | **pass** | 5485 systems + 6989 edges + 5485 temperatures (2010–7496 K); Jita F / 7305 K |
| E. renderer init | **pass** | `TrinityAL CreateDevice succeeded` |
| F. present + bloom | **pass** | first Present; 40 bloom-on frames (6 draws / 4 PP) then 20 bloom-off (3 / 1) |
| G. pixels / look | **not claimed** | human must tune and look |

### Visual lab smoke log

```
system count: 5485 (known-space 5485, other-space 0)
connection count: 6989
star temperature window: 2010-7496 K (Jita=7305 K F)
first Present completed
smoke: frames=60 ... bloom_on_draws=6 bloom_on_pp=4 bloom_off_draws=3 bloom_off_pp=1 ... avg_frame_ms=0.43 avg_fps=2304.2
```

The 0.43 ms / 2304 fps figure is QPC around BeginScene through Present with `PRESENT_INTERVAL_IMMEDIATE` in `--smoke` only. It is not a vsync-capped interactive measurement. Frozen triangle (30 frames) and synthetic starfield (25k points) stayed green.

Launch for human tuning:

```powershell
.\scripts\run-visual-lab.ps1
```

See [docs/visual-rendering-plan.md](visual-rendering-plan.md).

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

- Visual look is not proven. Defaults are a starting preset for human sliders.
- `EveSpriteSet` / `Tr2QuadRenderer` / `Tr2PPBloomEffect` cannot be used from this TrinityAL-only host (Blue / `res:/` / Eve scene).
- Gate lines remain 1 px. There is still no DX11 line-width API.
- W-space is intentionally omitted.
- Metal marks `TOP_POINTS` `validType=false`. This host is DX11-only.
- Resize of bloom RTs is implemented from verified APIs but was not interactively exercised in smoke.
- Debug CRT is `/MD`. Global git `insteadOf` is still mutated by configure.
- No labels, picking, routing, security colours, ESI, or product UI.

## Human smoke (already done)

Re-run if Carbon or the host changes:

```powershell
.\scripts\run-visual-lab.ps1
.\scripts\run-starfield.ps1
.\scripts\run-triangle.ps1
```

Milestones 0 / 1A / 1B / 1C are already human-verified. Re-run those only if Carbon or the frozen hosts change. The visual lab needs a human to look and tune.
