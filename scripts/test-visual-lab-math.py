#!/usr/bin/env python3
"""Numeric contracts for the visual-lab gate and star colour path.

Mirrors src/visual_lab_math.h / src/star_color.h so a regression can fail
without a GPU or a human looking at pixels. Aesthetic judgement stays human.
"""

from __future__ import annotations

import math
import sys

GATE_GREY = 0.55
CLEAR = (2.0 / 255.0, 2.0 / 255.0, 8.0 / 255.0)
TONE_SHOULDER = 0.35
STAR_SAT = 1.70
GATE_OPACITY = 0.38
GATE_DIST_ATTEN = 1.00
EXPOSURE = 1.10
TEMP_MIN = 2300.0
TEMP_MAX = 40000.0
EMISSIVE_MAX = 7496.0
EMISSIVE_GAMMA = 3.0

STOPS_K = [
    40000.0, 33000.0, 21500.0, 10000.0, 8650.0, 7300.0, 6650.0,
    6000.0, 5650.0, 5300.0, 4600.0, 3900.0, 3100.0, 2300.0,
]
STOPS_HEX = [
    0x9BB0FF, 0x9BB0FF, 0xA3B9FF, 0xAABFFF, 0xBACFFF, 0xCAD7FF, 0xE4E7FF,
    0xF8F7FF, 0xFFF6F3, 0xFFF4EA, 0xFFE3C6, 0xFFD2A1, 0xFFBF80, 0xFFCC6F,
]


def hex_rgb(hex_value: int) -> tuple[float, float, float]:
    return (
        ((hex_value >> 16) & 0xFF) / 255.0,
        ((hex_value >> 8) & 0xFF) / 255.0,
        (hex_value & 0xFF) / 255.0,
    )


def lerp(a: tuple[float, float, float], b: tuple[float, float, float], t: float):
    return (
        a[0] + (b[0] - a[0]) * t,
        a[1] + (b[1] - a[1]) * t,
        a[2] + (b[2] - a[2]) * t,
    )


def temperature_rgb(temperature_k: float) -> tuple[float, float, float]:
    t = temperature_k
    if not math.isfinite(t):
        t = TEMP_MIN
    t = min(max(t, TEMP_MIN), TEMP_MAX)
    for i in range(len(STOPS_K) - 1):
        if t >= STOPS_K[i + 1]:
            span = STOPS_K[i] - STOPS_K[i + 1]
            u = ((t - STOPS_K[i + 1]) / span) if span > 1.0e-6 else 0.0
            return lerp(hex_rgb(STOPS_HEX[i + 1]), hex_rgb(STOPS_HEX[i]), u)
    return hex_rgb(STOPS_HEX[-1])


def emissive(temperature_k: float) -> float:
    if not math.isfinite(temperature_k) or temperature_k <= 0.0:
        return 1.0
    u = (math.log10(temperature_k) - math.log10(TEMP_MIN)) / (
        math.log10(EMISSIVE_MAX) - math.log10(TEMP_MIN)
    )
    u = min(max(u, 0.0), 1.0)
    return 1.0 + 5.0 * (u ** EMISSIVE_GAMMA)


def lum(c: tuple[float, float, float]) -> float:
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def chroma(c: tuple[float, float, float]) -> float:
    return max(c) - min(c)


def boost(c: tuple[float, float, float], sat: float) -> tuple[float, float, float]:
    y = lum(c)
    return (
        min(max(y + (c[0] - y) * sat, 0.0), 1.0),
        min(max(y + (c[1] - y) * sat, 0.0), 1.0),
        min(max(y + (c[2] - y) * sat, 0.0), 1.0),
    )


def core_gain(e: float) -> float:
    return 0.82 + 0.18 * e


def tonemap(hdr: tuple[float, float, float], exposure: float) -> tuple[float, float, float]:
    e = (hdr[0] * exposure, hdr[1] * exposure, hdr[2] * exposure)
    y = lum(e)
    if y <= 1.0e-5:
        mapped = e
    else:
        scale = (y / (1.0 + y * TONE_SHOULDER)) / y
        mapped = (e[0] * scale, e[1] * scale, e[2] * scale)
    peak = max(mapped)
    if peak > 1.0:
        mapped = (mapped[0] / peak, mapped[1] / peak, mapped[2] / peak)
    return mapped


def gate_fragment(t: float):
    gain = 1.0 - 0.65 * GATE_DIST_ATTEN * min(max(t, 0.0), 1.0)
    alpha = min(max(GATE_OPACITY * gain, 0.0), 1.0)
    rgb = (GATE_GREY, GATE_GREY, GATE_GREY)
    blended = (
        rgb[0] * alpha + CLEAR[0] * (1.0 - alpha),
        rgb[1] * alpha + CLEAR[1] * (1.0 - alpha),
        rgb[2] * alpha + CLEAR[2] * (1.0 - alpha),
    )
    return rgb, alpha, blended


def check(cond: bool, message: str) -> None:
    if not cond:
        raise SystemExit(f"FAILED visual lab math: {message}")


def main() -> int:
    near_rgb, near_a, near_b = gate_fragment(0.0)
    far_rgb, far_a, far_b = gate_fragment(1.0)
    check(near_rgb[0] >= 0.40 and near_rgb[0] == near_rgb[1] == near_rgb[2], "gate RGB is not constant light grey")
    check(min(near_b) >= 0.15, "near gate blended contribution is too dark")
    check(lum(far_b) < lum(near_b) - 1.0e-4, "gate distance attenuation does not reduce contribution")
    check(far_rgb[0] + 1.0e-5 >= near_rgb[0], "gate attenuation darkened RGB instead of alpha")
    check(far_a < near_a, "gate attenuation did not reduce alpha")

    warm_src = temperature_rgb(2300.0)
    cool_src = temperature_rgb(7305.0)
    warm = boost(warm_src, STAR_SAT)
    mid = boost(temperature_rgb(4436.0), STAR_SAT)
    cool = boost(cool_src, STAR_SAT)
    white = boost(temperature_rgb(6000.0), STAR_SAT)

    check(chroma(warm) > chroma(warm_src) + 0.04, "2300 K chroma did not increase")
    check(chroma(cool) > chroma(cool_src) + 0.02, "7305 K chroma did not increase")
    check(warm[0] > warm[2] + 0.20, "2300 K is not materially warmer than blue")
    check(cool[2] > cool[0] + 0.08, "7305 K is not materially cooler than red")
    check(chroma(white) < 0.18, "6000 K should stay near-white")

    warm_map = tonemap(tuple(c * core_gain(emissive(2300.0)) for c in warm), EXPOSURE)
    cool_map = tonemap(tuple(c * core_gain(emissive(7305.0)) for c in cool), EXPOSURE)
    check(not all(c > 0.97 for c in warm_map), "tonemap collapsed 2300 K to white")
    check(not all(c > 0.97 for c in cool_map), "tonemap collapsed 7305 K to white")
    check(abs(warm_map[0] - cool_map[0]) >= 0.08 or abs(warm_map[2] - cool_map[2]) >= 0.08,
          "tonemap made warm and cool stars the same colour")
    check(chroma(warm_map) >= 0.12 and chroma(cool_map) >= 0.08, "tonemap removed usable chroma")

    print(
        "visual math: gate near rgb={:.3f} a={:.3f} blended={:.3f} far blended={:.3f} | "
        "raw chroma 2300={:.3f} 7305={:.3f} | present 2300={:.3f},{:.3f},{:.3f} "
        "4436={:.3f},{:.3f},{:.3f} 7305={:.3f},{:.3f},{:.3f} 6000={:.3f},{:.3f},{:.3f} | "
        "mapped 2300={:.3f},{:.3f},{:.3f} 7305={:.3f},{:.3f},{:.3f}".format(
            near_rgb[0], near_a, lum(near_b), lum(far_b),
            chroma(warm_src), chroma(cool_src),
            *warm, *mid, *cool, *white,
            *warm_map, *cool_map,
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
