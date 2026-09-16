# EO-Map Carbon

Personal technical exercise: can an external developer consume the public CCP/Fenris **Carbon / Trinity** stack and render a native 3D New Eden map with TrinityAL.

This is **not** a product, **not** a port of EO-Map, and **not** a modification of the EVE Online or EVE Frontier clients. Only public source, documentation, dependencies and data are used.

## What this became

It started as a bootstrap question. The public Carbon tree is real, but there is no hello-world app. This repo is the sequence of experiments that followed from that:

1. Native TrinityAL / Direct3D 11 host in a Win32 window.
2. A human-verified red triangle (`eo-map-carbon-triangle`).
3. A human-verified synthetic field of 25,000 points (`eo-map-carbon-starfield`), with EO-Map-matching orbit / pan / zoom.
4. A human-verified New Eden known-space point cloud of 5,485 systems (`eo-map-carbon-neweden`), from an export of EO-Map's pinned Contract A artefact.
5. A human-verified stargate graph on that same host: 6,989 unique undirected known-space connections.
6. Creator Mode visual work on the New Eden host: sized temperature-coloured stars, faded gates, HDR bloom, procedural sky, interstellar-medium disc, optional glow / flare, optional region tint, INI persist, and a developer Win32 tuning panel. **Aesthetics are not proven.** Automated smoke does not claim pixels.
7. A graph-derived Jita to Amarr shortest-hop route (11 hops), reconstructed with parent-pointer BFS on the loaded undirected graph, validated, and drawn as a brighter overlay on the current Creator Mode `GateLine` path.

The geometry question is answered. Creator Mode is a visual lab, not a finished look. The Jita-Amarr overlay is a routing experiment, not a map router.

New Eden coordinates and stargate pairs are a slim static export of EO-Map's pinned Contract A artefact. See [data/README.md](data/README.md). The native host does not open SQLite or call ESI.

## Other experiments in this repository

[TypeSafe Jev / System One](experiments/typesafe/README.md) work lives under `experiments/typesafe/`. It is **not** wired into Carbon. There are no NPCs in the native host. The harness was already in this repo, so the later synthetic commander benchmark was added next to it.

Three completed Jev experiments:

- Repository-comprehension benchmark against a Milestone 1C snapshot: [experiments/typesafe/REPORT.md](experiments/typesafe/REPORT.md)
- Jev-assisted Jita-Amarr coding-task preflight/postflight: [experiments/typesafe/ROUTING_EXPERIMENT.md](experiments/typesafe/ROUTING_EXPERIMENT.md)
- Synthetic tactical commander feasibility test: one boss, up to 60 deterministic subordinates, up to 12 players, 16 typed questions per inference, 220 live `jev-1.13.0` calls

Readable commander writeup:

- [experiments/typesafe/JEV_TACTICAL_COMMANDER.md](experiments/typesafe/JEV_TACTICAL_COMMANDER.md)

Full commander evidence (latency, sensitivity, stability, tokens):

- [experiments/typesafe/COMMANDER_BENCHMARK.md](experiments/typesafe/COMMANDER_BENCHMARK.md)

The commander architecture is:

`synthetic numeric battlefield state → TypeSafe System One API → typed tactical decisions → deterministic fallback/composer`

It is not `Carbon NPCs → Jev`.

## What this is not

- A map product. No labels, picking, search, sovereignty, ESI, SSO, networking, jump bridges, installers, or production packaging.
- A replacement of Trinity with SDL / OpenGL / raw DirectX / Three.js / a WebView.
- A fork of Carbon. Trinity is cloned **outside** this repository.
- EVE Frontier NPC AI, Fenris Feral AI, or Jev controlling anything inside the Carbon host.

The New Eden host has a developer-only Win32 Creator panel, INI persist, a static Jita-Amarr route overlay, and optional region tint. That is experiment UI, not product UI.

## Carbon architecture actually used

Public Carbon docs (`carbonengine/documentation`) still have an empty Getting Started page. From current source:

- Carbon components are built with **CMake presets + vcpkg**, using `carbonengine/vcpkg-registry` overlay triplets (`v141`, Windows SDK `10.0.17763.0`).
- A full Carbon **game** is a Python/Blue process hosted by `exefile`. That is **not** required for this experiment.
- **TrinityAL** (`trinityal/`, `TRINITY_AL_WITH_BLUE_EXPOSURE=0`) is a C++ layer that creates a D3D device from an `HWND` and draws primitives. The in-tree `TrinityALTest_dx11` already does this. The synthetic starfield uses `TOP_POINTS` (DX11 `POINTLIST`) plus a `Tr2ConstantBufferAL` for the view-projection matrix. `CreateDevice` does not create a depth buffer; that is a separate `Tr2TextureAL`.
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
.\scripts\build-neweden.ps1
.\scripts\run-neweden.ps1
.\scripts\run-neweden.ps1 -Smoke
.\scripts\run-visual-lab.ps1
.\scripts\run-creator-mode.ps1

# 7. Independent Jita-Amarr hop check (no Trinity)
python scripts\test-jita-amarr-route.py

# 8. Creator Mode numeric contracts (no GPU)
python scripts\test-visual-lab-math.py

# 9. TypeSafe harness unit tests (no API key required)
python -m unittest discover -s experiments\typesafe\tests -t experiments\typesafe
```

`CMakeUserPresets.json` in the Trinity checkout is local (gitignored by Trinity). This repo keeps the template at `cmake/trinity-CMakeUserPresets.json`.

Our CMake consumes Trinity's already-installed vcpkg prefix (`VCPKG_MANIFEST_MODE=OFF`) rather than installing a second copy.

## Human smoke test

Automated `--smoke` proves init, draw submission, Present, and a clean exit. It does **not** prove pixels. A human has to look at the window.

Milestone 0 diagnostic:

```powershell
.\scripts\run-triangle.ps1
```

Win32 window titled **EO-Map Carbon triangle (TrinityAL DX11)**, red triangle on a dark blue-grey background, until you close it.

Milestone 1A (human-verified geometry and camera):

```powershell
.\scripts\run-starfield.ps1
```

Win32 window titled **EO-Map Carbon starfield (TrinityAL DX11)**. Thousands of white/grey stars on a near-black background. Left-drag orbits the current target (same direction as EO-Map). Right-drag pans that target. Mouse wheel zooms toward it.

Milestone 1B / 1C geometry plus Creator Mode (geometry/topology human-verified; **look is not**):

```powershell
.\scripts\run-neweden.ps1
.\scripts\run-creator-mode.ps1
```

Win32 window titled **EO-Map Carbon New Eden Creator Mode (TrinityAL DX11)**. Temperature-coloured sized stars, faded gates, a brighter Jita-Amarr route overlay, procedural sky, restrained ISM, optional glow, optional bloom, and a **Creator / Visual lab** slider window. Same orbit / pan / zoom as the starfield.

A console/log line `first Present completed` means `Present` returned success. It does not by itself prove pixels.

## Current status

See [docs/status.md](docs/status.md).

## License

This repository's original files are for a personal experiment.

Trinity, TrinityAL tests, and the two HLSL files copied into `src/shaders/` are **MIT**, © CCP Games / CCP ehf. See those upstream trees. The MIT license does not grant CCP trademarks or EVE game content.
