# EO-Map Carbon Experiment

Personal technical exercise: can an external developer bootstrap the public CCP/Fenris **Carbon / Trinity** stack, with LLM coding agents doing the implementation, and render something native.

This is **not** a product, **not** a port of EO-Map, and **not** a modification of the EVE Online or EVE Frontier clients.

Only public source, documentation, dependencies and data are used.

## What this is

A Windows-native experiment that:

1. Consumes public Carbon repositories (`carbonengine/trinity` and the packages its vcpkg manifest pulls).
2. Builds Trinity's C++ abstraction layer **TrinityAL** with the **Direct3D 11** backend.
3. Opens a native Win32 window.
4. Draws a human-verified red triangle (`eo-map-carbon-triangle`) using TrinityAL APIs from `trinityal/tests`.
5. Draws a human-verified synthetic 3D starfield of 25,000 points (`eo-map-carbon-starfield`) through the same TrinityAL DX11 path.

The visual sophistication is still low. The question is whether Carbon/Trinity can be consumed, then whether many points can be drawn in 3D without leaving TrinityAL.

## What this is not

- New Eden, star systems, EO-Map data, routing, labels, sovereignty, ESI, SSO, networking, UI panels, installers, auto-update, or production packaging.
- A replacement of Trinity with SDL / OpenGL / raw DirectX / Three.js.
- A fork of Carbon. Trinity is cloned **outside** this repository.

New Eden data, picking, labels, and routing are still later. They are not implemented here.

## Carbon architecture actually used

Public Carbon docs (`carbonengine/documentation`) still have an empty Getting Started page. From current source:

- Carbon components are built with **CMake presets + vcpkg**, using `carbonengine/vcpkg-registry` overlay triplets (`v141`, Windows SDK `10.0.17763.0`).
- A full Carbon **game** is a Python/Blue process hosted by `exefile`. That is **not** required for this experiment.
- **TrinityAL** (`trinityal/`, `TRINITY_AL_WITH_BLUE_EXPOSURE=0`) is a C++ layer that creates a D3D device from an `HWND` and draws primitives. The in-tree `TrinityALTest_dx11` already does this. The starfield uses `TOP_POINTS` (DX11 `POINTLIST`) plus a `Tr2ConstantBufferAL` for the view-projection matrix. `CreateDevice` does not create a depth buffer; that is a separate `Tr2TextureAL`.
- Renderer backends default to **OFF**. DX11 is enabled explicitly (`-DBUILD_DX11=ON`).
- Granny (`WITH_GRANNY`) is internal-only. It stays **OFF**.
- There is no public hello-world app. The bootstrap is the TrinityAL tests plus this tiny host.

Upstream checkout (not a git submodule of this repo):

```
C:\dev\carbon-upstream\trinity    https://github.com/carbonengine/trinity
```

Proven against `carbonengine/trinity` `f26b1f2bfdf37e63f85a209b6d878f6e8b02a67d` (`v6.0.0-2-gf26b1f2b`).

## Public-build issues this experiment has to handle

| Issue | Source | What we do |
| --- | --- | --- |
| `fxc` port downloads `vcpkg-prebuilt-sdks.ccpgames.com`, which does not resolve | `vcpkg-registry/ports/fxc/portfile.cmake` | Overlay port `overlays/vcpkg/fxc` copies `fxc.exe` from a local Windows SDK |
| Many Carbon portfiles still use `git@github.com` | registry portfiles | `scripts\configure-trinity.ps1` sets `url.https://github.com/.insteadOf git@github.com:` |
| Windows triplets pin **v141** and SDK **10.0.17763.0** | `x64-windows-carbon.cmake` | VS 2022 Build Tools + MSVC v141; Windows SDK 10.0.17763.0 |
| Visual Studio generator + v141 probes Windows SDK 8.1 | CMake/VS | Use **Ninja** + `vcvars64.bat -vcvars_ver=14.16` |
| `carbon-trinity` vcpkg port is **4.0.2** / SSH while GitHub Trinity is **v6.0.0** | registry vs trinity tags | Build Trinity from the public git checkout, not the stale port |
| Granny CDN is the same dead host | `WITH_GRANNY` | Leave OFF |
| `atlbase.h` is missing from the v141 Build Tools catalog | CcpCore / TrinityAL | Junction v143 ATL into the v141 tree (see below) |
| v143 ATL headers emit C5030 `msvc::nocastguard` on the v141 compiler | `atlcomcli.h` | `CMAKE_COMPILE_WARNING_AS_ERROR=OFF` in the Trinity user preset |
| NVIDIA Aftermath URL | `ports/nvidia-aftermath` | Public NVIDIA download. Not CCP-internal. |
| Carbon Debug is `/MD`, not `/MDd` | `x64-windows-carbon` toolchain | Our host uses `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` |

### ATL junction (machine-local, not in this repo)

VS 2022 Build Tools does not offer `Microsoft.VisualStudio.Component.VC.v141.ATL`. CcpCore still includes `atlbase.h`. On this machine the workaround is an elevated junction from the v143 ATL tree into v141:

```bat
mklink /J "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.16.27023\atlmfc" "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\atlmfc"
```

That is a local Visual Studio layout change, not a Carbon fork. It is required before `carbon-core` will compile.

## Clean-build prerequisites (Windows)

- Git
- CMake 3.31 or newer (this machine: 4.1.1)
- Visual Studio 2022 Build Tools with **MSVC v141 (VS 2017 C++ toolset)** plus a later MSVC ATL tree for the junction above
- Windows SDK **10.0.17763.0** (Carbon's toolchain asks for it) and a later SDK that contains `fxc.exe` if 17763 does not
- Python 3 (shader `bin2h` and some Carbon tools)
- Disk: vcpkg spends tens of GB under `C:\buildtrees` (short path; Carbon's own recommendation)
- GitHub HTTPS. `scripts\configure-trinity.ps1` writes a **global** git `insteadOf` rewrite because many Carbon vcpkg portfiles still use SSH URLs for public repos
- Ninja (the script uses the copy vcpkg already downloaded)
- Do **not** enable Granny

## Exact commands

From a Developer PowerShell (x64), in this repository:

```powershell
# 1. Clone Trinity outside this repo (once)
git clone --recurse-submodules https://github.com/carbonengine/trinity.git C:\dev\carbon-upstream\trinity

# 2. One-time ATL junction (elevated Command Prompt; see above)

# 3. Configure Trinity (copies cmake/trinity-CMakeUserPresets.json into the checkout)
.\scripts\configure-trinity.ps1

# 4. Build TrinityAL DX11 + in-tree tests
.\scripts\build-trinity.ps1

# 5. Upstream triangle test (gtest; no interactive window by default)
& C:\dev\carbon-upstream\trinity\.cmake-build-local-dx11-debug\carbon\autobuild\TrinityALTest\Windows\x64\v141\TrinityALTest_dx11_debug.exe --gtest_filter=Rendering.CanRenderASingleTriangle

# 6. Build and run the hosts
.\scripts\build-triangle.ps1
.\scripts\run-triangle.ps1
.\scripts\run-triangle.ps1 -Smoke
.\scripts\build-starfield.ps1
.\scripts\run-starfield.ps1
.\scripts\run-starfield.ps1 -Smoke
```

`CMakeUserPresets.json` in the Trinity checkout is local (gitignored by Trinity). This repo keeps the template at `cmake/trinity-CMakeUserPresets.json`.

Our triangle CMake consumes Trinity's already-installed vcpkg prefix (`VCPKG_MANIFEST_MODE=OFF`) rather than installing a second copy.

## Human smoke test

Milestone 0 diagnostic:

```powershell
.\scripts\run-triangle.ps1
```

Win32 window titled **EO-Map Carbon triangle (TrinityAL DX11)**, red triangle on a dark blue-grey background, until you close it.

Milestone 1A (primary):

```powershell
.\scripts\run-starfield.ps1
```

Win32 window titled **EO-Map Carbon starfield (TrinityAL DX11)**. Thousands of white/grey stars on a near-black background. Left-drag orbits the current target (same direction as EO-Map). Right-drag pans that target. Mouse wheel zooms toward it. Close the window to exit.

A console/log line `first Present completed` means `Present` returned success. It does not by itself prove pixels.

## Current status

See [docs/status.md](docs/status.md).

## License

This repository's original files are for a personal experiment.

Trinity, TrinityAL tests, and the two HLSL files copied into `src/shaders/` are **MIT**, © CCP Games / CCP ehf. See those upstream trees. The MIT license does not grant CCP trademarks or EVE game content.
