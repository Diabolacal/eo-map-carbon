#pragma once

// CPU mirror of the visual-lab gate / chroma / tonemap contracts.
// The HLSL copies of these formulas must stay in lockstep.
// Used by smoke assertions so we can fail without looking at pixels.

#include "star_color.h"
#include "tune_params.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace vislab
{
// 1C wrote RGB 0.22, alpha 1, no blend, onto an LDR backbuffer.
// That is the intended *displayed* grey. This host now blends then tonemaps,
// so the shader keeps a constant unpremultiplied light grey and puts
// visibility in alpha only. 0.55 * baseline opacity 0.62 ≈ 0.34 blended.
constexpr float kGateUnpremulGrey = 0.55f;
constexpr float kSceneClearR = 2.0f / 255.0f;
constexpr float kSceneClearG = 2.0f / 255.0f;
constexpr float kSceneClearB = 8.0f / 255.0f;
constexpr float kTonemapShoulder = 0.35f;

inline float GateGain(float distanceAtten, float t)
{
	const float tt = TuneClamp(t, 0.0f, 1.0f);
	return 1.0f - 0.65f * distanceAtten * tt;
}

inline float GateAlpha(float opacity, float distanceAtten, float t)
{
	return TuneClamp(opacity * GateGain(distanceAtten, t), 0.0f, 1.0f);
}

struct GateFragment
{
	starcolor::Rgb rgb;
	float alpha;
	starcolor::Rgb blended; // SRCALPHA / INVSRCALPHA over the near-black clear
};

inline GateFragment EvaluateGate(float opacity, float distanceAtten, float t)
{
	GateFragment frag;
	frag.rgb = { kGateUnpremulGrey, kGateUnpremulGrey, kGateUnpremulGrey };
	frag.alpha = GateAlpha(opacity, distanceAtten, t);
	frag.blended.r = frag.rgb.r * frag.alpha + kSceneClearR * (1.0f - frag.alpha);
	frag.blended.g = frag.rgb.g * frag.alpha + kSceneClearG * (1.0f - frag.alpha);
	frag.blended.b = frag.rgb.b * frag.alpha + kSceneClearB * (1.0f - frag.alpha);
	return frag;
}

inline float Luminance(starcolor::Rgb c)
{
	return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
}

inline float Chroma(starcolor::Rgb c)
{
	const float mx = (std::max)(c.r, (std::max)(c.g, c.b));
	const float mn = (std::min)(c.r, (std::min)(c.g, c.b));
	return mx - mn;
}

inline starcolor::Rgb BoostChroma(starcolor::Rgb rgb, float saturation)
{
	const float lum = Luminance(rgb);
	starcolor::Rgb out = {
		lum + (rgb.r - lum) * saturation,
		lum + (rgb.g - lum) * saturation,
		lum + (rgb.b - lum) * saturation,
	};
	out.r = TuneClamp(out.r, 0.0f, 1.0f);
	out.g = TuneClamp(out.g, 0.0f, 1.0f);
	out.b = TuneClamp(out.b, 0.0f, 1.0f);
	return out;
}

inline starcolor::Rgb PresentStarRgb(float temperatureK, float saturation)
{
	return BoostChroma(starcolor::TemperatureRgb(temperatureK), saturation);
}

// Modest core scale so emissive 1..6 feeds bloom without clipping every channel.
inline float CoreGain(float emissive)
{
	return 0.82f + 0.18f * emissive;
}

inline starcolor::Rgb ToneMapLuminance(starcolor::Rgb hdr, float exposure)
{
	starcolor::Rgb e = { hdr.r * exposure, hdr.g * exposure, hdr.b * exposure };
	const float lum = Luminance(e);
	if (lum <= 1.0e-5f)
	{
		return e;
	}
	const float mappedLum = lum / (1.0f + lum * kTonemapShoulder);
	const float scale = mappedLum / lum;
	starcolor::Rgb mapped = { e.r * scale, e.g * scale, e.b * scale };
	// Hue-preserving display clamp. Per-channel clip would flatten hot stars to white.
	const float peak = (std::max)(mapped.r, (std::max)(mapped.g, mapped.b));
	if (peak > 1.0f)
	{
		mapped.r /= peak;
		mapped.g /= peak;
		mapped.b /= peak;
	}
	return mapped;
}

inline bool NearlyEqual(float a, float b, float eps)
{
	return std::fabs(a - b) <= eps;
}

inline bool ValidateVisualLabMath(std::string& error, char* logBuf, size_t logBufSize)
{
	const TuneParams defaults = TuneDefaults();
	const GateFragment nearGate = EvaluateGate(defaults.gateOpacity, defaults.gateDistanceAtten, 0.0f);
	const GateFragment farGate = EvaluateGate(defaults.gateOpacity, defaults.gateDistanceAtten, 1.0f);

	if (nearGate.rgb.r < 0.40f || !NearlyEqual(nearGate.rgb.r, nearGate.rgb.g, 1.0e-5f) ||
		!NearlyEqual(nearGate.rgb.g, nearGate.rgb.b, 1.0e-5f))
	{
		error = "gate shader RGB is not a constant light grey";
		return false;
	}
	if (nearGate.blended.r < 0.15f || nearGate.blended.g < 0.15f || nearGate.blended.b < 0.15f)
	{
		error = "near gate blended contribution is too dark (black-line regression)";
		return false;
	}
	if (Luminance(farGate.blended) >= Luminance(nearGate.blended) - 1.0e-4f)
	{
		error = "gate distance attenuation does not reduce contribution";
		return false;
	}
	if (farGate.rgb.r + 1.0e-5f < nearGate.rgb.r)
	{
		error = "gate attenuation darkened RGB instead of alpha";
		return false;
	}

	const starcolor::Rgb warmSrc = starcolor::TemperatureRgb(2300.0f);
	const starcolor::Rgb coolSrc = starcolor::TemperatureRgb(7305.0f);
	const starcolor::Rgb whiteSrc = starcolor::TemperatureRgb(6000.0f);
	const starcolor::Rgb warm = PresentStarRgb(2300.0f, defaults.starSaturation);
	const starcolor::Rgb mid = PresentStarRgb(4436.0f, defaults.starSaturation);
	const starcolor::Rgb cool = PresentStarRgb(7305.0f, defaults.starSaturation);
	const starcolor::Rgb white = PresentStarRgb(6000.0f, defaults.starSaturation);

	if (Chroma(warm) <= Chroma(warmSrc) + 0.04f || Chroma(cool) <= Chroma(coolSrc) + 0.02f)
	{
		error = "saturation boost did not increase warm/cool chroma vs the raw blackbody";
		return false;
	}
	if (warm.r <= warm.b + 0.20f)
	{
		error = "2300 K star is not materially warmer than it is blue";
		return false;
	}
	if (cool.b <= cool.r + 0.08f)
	{
		error = "7305 K star is not materially cooler than it is red";
		return false;
	}
	if (Chroma(white) >= 0.18f)
	{
		error = "6000 K star should stay near-white";
		return false;
	}

	const float warmE = starcolor::EmissiveForTemperature(2300.0f);
	const float coolE = starcolor::EmissiveForTemperature(7305.0f);
	const starcolor::Rgb warmHdr = {
		warm.r * CoreGain(warmE),
		warm.g * CoreGain(warmE),
		warm.b * CoreGain(warmE),
	};
	const starcolor::Rgb coolHdr = {
		cool.r * CoreGain(coolE),
		cool.g * CoreGain(coolE),
		cool.b * CoreGain(coolE),
	};
	const starcolor::Rgb warmMap = ToneMapLuminance(warmHdr, defaults.exposure);
	const starcolor::Rgb coolMap = ToneMapLuminance(coolHdr, defaults.exposure);

	const bool warmWhite = warmMap.r > 0.97f && warmMap.g > 0.97f && warmMap.b > 0.97f;
	const bool coolWhite = coolMap.r > 0.97f && coolMap.g > 0.97f && coolMap.b > 0.97f;
	if (warmWhite || coolWhite)
	{
		error = "tonemap collapsed a representative star to white";
		return false;
	}
	if (std::fabs(warmMap.r - coolMap.r) < 0.08f && std::fabs(warmMap.b - coolMap.b) < 0.08f)
	{
		error = "tonemap made warm and cool stars effectively the same colour";
		return false;
	}
	if (Chroma(warmMap) < 0.12f || Chroma(coolMap) < 0.08f)
	{
		error = "tonemap removed usable chroma from representative stars";
		return false;
	}

	if (logBuf && logBufSize > 0)
	{
		std::snprintf(
			logBuf,
			logBufSize,
			"visual math: gate near rgb=%.3f a=%.3f blended=%.3f far blended=%.3f | "
			"raw chroma 2300=%.3f 7305=%.3f | present 2300=%.3f,%.3f,%.3f 4436=%.3f,%.3f,%.3f "
			"7305=%.3f,%.3f,%.3f 6000=%.3f,%.3f,%.3f | mapped 2300=%.3f,%.3f,%.3f 7305=%.3f,%.3f,%.3f\n",
			nearGate.rgb.r,
			nearGate.alpha,
			Luminance(nearGate.blended),
			Luminance(farGate.blended),
			Chroma(warmSrc),
			Chroma(coolSrc),
			warm.r,
			warm.g,
			warm.b,
			mid.r,
			mid.g,
			mid.b,
			cool.r,
			cool.g,
			cool.b,
			white.r,
			white.g,
			white.b,
			warmMap.r,
			warmMap.g,
			warmMap.b,
			coolMap.r,
			coolMap.g,
			coolMap.b);
	}
	(void)whiteSrc;
	return true;
}
}
