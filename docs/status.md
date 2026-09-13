# Current status

Last updated 2026-09-13 after a real configure/build/run on this machine.

## Outcome

**PARTIALLY PROVEN**

Public TrinityAL DX11 can be configured, compiled, linked, launched, and driven through `CreateDevice` + `DrawPrimitive` + `Present` from an external C++ host. Pixel contents of the swap chain have not been captured; a human still has to look at the window.

This is not a stub. The executable links `TrinityAL_dx11_debug.lib` and calls TrinityAL APIs from `trinityal/tests`.

## Evidence log

| Gate | Result | Evidence |
| --- | --- | --- |
| A. configure | **pass** | `cmake --preset local-dx11-debug` wrote `C:\dev\carbon-upstream\trinity\.cmake-build-local-dx11-debug` (Ninja, MSVC 19.16.27054, `BUILD_DX11=ON`, `WITH_GRANNY=OFF`). Our host: `cmake --preset triangle-debug` wrote `.cmake-build-triangle-debug`. |
| B. compile | **pass** | `TrinityAL_dx11` compiled (~131 objs). `eo-map-carbon-triangle` compiled (`triangle_main.cpp.obj`). C5030 from v143 ATL on v141 is a warning, not an error. |
| C. link | **pass** | `TrinityAL_dx11_debug.lib`, `TrinityALTest_dx11_debug.exe`, `.cmake-build-triangle-debug\bin\eo-map-carbon-triangle.exe` (1223680 bytes after the smoke-log rebuild). |
| D. launch | **pass** | `eo-map-carbon-triangle.exe --smoke` started, created `HWND 0000000000480DBA`, process exit 0. |
| E. renderer init | **pass** | Log: `adapter count: 3` then `TrinityAL CreateDevice succeeded`. Upstream `RenderContextCreation.CanCreateRenderContext` **PASSED** (349 ms). |
| F. visible rendering | **API pass / pixels unverified** | Log: `first Present completed` then `smoke test reached 30 frames, exiting`. Upstream `Rendering.CanRenderASingleTriangle` **PASSED** (3 ms). No backbuffer capture in this host. Human must confirm a red triangle. |

### TrinityALTest (from the TrinityALTest output directory)

```
TrinityALTest_dx11_debug.exe --gtest_filter=RenderContextCreation.CanCreateRenderContext
[  PASSED  ] 1 test.   (349 ms)

TrinityALTest_dx11_debug.exe --gtest_filter=Rendering.CanRenderASingleTriangle
[  PASSED  ] 1 test.   (3 ms)
```

### Our host smoke log

File: `.cmake-build-triangle-debug\bin\eo-map-carbon-triangle-smoke.log` (gitignored)

```
eo-map-carbon-triangle starting
renderer: TrinityAL DX11
adapter count: 3
window hwnd=0000000000480DBA 1280x720
TrinityAL CreateDevice succeeded
Rendering a red triangle. Close the window to exit.
first Present completed
smoke test reached 30 frames, exiting
exiting after 30 frames
```

Process exit code: 0.

## Dependency / build approach

- Clone `https://github.com/carbonengine/trinity` to `C:\dev\carbon-upstream\trinity` (`--recurse-submodules`). Not a submodule of this repo.
- Trinity vcpkg (registry + vendored vcpkg submodule) installs public packages into the Trinity build dir.
- Overlay `overlays/vcpkg/fxc` replaces the CCP prebuilt-SDK download of `fxc`.
- Our CMake uses Trinity's vcpkg toolchain with `VCPKG_MANIFEST_MODE=OFF` and `VCPKG_INSTALLED_DIR=<trinity>/.cmake-build-local-dx11-debug/vcpkg_installed`.
- Application language: native C++. Blue/Python/exefile not used.
- Renderer path: TrinityAL DX11 (`Tr2PrimaryRenderContextAL::CreateDevice`, `BeginScene` / `Clear` / `DrawPrimitive` / `EndScene` / `Present`).

## Remaining public-build friction (workarounds exist)

These did **not** stop the bootstrap on this machine, but they are still real external-developer costs:

1. **fxc CDN** `vcpkg-prebuilt-sdks.ccpgames.com` does not resolve. Overlay copies Windows SDK `fxc.exe`. Smallest upstream change: host FXC from a public Microsoft source or document the SDK copy.
2. **SSH git URLs** in public Carbon portfiles. Workaround: git `insteadOf` HTTPS. Smallest upstream change: use `https://github.com/` in portfiles.
3. **v141 ATL missing** from VS 2022 Build Tools. Workaround: junction v143 ATL + `CMAKE_COMPILE_WARNING_AS_ERROR=OFF`. Smallest upstream change: stop requiring ATL for CcpCore, or document a supported ATL install.
4. **Windows SDK 10.0.17763.0** is pinned. A modern box may not have it until the standalone SDK is installed.
5. **Granny** remains internal (same dead CDN). Not needed for this triangle.
6. **`carbon-trinity` vcpkg port** is stale (4.0.2 / SSH vs GitHub v6.0.0). Do not consume Trinity through that port today.

## Known gaps

- No GPU readback / screenshot in our host, so gate F is not pixel-proven.
- Debug CRT is `/MD` (`_ITERATOR_DEBUG_LEVEL=0`). Mixing with `/MDd` consumers will LNK2038.
- Global git `insteadOf` is mutated by the configure script.
- Hardcoded paths assume `C:\dev\eo-map-carbon` and `C:\dev\carbon-upstream\trinity`.
- Trinity user preset disables warnings-as-errors because of the ATL junction.
- Streamline/DLSS/Aftermath are pulled because they are PUBLIC deps of `TrinityAL_dx11`; this triangle does not use them.
- No New Eden / map functionality (intentionally out of scope).

## Human smoke required

```powershell
cd C:\dev\eo-map-carbon
.\scripts\run-triangle.ps1
```

Look for: window title **EO-Map Carbon triangle (TrinityAL DX11)**, **red triangle**, dark blue-grey clear colour, stays until closed.
