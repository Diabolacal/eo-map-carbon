# Agent guidance

This is a personal experiment: can an external developer consume public Carbon/Trinity and render with TrinityAL. It is not a product, not an EO-Map port, and not a client modification.

## Non-goals

Do not add ESI, SSO, labels, picking, routing, jump bridges, security colours, regions, sovereignty, UI panels, search, persistence, installers, or production packaging unless a later milestone explicitly asks for that one thing.

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

`eo-map-carbon-neweden` loads 5,485 known-space systems from `data/new_eden_systems.bin`, an export of EO-Map's pinned Contract A artefact (`map_data_eo_3464040.db`, SDE 3464040, builder 1.5.0). The host applies EO-Map's live display mapping `scene = (db.x, -db.z, -db.y)` and draws them with the same TrinityAL `TOP_POINTS` path as 1A. A human confirmed the cluster is recognisably New Eden, with correct-enough orientation and the same orbit / pan / zoom as 1A.

Do not re-interpret the SDE. Regenerate both artefacts with `scripts/export-new-eden-systems.py` from the sibling EO-Map checkout. Do not open SQLite, ESI, or the EO-Map web app from this executable.

W-space (2,604 Anoikis systems in Contract A) is a separate ~1,300 LY cluster and is omitted from this first visual.

## Proven New Eden stargate graph (Milestone 1C)

Automated smoke is in place. Pixels still need a human look.

`eo-map-carbon-neweden` also loads `data/new_eden_stargates.bin`: 6,989 unique undirected known-space connections from the same Contract A `stargates` table (13,978 directed rows, already k-space only). The host draws them as one static `TOP_LINES` buffer (`DrawPrimitive(0, 6989)` — count is the number of segments). Systems stay on the unchanged 1B `TOP_POINTS` path. Two draw calls per frame. Same orbit / pan / zoom.

Do not re-interpret the SDE. Regenerate both artefacts with `scripts/export-new-eden-systems.py`. Do not add W-space, wormholes, jump bridges, security colours, or route highlighting here.

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
- `eo-map-carbon-neweden` — Milestone 1B point cloud plus Milestone 1C static stargate graph.

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
- Full Trinity's sized-sprite path is instanced triangles (`EveSpriteSet` / `Tr2QuadRenderer`), not a TrinityAL primitive. Use that later if stars need size. Do not invent point-sprite state.

## Milestone discipline

One milestone at a time. Do not implement later map features while proving a renderer step.

Record newly discovered Carbon behaviour in `docs/status.md` and, if it will still be true next month, here.

Automated smoke can prove init, draw, Present, and a clean exit. It cannot claim pixels. A human has to look at the window.

## Git hygiene

Work on a feature branch. Do not modify `main` directly unless asked.

Do not commit build trees, binaries, logs, `.env`, credentials, proprietary assets, or cloned Carbon sources.

Do not commit unless asked, except when the task explicitly includes a branch/commit/push closeout.

Do not merge to `main` unless asked.

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

Milestone 1B geometry is already human-verified. Milestone 1C adds the real stargate network on the same host. Expect a Win32 window titled **EO-Map Carbon New Eden (TrinityAL DX11)** with the recognisable New Eden cluster plus a subdued grey network of straight 3D gate lines that stay attached while orbiting. Same orbit / pan / zoom as the starfield. Automated `--smoke` loads the pinned Contract A systems and undirected gates, checks 5485 systems, 6989 connections, hub adjacency, Jita→Amarr = 11 hops, presents 60 frames, and still cannot claim pixels.
