# Creator Mode port map

Durable map of EO-Map cinematic / Creator visuals onto this TrinityAL host.
Source of truth is current EO-Map and Carbon source, not prior summaries.

Aesthetics are **not** proven. Automated smoke cannot claim pixels.

## How to read this table

| Status | Meaning |
| --- | --- |
| implemented | Carbon has a working analogue |
| adapted | Same visual intent, different machinery |
| deferred | Understood, not built (or only a cheap hook) |
| not applicable | Product / Three.js / out of scope |

## Feature map

| EO-Map feature | Visual purpose | Carbon approach | Tuning | Status | Notable differences |
| --- | --- | --- | --- | --- | --- |
| Star size / brightness / sat | Core field readable and coloured | Existing instanced quads + `starSize` / `starBrightness` / `starSaturation` | Stars tab | implemented | Locked human baseline, not EO defaults |
| Depth brightness | Near/far not a flat plate | Existing `nearStarAtten` / `farStarAtten` | Stars tab | implemented | Same 3 / 275 view-Z window |
| Depth desaturation | Distant stars go cooler grey | `starDepthDesat` lerp toward luminance in `StarSprite.vsh` | Stars tab | adapted | EO default 0.25; we default 0.18 |
| Temperature colour | Natural stellar palette | Existing `NESTAR1` + Harvard D65 | — | implemented | Unchanged |
| Creator palettes / Data colouring | Themed or intel paint | Not a cinematic need | — | deferred | Keep temperature as the hero |
| Sprite glow / fringe | Soft halo on nearby stars | Second instanced pass `StarGlow.*`, additive, emissive threshold | Glow tab | adapted | EO shipped glow at 0; we default a modest 0.22 |
| Sprite flare | Horizontal streak on close bright stars | Third instanced pass `StarFlare.*`, off by default | Glow tab | adapted | Threshold + close-gate so dwarfs stay quiet |
| Hero corona | Selected-system billboard | Needs picking | — | deferred / not applicable | Explicit non-goal |
| Map / film bloom | Soft halo around HDR cores | Existing extract / blur H/V / composite | Post tab | implemented | Human baseline 0.06 / 1.03 / 1.60 |
| Exposure | Whole-frame brightness | Existing composite exposure | Post tab | implemented | Locked 2.09 |
| Neutral-ish tonemap | Keep star hue | Luminance Reinhard + hue-preserving peak clamp | — | implemented | Do not regress to per-channel Reinhard |
| Vignette | Corner falloff for screenshots | Radial multiply in composite | Post tab | adapted | Default 0.16; no CSS overlay |
| Saturation / contrast / black / gamma | Final grade | Composite after tonemap | Post tab | adapted | Identity except slight vignette |
| Bloom tint | Warm/cool bloom | RGB multiply on bloom before mix | Post tab + colour button | adapted | Default white |
| Grain / CA / dirt / DoF | Film finish | Not justified here | — | deferred | Keep the image clean |
| Layered nebula shells | Sky that is not flat black | `DeepSpace.psh`: 3 huge-radius FBM shells + view-dir hash stars | Background tab | adapted | One fullscreen pass, no Three meshes / baked 3D tex |
| Deep-space dome hash stars | Distant infinite dressing | Hash on view direction only | Background tab | adapted | Pan must not translate them |
| 320 geometric parallax points | Extra film dressing | — | — | deferred | Hash stars cover the need |
| Background galaxies | 8 far billboards | — | — | deferred | Avoid proprietary-looking stamps |
| Universe Effects / ISM raymarch | Dust dims what is behind it | Half-res 4/8-tap world slab, Beer–Lambert | Medium tab | adapted | Not EO's 48-step march or 96³ volumes |
| Dark lanes | Extinction threads | Extra optical depth from ridged FBM | Medium tab | adapted | Never writes black RGB |
| Density glow / “light lanes” | Unresolved starlight wash | Envelope glow + wisps + bloom-as-irradiance scatter | Medium tab | adapted | No star-density histogram |
| Diffuse gas / emission regions | Dropped EO layers | — | — | not applicable | Permanently off in EO |
| Gate opacity / distance fade | Web stays attached, recedes | Existing `TOP_LINES` + alpha-only fade | Gates tab | implemented | Locked 0.62 / 1.55; 1 px lines |
| Gate tint | Compose a coloured web | `gateTint` RGB, default 0.55 grey | Gates tab | adapted | Still unpremultiplied; fade stays in alpha |
| Gate selection gradient | Colour bleeds along selected gates | Needs selection | — | deferred | |
| Color By Region | Spatial colour on stars | `NEREGN1` sidecar + `region_id % 11` atlas | Colour tab | adapted | Off by default |
| Creator region highlight / dim / hide | “The Spire yellow” compose | Needs region selection UI | — | deferred | Product surface |
| Region labels / title | CSS2D overlay | — | — | not applicable | Labels are a non-goal |
| Transients / auto-tour / route pulse | Motion / product | — | — | not applicable | Still visual lab |

## Rendering order

```
HDR sceneRT (RGBA16F) + D24S8
  0. optional DeepSpace fullscreen (Z off)     // view-dir sky
  1. TOP_LINES gates
  2. instanced star quads
  3. optional instanced glow
  4. optional instanced flare
  unbind depth
  5–7. bloom extract / blur H / blur V @ 1/2   // if bloom on
  8. IsmField @ 1/2 (or clear to T=1)          // world slab
  9. composite to backbuffer
```

Default interactive draws / PP: **9 / 6** (sky + glow + ISM + bloom). Bloom off: **6 / 3**. All creator layers off: **3 / 1**.

## Persistence

File: `eo-map-carbon-neweden-tune.ini` next to the exe.

- Interactive startup loads it if present.
- `--smoke` never loads it.
- F8 / Save / Copy write the file and clipboard.
- F9 / Baseline restores `TuneDefaults()`.
- Unknown keys ignored. Non-finite values leave the previous field. Out-of-range values clamp.

Named preset in the file is `preset=Baseline`. That is `TuneDefaults()`, not a preset manager.

## Procedural notes

- Sky stars: `valueNoise(rd * 280)` and `rd * 520`. No `cameraPos` in the hash.
- Sky nebula: `p = cameraPos + rd * R` at EO-like shell radii 2833 / 4959 / 7084.
- ISM: analytic three-lobe disc around `kCentre`, 4 or 8 taps, half-res RGBA16F (`rgb` emission, `a` transmittance).
- Star illumination of dust: one sample of the bloom buffer (`ismScatter`). Not N point lights.
- Region ISM mix: world XZ atlas tint, not a true region volume.

## Carbon APIs used (verified)

Already proven: `Tr2TextureAL` RT|SRV, `SetRenderTarget`, `Clear`, `Tr2ConstantBufferAL`, `SetConstants`, `Tr2ResourceSetAL`, `DrawPrimitive`, `DrawIndexedInstanced`, `PIXEL_FORMAT_R16G16B16A16_FLOAT`.

Creator Mode adds no new Trinity classes. Extra passes reuse `Fullscreen.vsh` + `DrawPrimitive(0, 2)` and a second/third instanced quad program.
