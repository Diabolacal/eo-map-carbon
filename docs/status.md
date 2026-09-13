# Current status

Last updated 2026-09-13 after a camera-control acceptance pass.

## Outcome

**PROVEN PENDING HUMAN VISUAL**

Milestone 0 is human-verified. Milestone 1A rendering (25k `TOP_POINTS` stars, depth, one draw call, ~240 FPS interactive) is human-verified. A follow-up camera pass flipped orbit to match EO-Map's three.js `OrbitControls` and added right-drag pan of an explicit orbit target. Automated smoke still passes. The revised controls have not been human-checked.

## Milestone 0 — triangle

**PROVEN**, including human pixel verification.

The frozen diagnostic target is unchanged: `eo-map-carbon-triangle`, `src/triangle_main.cpp`, the two original shaders, `.\scripts\run-triangle.ps1`.

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `cmake --preset triangle-debug` |
| B. compile | **pass** | `ninja: no work to do` after the CMake share-helper refactor (triangle sources untouched) |
| C. link | **pass** | existing `eo-map-carbon-triangle.exe` |
| D–F. smoke | **pass** | `--smoke` exit 0; `CreateDevice succeeded`; `first Present completed`; 30 frames |
| G. pixels | **pass (human)** | red triangle on dark blue-grey, previously confirmed |

Upstream `Rendering.CanRenderASingleTriangle` still **PASSED** (3 ms) after this work.

## Milestone 1A — synthetic starfield

**Rendering: PROVEN (human).** Camera revision: **pending human smoke.**

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `.\scripts\build-starfield.ps1` / `cmake --preset triangle-debug` |
| B. compile | **pass** | `starfield_main.cpp.obj`; C5030 from v143 ATL on v141 is a warning |
| C. link | **pass** | `.cmake-build-triangle-debug\bin\eo-map-carbon-starfield.exe` |
| D. launch | **pass** | `--smoke` created `HWND`, process exit 0 |
| E. renderer init | **pass** | `adapter count: 3` then `TrinityAL CreateDevice succeeded` |
| F. starfield construct | **pass** | `star count: 25000`; vertex buffer create succeeded (otherwise smoke exits 1) |
| G. present | **pass** | `first Present completed`; 60-frame smoke exit 0 |
| H. pixels / depth / 1 draw | **pass (human)** | ~25k stars visible, clearly 3D, ~240 FPS / ~4.16 ms, 1 draw/frame, clean close |
| I. revised orbit / pan | **unverified** | orbit sign now matches EO-Map `OrbitControls`; right-drag pans the target |

### Starfield smoke log

File: `.cmake-build-triangle-debug\bin\eo-map-carbon-starfield-smoke.log` (gitignored)

```
eo-map-carbon-starfield starting
renderer: TrinityAL DX11
path: TOP_POINTS DrawPrimitive (one call)
star count: 25000
draw calls per frame: 1
adapter count: 3
window hwnd=0000000000700AA2 1280x720
TrinityAL CreateDevice succeeded
Rendering 25000 synthetic stars via TOP_POINTS. Left-drag orbits, wheel zooms. Close the window to exit.
first Present completed
camera eye=70.4,53.8,114.8 distance=145.0
smoke test reached 60 frames, exiting
smoke: frames=60 stars=25000 draw_calls/frame=1 avg_frame_ms=0.27 avg_fps=3762.4 path=TrinityAL_DX11/TOP_POINTS
exiting after 60 frames
```

Process exit code: 0.

The 0.27 ms / 3762 fps figure is QPC around BeginScene through Present with `PRESENT_INTERVAL_IMMEDIATE` in `--smoke` only. It is not a vsync-capped interactive measurement. Interactive mode uses `PRESENT_INTERVAL_ONE`.

## Architecture actually used (1A)

- Same Win32 + TrinityAL DX11 bootstrap as the triangle.
- Stars: 25,000 deterministic `float3` + intensity vertices in one immutable `Tr2BufferAL`.
- Draw: `SetTopology(TOP_POINTS)` then `DrawPrimitive(0, 25000)`. One call per frame.
- Camera: host orbit around an explicit target (RH look-at + perspective) written into `Tr2ConstantBufferAL` and bound with `SetConstants(..., VERTEX_SHADER, 0)`.
- Depth: `Tr2TextureAL` `PIXEL_FORMAT_D24_UNORM_S8_UINT` + `SetDepthStencil`. `CreateDevice` does not create a depth surface.
- Resize: `SetPresentParameters` (same as `SwapChainResizing` tests) then recreate the depth texture. Viewport is reset inside TrinityAL `CreateBackBuffers`.
- Input: raw Win32. Left-drag orbit signs match EO-Map's three.js `OrbitControls` (`yaw -= dx`, `pitch += dy`). Right-drag pans the target in screen space. Wheel zooms toward the target.

`TOP_POINTS` is a real TrinityAL topology (DX11 `POINTLIST`). `DrawPrimitive` primitive count is the point count. `RS_POINTSIZE` / point sprites are stored and ignored on DX11, so stars are hardware 1-pixel points. That is acceptable for this smoke. Full Trinity's later many-object path for sized sprites is instanced triangles (`EveSpriteSet` / `Tr2QuadRenderer`); that is not TrinityAL-only and was not used here.

## Known gaps

- Revised orbit direction and right-drag pan have not been human-checked yet. Rendering already was.
- Point size is not controllable through TrinityAL on DX11. Later New Eden stars that need size should move to the verified instanced-triangle path, not `RS_POINTSIZE`.
- Metal marks `TOP_POINTS` `validType=false`. This host is DX11-only.
- No TrinityAL test draws `TOP_POINTS`. Behaviour is taken from the enum, the DX11 topology table, `ComputeVertexCount`, and Trinity's debug point-cloud submit.
- Resize is implemented from verified APIs but was not interactively exercised in smoke.
- Debug CRT is `/MD`. Global git `insteadOf` is still mutated by configure. Paths still assume `C:\dev\eo-map-carbon` and `C:\dev\carbon-upstream\trinity`.
- No New Eden / map functionality (intentionally out of scope).

## Human smoke required

Primary command:

```powershell
cd C:\dev\eo-map-carbon
.\scripts\run-starfield.ps1
```

Expect the same proven starfield. Check the camera pass: left-drag orbit should match EO-Map, right-drag should pan the target, further left-drag should orbit the new target, wheel zoom should still work.

Triangle diagnostic (still valid):

```powershell
.\scripts\run-triangle.ps1
```
