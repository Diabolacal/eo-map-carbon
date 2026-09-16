# Agent guidance

This is a personal experiment: can an external developer consume public Carbon/Trinity and render with TrinityAL. It is not a product, not an EO-Map port, and not a client modification.

## Non-goals

Do not add ESI, SSO, labels, picking, jump bridges, security colours, sovereignty, search, installers, or production packaging unless a later milestone explicitly asks for that one thing. Creator Mode already has a developer tuning window, INI persist, and an optional region-tint sidecar. The host already has one experimental Jita-Amarr shortest-route overlay. Do not grow those into product UI or a general router.

Milestone 1B already adds a static New Eden known-space point cloud. Do not grow that into a map product.

Do not replace TrinityAL with raw DirectX, OpenGL, SDL renderer, Three.js, a WebView, or any other engine.

Do not fork Carbon. Trinity lives outside this repo.

## Proven Carbon bootstrap (Milestone 0)

Human-verified. Do not redo or redesign it.

The triangle host builds against public Trinity, uses TrinityAL DX11, opens a native Win32 window, creates a valid render context, draws one red triangle, and presents continuously. Pixel contents have been confirmed by a human looking at the window.

Keep `src/triangle_main.cpp`, `src/shaders/PositionOnly.vsh`, `src/shaders/ConstantColor.psh`, and `scripts/run-triangle.ps1` as a diagnostic target. Do not casually edit the triangle draw path, window title, clear colour, or smoke behaviour.

## Proven starfield (Milestone 1A)

Human-verified. Do not redo it.

`eo-map-carbon-starfield` draws 25,000 deterministic synthetic stars through TrinityAL `TOP_POINTS` in one `DrawPrimitive`, with an explicit orbit target. Left-drag orbit, right-drag pan, and wheel zoom match EO-Map's three.js `OrbitControls` signs. Interactive performance on this machine was about 240 FPS / 4.16 ms.

Do not fold later map features into this host. New Eden coordinates live in `eo-map-carbon-neweden`, not here.

## Proven New Eden point cloud (Milestone 1B)

Human-verified. Do not redo it.

`eo-map-carbon-neweden` loads 5,485 known-space systems from `data/new_eden_systems.bin`, an export of EO-Map's pinned Contract A artefact (`map_data_eo_3464040.db`, SDE 3464040, builder 1.5.0). The host applies EO-Map's live display mapping `scene = (db.x, -db.z, -db.y)`. Milestone 1B was proven on `TOP_POINTS`. Creator Mode later changed the default system draw to instanced quads; `TOP_POINTS` remains `--points` / F7. A human confirmed the cluster is recognisably New Eden, with correct-enough orientation and the same orbit / pan / zoom as 1A.

Do not re-interpret the SDE. Regenerate both artefacts with `scripts/export-new-eden-systems.py` from the sibling EO-Map checkout. Do not open SQLite, ESI, or the EO-Map web app from this executable.

W-space (2,604 Anoikis systems in Contract A) is a separate ~1,300 LY cluster and is omitted from this first visual.

## Proven New Eden stargate graph (Milestone 1C)

Human-verified. Do not redo it.

`eo-map-carbon-neweden` also loads `data/new_eden_stargates.bin`: 6,989 unique undirected known-space connections from the same Contract A `stargates` table (13,978 directed rows, already k-space only). Creator Mode draws that network as faded `TOP_LINES` through the `GateLine` program. `ShortestRoute` reconstructs the ordered Jita-Amarr path (expected 11 hops); the host draws those segments as a second, brighter `GateLine` pass. A human confirmed the 1C network is the real New Eden graph. The route overlay is an experiment on top of that; its pixels are not separately human-proven.

Do not re-interpret the SDE. Regenerate artefacts with `scripts/export-new-eden-systems.py`. Do not add W-space, wormholes, jump bridges, security colours, or additional routes here.

## Visual rendering lab / Creator Mode

Aesthetics are not proven. Human look-and-tune is required.

The New Eden host draws instanced camera-facing star quads, temperature colour, faded `TOP_LINES` gates, HDR bloom, a procedural view-locked sky, a half-res world-space ISM disc raymarch, optional glow/flare, optional region tint, and a tabbed Win32 Creator panel. `TOP_POINTS` remains `--points` / F7.

`TuneParams` defaults are the locked human baseline for stars/gates/bloom/exposure, plus restrained cinematic layers. F9 / Baseline reset to `TuneDefaults()`. Interactive mode loads `eo-map-carbon-neweden-tune.ini` next to the exe if present. `--smoke` never loads it.

Do not replace TrinityAL with raw DirectX. Do not pull in `EveSpriteSet` / `Tr2PPBloomEffect`. Do not copy EVE client skyboxes or textures. See `docs/visual-rendering-plan.md` and `docs/creator-mode-port.md`.

## Upstream Trinity

Checkout (not a submodule):

```
C:\dev\carbon-upstream\trinity
https://github.com/carbonengine/trinity
```

Milestone 0 was proven against `f26b1f2bfdf37e63f85a209b6d878f6e8b02a67d` (`v6.0.0-2-gf26b1f2b`).

Treat current Carbon source as authoritative. Compiler and runtime behaviour win over comments, docs, and memory.

## Verify APIs. Do not invent them.

Every Carbon/Trinity API, enum, class, method, or rendering sequence must be read from the current public tree before you use it. TrinityAL tests under `trinityal/tests` are the usual pattern source.

If an API is missing, document the gap. Do not hide it behind a substitute renderer.

Full Trinity (`EveStarfield`, `TriView`, Blue/Python/`exefile`) is not the TrinityAL surface. Do not pull those in to paper over an AL-only host.

## Executables

Three WIN32 hosts, one CMake project, one vcpkg prefix:

- `eo-map-carbon-triangle` — frozen Milestone 0 diagnostic.
- `eo-map-carbon-starfield` — frozen Milestone 1A synthetic 3D starfield.
- `eo-map-carbon-neweden` — proven 1B/1C geometry plus Creator Mode (instanced stars, bloom, sky, ISM, glow, persist, Win32 sliders) and an experimental Jita-Amarr route overlay. Aesthetics are not proven.

Do not fold camera/depth/starfield/New Eden changes into `triangle_main.cpp`.
Do not replace the synthetic 1A generator with New Eden data.

## Build and run

From a Developer PowerShell (x64) in this repo, after the machine-local Trinity/ATL/fxc prerequisites in the README:

```powershell
.\scripts\configure-trinity.ps1
.\scripts\build-trinity.ps1
.\scripts\build-triangle.ps1
.\scripts\run-triangle.ps1
.\scripts\run-triangle.ps1 -Smoke
.\scripts\build-starfield.ps1
.\scripts\run-starfield.ps1
.\scripts\run-starfield.ps1 -Smoke
.\scripts\build-neweden.ps1
.\scripts\run-neweden.ps1
.\scripts\run-neweden.ps1 -Smoke
.\scripts\run-visual-lab.ps1
.\scripts\run-creator-mode.ps1
```

Our CMake consumes Trinity's already-installed vcpkg prefix (`VCPKG_MANIFEST_MODE=OFF`). Do not add a second vcpkg manifest for this host.

Build trees, binaries, logs, and generated shader headers stay untracked.

## Build quirks already established

- DX11 is off unless you pass `-DBUILD_DX11=ON`. Granny stays OFF.
- Use Ninja + `vcvars64.bat -vcvars_ver=14.16`. Visual Studio generator + v141 probes Windows SDK 8.1.
- Triplets pin v141 and Windows SDK `10.0.17763.0`. Carbon Debug is `/MD`, not `/MDd`.
- Overlay `overlays/vcpkg/fxc` replaces the dead CCP FXC CDN.
- `configure-trinity.ps1` writes a global git `insteadOf` rewrite because many public Carbon portfiles still use SSH URLs.
- v141 Build Tools lack ATL. This machine junctions v143 ATL into the v141 tree. Trinity user preset sets `CMAKE_COMPILE_WARNING_AS_ERROR=OFF` because those headers emit C5030.
- Do not consume Trinity through the stale `carbon-trinity` vcpkg port (4.0.2 vs GitHub v6).
- `g_moduleName` is required by CcpCore. Each executable defines its own. Do not put it in a shared static lib.
- `CreateDevice` does not create a depth buffer. `Tr2PresentParametersAL` has no depth field. Depth is a `Tr2TextureAL` with `PIXEL_FORMAT_D24_UNORM_S8_UINT` and `Tr2GpuUsage::DEPTH_STENCIL`, then `SetDepthStencil`.
- Resize with `SetPresentParameters`. TrinityAL `CreateBackBuffers` resets the D3D viewport and unbinds depth. Recreate the depth texture after a resize.
- There is no TrinityAL camera, input helper, or `SetShaderConstant`. Matrices go through `Tr2ConstantBufferAL` (`Lock` / write / `Unlock`) + `SetConstants(..., VERTEX_SHADER, register)`. The VS signature must `Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, register)`. Mouse input is raw Win32.
- Starfield camera is host-side and must keep EO-Map drag semantics (three.js `OrbitControls`, no mouse-orbit invert): left-drag orbit uses `yaw -= dx`, `pitch += dy`; right-drag pans the orbit target in screen space (`target += -right*dx + up*dy`); wheel zooms toward the current target. Do not flip orbit signs back to a "turntable" feel.
- `TOP_POINTS` exists and is DX11 `POINTLIST`. `DrawPrimitive(start, count)` count is the number of points. No TrinityAL test draws it. `RS_POINTSIZE` / point sprites are ignored on DX11 (1-pixel points only). Metal marks `TOP_POINTS` `validType=false`; this repo is DX11-only.
- `TOP_LINES` exists and is DX11 `LINELIST`. `DrawPrimitive(start, count)` count is the number of segments; the vertex buffer holds `2 * count` endpoints. No TrinityAL test draws it. There is no line-width API. `RS_ANTIALIASEDLINEENABLE` is enum-only on DX11 and is not applied. Metal marks `TOP_LINES` valid. This host is still DX11-only.
- Full Trinity's sized-sprite path is instanced triangles (`EveSpriteSet` / `Tr2QuadRenderer`), not a TrinityAL primitive. This host reproduces that batching with TrinityAL `DrawIndexedInstanced` (verified by `CanDoInstancedRendering`). Do not instantiate Eve/Blue types. Do not invent point-sprite state.
- `Tr2PPBloomEffect` / `Tr2PostProcessRenderer` need Blue, `Tr2Effect`, and `res:/` FX files that are not in the public tree. Bloom here is a host TrinityAL HDR RT → extract → blur H/V → composite.
- `Tr2ResourceSetAL` is immutable after `Create`. Default/empty resource sets do not unbind SRVs; bind a dummy set before drawing into a texture that was sampled last frame.
- Constant-buffer `Create` size must be a multiple of 16. TrinityAL does not pad.

## TypeSafe / Jev experiments

Isolated Python work under `experiments/typesafe/`. Not a Carbon renderer milestone. Do not wire Jev, TypeSafe, or any HTTP API into a TrinityAL host.

The tactical commander benchmark is:

synthetic numeric battlefield state -> TypeSafe System One API -> typed tactical decisions -> deterministic fallback/composer

It is not Carbon NPCs, and this repo does not run NPCs inside Carbon.

Readable report: `experiments/typesafe/JEV_TACTICAL_COMMANDER.md`. Full evidence: `experiments/typesafe/COMMANDER_BENCHMARK.md`. Raw API dumps stay gitignored under `experiments/typesafe/results/`.

## Milestone discipline

One milestone at a time. Do not implement later map features while proving a renderer step.

Record newly discovered Carbon behaviour in `docs/status.md` and, if it will still be true next month, here.

Automated smoke can prove init, draw, Present, and a clean exit. It cannot claim pixels. A human has to look at the window.

## Git hygiene

The public default branch is `main`. Do not invent extra long-lived branches for already-landed experiments.

Do not commit build trees, binaries, logs, `.env`, credentials, proprietary assets, or cloned Carbon sources.

Do not commit unless asked, except when the task explicitly includes a branch/commit/push closeout.

## Human smoke

Triangle:

```powershell
.\scripts\run-triangle.ps1
```

Expect a Win32 window titled **EO-Map Carbon triangle (TrinityAL DX11)** with a red triangle on a dark blue-grey background, until the window is closed.

A log line `first Present completed` means Present succeeded. It does not prove pixels.

Starfield:

```powershell
.\scripts\run-starfield.ps1
```

Milestone 1A is already human-verified. Re-run only if the host or Carbon changes. Expect thousands of white/grey stars, EO-Map-matching left-drag orbit, right-drag pan, and wheel zoom toward the current target.

Automated `--smoke` builds the full 25k starfield, presents 60 frames, and exits non-zero on setup/draw failure. It still cannot claim pixels.

New Eden:

```powershell
.\scripts\run-neweden.ps1
```

Milestone 1B geometry and Milestone 1C stargate topology are already human-verified. Re-run only if the host or Carbon changes. Expect a Win32 window titled **EO-Map Carbon New Eden Creator Mode (TrinityAL DX11)** with the locked human baseline, procedural sky, a broad New Eden-scale ISM disc, optional glow, faded gates, a brighter Jita-Amarr route overlay, and a tabbed Creator / Visual lab window. Same orbit / pan / zoom as the starfield. Automated `--smoke` loads the pinned Contract A systems, undirected gates, star temperatures, and region ids, reconstructs Jita-Amarr = 11 hops, checks persist parse and ISM envelope math, presents 40 bloom-on + 10 bloom-off creator + 10 creator-off frames (draw counts include the extra route pass), and still cannot claim pixels or aesthetics. The route overlay has not been separately human-verified.

```powershell
.\scripts\run-creator-mode.ps1
```
