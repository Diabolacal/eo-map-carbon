#pragma once

// EO-Map default universe-map colour / emissive, from
// eve-frontier-map/src/utils/starColors.ts (read 2026-09-13).
// Runtime presentation only. The binary stores raw kelvin + spectral letter.

#include <cmath>
#include <cstdint>

namespace starcolor
{
constexpr float kTempMinK = 2300.0f;
constexpr float kTempMaxK = 40000.0f;
constexpr float kEmissiveTempMaxK = 7496.0f;
constexpr float kEmissiveGamma = 3.0f;

struct Rgb
{
	float r;
	float g;
	float b;
};

inline Rgb HexRgb(uint32_t hex)
{
	return {
		float((hex >> 16) & 0xff) / 255.0f,
		float((hex >> 8) & 0xff) / 255.0f,
		float(hex & 0xff) / 255.0f,
	};
}

inline Rgb Lerp(Rgb a, Rgb b, float t)
{
	return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

// Harvard D65 stops, descending temperature. Same hex values as TEMPERATURE_STOPS.
inline Rgb TemperatureRgb(float temperatureK)
{
	float t = temperatureK;
	if (!std::isfinite(t))
	{
		t = kTempMinK;
	}
	if (t < kTempMinK)
	{
		t = kTempMinK;
	}
	if (t > kTempMaxK)
	{
		t = kTempMaxK;
	}

	static const float kStopsK[] = {
		40000.0f, 33000.0f, 21500.0f, 10000.0f, 8650.0f, 7300.0f, 6650.0f,
		6000.0f, 5650.0f, 5300.0f, 4600.0f, 3900.0f, 3100.0f, 2300.0f,
	};
	static const uint32_t kStopsHex[] = {
		0x9bb0ff, 0x9bb0ff, 0xa3b9ff, 0xaabfff, 0xbacfff, 0xcad7ff, 0xe4e7ff,
		0xf8f7ff, 0xfff6f3, 0xfff4ea, 0xffe3c6, 0xffd2a1, 0xffbf80, 0xffcc6f,
	};
	constexpr int kCount = 14;

	for (int i = 0; i < kCount - 1; ++i)
	{
		if (t >= kStopsK[i + 1])
		{
			const Rgb upper = HexRgb(kStopsHex[i]);
			const Rgb lower = HexRgb(kStopsHex[i + 1]);
			const float span = kStopsK[i] - kStopsK[i + 1];
			const float u = (span > 1.0e-6f) ? ((t - kStopsK[i + 1]) / span) : 0.0f;
			return Lerp(lower, upper, u);
		}
	}
	return HexRgb(kStopsHex[kCount - 1]);
}

inline float EmissiveForTemperature(float temperatureK)
{
	if (!std::isfinite(temperatureK) || temperatureK <= 0.0f)
	{
		return 1.0f;
	}
	const float logT = log10f(temperatureK);
	const float logMin = log10f(kTempMinK);
	const float logMax = log10f(kEmissiveTempMaxK);
	float u = (logT - logMin) / (logMax - logMin);
	if (u < 0.0f)
	{
		u = 0.0f;
	}
	if (u > 1.0f)
	{
		u = 1.0f;
	}
	return 1.0f + 5.0f * powf(u, kEmissiveGamma);
}

// EO-Map useStarfieldAndStargates.ts: (sin(id * 12.9898) * 43758.5453) % 37 < 1
inline float AnchorSize(uint32_t systemId)
{
	const float seed = sinf(float(systemId) * 12.9898f) * 43758.5453f;
	const float rem = fmodf(seed, 37.0f);
	return (rem < 1.0f) ? 1.6f : 1.0f;
}
}
