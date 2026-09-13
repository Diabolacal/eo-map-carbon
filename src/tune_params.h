#pragma once

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Checked-in Creator Mode baseline. Values are the human-approved visual pass
// locked before the ISM volume fix. F9 / Baseline restore TuneDefaults().
struct TuneParams
{
	float starSize = 0.39f;
	float starBrightness = 1.26f;
	float starSaturation = 2.50f;
	float starDepthDesat = 0.18f;
	bool bloomEnabled = true;
	float bloomThreshold = 0.06f;
	float bloomStrength = 0.89f;
	float bloomRadius = 1.80f;
	float nearStarAtten = 1.80f;
	float farStarAtten = 0.57f;
	float gateOpacity = 0.19f;
	float gateDistanceAtten = 1.61f;
	float gateTintR = 0.55f;
	float gateTintG = 0.55f;
	float gateTintB = 0.55f;
	float exposure = 2.18f;
	float saturation = 1.21f;
	float contrast = 0.99f;
	float blackLevel = 0.00f;
	float gamma = 1.19f;
	float vignette = 0.35f;
	float bloomTintR = 1.00f;
	float bloomTintG = 1.00f;
	float bloomTintB = 1.00f;

	bool skyEnabled = true;
	float skyIntensity = 0.49f;
	float skyContrast = 2.23f;
	float skyBand = 0.41f;
	float skyStarAmount = 2.00f;
	float skyStarBright = 1.37f;
	float skyCoolR = 1.000f;
	float skyCoolG = 0.000f;
	float skyCoolB = 0.000f;
	float skyWarmR = 1.000f;
	float skyWarmG = 1.000f;
	float skyWarmB = 0.000f;
	float skyBaseR = 0.000f;
	float skyBaseG = 0.000f;
	float skyBaseB = 0.000f;

	bool ismEnabled = true;
	float ismDensity = 1.46f;
	float ismScale = 0.91f;
	float ismDetail = 0.70f;
	float ismContrast = 2.73f;
	float ismEmission = 0.170f;
	float ismRedden = 0.78f;
	float ismStarExt = 0.37f;
	float ismMinT = 0.46f;
	float ismNearCut = 0.17f;
	float ismDarkLane = 0.69f;
	float ismDarkScale = 1.56f;
	float ismLightLane = 0.170f;
	float ismLightScale = 1.10f;
	float ismPrimaryR = 0.160f;
	float ismPrimaryG = 0.100f;
	float ismPrimaryB = 0.220f;
	float ismSecondaryR = 0.120f;
	float ismSecondaryG = 0.090f;
	float ismSecondaryB = 0.070f;
	float ismHighlightR = 0.320f;
	float ismHighlightG = 0.220f;
	float ismHighlightB = 0.160f;
	float ismScatter = 0.18f;
	float ismSteps = 16.00f;
	float ismRadius = 32.00f;
	float ismThickness = 8.00f;
	float ismEdgeSoft = 1.40f;
	float ismLobe = 0.70f;
	float ismDebug = 0.00f;

	bool glowEnabled = true;
	float glowIntensity = 0.68f;
	float glowScale = 2.28f;
	float glowThreshold = 0.95f;
	bool flareEnabled = false;
	float flareIntensity = 0.74f;
	float flareThreshold = 0.52f;
	float flareLength = 2.85f;
	float flareChroma = 0.28f;

	bool regionEnabled = false;
	float regionStarMix = 0.29f;
	float regionIsmMix = 0.16f;
};

inline TuneParams TuneDefaults()
{
	return TuneParams();
}

inline float TuneClamp(float v, float lo, float hi)
{
	if (!(v == v)) // NaN
	{
		return lo;
	}
	if (v < lo)
	{
		return lo;
	}
	if (v > hi)
	{
		return hi;
	}
	return v;
}

inline bool TuneIsFinite(float v)
{
	return v == v && v != HUGE_VALF && v != -HUGE_VALF;
}

inline int TuneToTicks(float v, float lo, float hi)
{
	const float clamped = TuneClamp(v, lo, hi);
	return int(floorf((clamped - lo) / 0.01f + 0.5f));
}

inline float TuneFromTicks(int ticks, float lo, float hi)
{
	return TuneClamp(lo + float(ticks) * 0.01f, lo, hi);
}

struct TuneFloatField
{
	const char* key;
	float TuneParams::* member;
	float lo;
	float hi;
};

struct TuneBoolField
{
	const char* key;
	bool TuneParams::* member;
};

inline const TuneFloatField* TuneFloatFields(size_t& count)
{
	static const TuneFloatField kFields[] = {
		{ "starSize", &TuneParams::starSize, 0.10f, 8.00f },
		{ "starBrightness", &TuneParams::starBrightness, 0.00f, 4.00f },
		{ "starSaturation", &TuneParams::starSaturation, 0.50f, 3.00f },
		{ "starDepthDesat", &TuneParams::starDepthDesat, 0.00f, 1.00f },
		{ "bloomThreshold", &TuneParams::bloomThreshold, 0.00f, 2.00f },
		{ "bloomStrength", &TuneParams::bloomStrength, 0.00f, 4.00f },
		{ "bloomRadius", &TuneParams::bloomRadius, 0.10f, 8.00f },
		{ "nearStarAtten", &TuneParams::nearStarAtten, 0.00f, 4.00f },
		{ "farStarAtten", &TuneParams::farStarAtten, 0.00f, 4.00f },
		{ "gateOpacity", &TuneParams::gateOpacity, 0.00f, 1.00f },
		{ "gateDistanceAtten", &TuneParams::gateDistanceAtten, 0.00f, 4.00f },
		{ "gateTintR", &TuneParams::gateTintR, 0.00f, 1.00f },
		{ "gateTintG", &TuneParams::gateTintG, 0.00f, 1.00f },
		{ "gateTintB", &TuneParams::gateTintB, 0.00f, 1.00f },
		{ "exposure", &TuneParams::exposure, 0.10f, 4.00f },
		{ "saturation", &TuneParams::saturation, 0.00f, 2.00f },
		{ "contrast", &TuneParams::contrast, 0.50f, 2.00f },
		{ "blackLevel", &TuneParams::blackLevel, 0.00f, 0.25f },
		{ "gamma", &TuneParams::gamma, 0.40f, 2.20f },
		{ "vignette", &TuneParams::vignette, 0.00f, 1.50f },
		{ "bloomTintR", &TuneParams::bloomTintR, 0.00f, 2.00f },
		{ "bloomTintG", &TuneParams::bloomTintG, 0.00f, 2.00f },
		{ "bloomTintB", &TuneParams::bloomTintB, 0.00f, 2.00f },
		{ "skyIntensity", &TuneParams::skyIntensity, 0.00f, 2.00f },
		{ "skyContrast", &TuneParams::skyContrast, 0.50f, 4.00f },
		{ "skyBand", &TuneParams::skyBand, 0.00f, 1.00f },
		{ "skyStarAmount", &TuneParams::skyStarAmount, 0.00f, 2.00f },
		{ "skyStarBright", &TuneParams::skyStarBright, 0.00f, 2.00f },
		{ "skyCoolR", &TuneParams::skyCoolR, 0.00f, 1.00f },
		{ "skyCoolG", &TuneParams::skyCoolG, 0.00f, 1.00f },
		{ "skyCoolB", &TuneParams::skyCoolB, 0.00f, 1.00f },
		{ "skyWarmR", &TuneParams::skyWarmR, 0.00f, 1.00f },
		{ "skyWarmG", &TuneParams::skyWarmG, 0.00f, 1.00f },
		{ "skyWarmB", &TuneParams::skyWarmB, 0.00f, 1.00f },
		{ "skyBaseR", &TuneParams::skyBaseR, 0.00f, 1.00f },
		{ "skyBaseG", &TuneParams::skyBaseG, 0.00f, 1.00f },
		{ "skyBaseB", &TuneParams::skyBaseB, 0.00f, 1.00f },
		{ "ismDensity", &TuneParams::ismDensity, 0.00f, 4.00f },
		{ "ismScale", &TuneParams::ismScale, 0.20f, 4.00f },
		{ "ismDetail", &TuneParams::ismDetail, 0.00f, 1.00f },
		{ "ismContrast", &TuneParams::ismContrast, 0.50f, 6.00f },
		{ "ismEmission", &TuneParams::ismEmission, 0.00f, 0.60f },
		{ "ismRedden", &TuneParams::ismRedden, 0.00f, 1.00f },
		{ "ismStarExt", &TuneParams::ismStarExt, 0.00f, 1.00f },
		{ "ismMinT", &TuneParams::ismMinT, 0.05f, 1.00f },
		{ "ismNearCut", &TuneParams::ismNearCut, 0.15f, 0.60f },
		{ "ismDarkLane", &TuneParams::ismDarkLane, 0.00f, 2.00f },
		{ "ismDarkScale", &TuneParams::ismDarkScale, 0.20f, 4.00f },
		{ "ismLightLane", &TuneParams::ismLightLane, 0.00f, 1.00f },
		{ "ismLightScale", &TuneParams::ismLightScale, 0.20f, 4.00f },
		{ "ismPrimaryR", &TuneParams::ismPrimaryR, 0.00f, 1.00f },
		{ "ismPrimaryG", &TuneParams::ismPrimaryG, 0.00f, 1.00f },
		{ "ismPrimaryB", &TuneParams::ismPrimaryB, 0.00f, 1.00f },
		{ "ismSecondaryR", &TuneParams::ismSecondaryR, 0.00f, 1.00f },
		{ "ismSecondaryG", &TuneParams::ismSecondaryG, 0.00f, 1.00f },
		{ "ismSecondaryB", &TuneParams::ismSecondaryB, 0.00f, 1.00f },
		{ "ismHighlightR", &TuneParams::ismHighlightR, 0.00f, 1.00f },
		{ "ismHighlightG", &TuneParams::ismHighlightG, 0.00f, 1.00f },
		{ "ismHighlightB", &TuneParams::ismHighlightB, 0.00f, 1.00f },
		{ "ismScatter", &TuneParams::ismScatter, 0.00f, 2.00f },
		{ "ismSteps", &TuneParams::ismSteps, 8.00f, 48.00f },
		{ "ismRadius", &TuneParams::ismRadius, 12.00f, 80.00f },
		{ "ismThickness", &TuneParams::ismThickness, 1.00f, 24.00f },
		{ "ismEdgeSoft", &TuneParams::ismEdgeSoft, 0.40f, 2.50f },
		{ "ismLobe", &TuneParams::ismLobe, 0.00f, 2.00f },
		{ "ismDebug", &TuneParams::ismDebug, 0.00f, 6.00f },
		{ "glowIntensity", &TuneParams::glowIntensity, 0.00f, 2.00f },
		{ "glowScale", &TuneParams::glowScale, 0.50f, 6.00f },
		{ "glowThreshold", &TuneParams::glowThreshold, 0.00f, 2.00f },
		{ "flareIntensity", &TuneParams::flareIntensity, 0.00f, 2.00f },
		{ "flareThreshold", &TuneParams::flareThreshold, 0.00f, 2.00f },
		{ "flareLength", &TuneParams::flareLength, 0.50f, 8.00f },
		{ "flareChroma", &TuneParams::flareChroma, 0.00f, 1.00f },
		{ "regionStarMix", &TuneParams::regionStarMix, 0.00f, 1.00f },
		{ "regionIsmMix", &TuneParams::regionIsmMix, 0.00f, 1.00f },
	};
	count = sizeof(kFields) / sizeof(kFields[0]);
	return kFields;
}

inline const TuneBoolField* TuneBoolFields(size_t& count)
{
	static const TuneBoolField kFields[] = {
		{ "bloomEnabled", &TuneParams::bloomEnabled },
		{ "skyEnabled", &TuneParams::skyEnabled },
		{ "ismEnabled", &TuneParams::ismEnabled },
		{ "glowEnabled", &TuneParams::glowEnabled },
		{ "flareEnabled", &TuneParams::flareEnabled },
		{ "regionEnabled", &TuneParams::regionEnabled },
	};
	count = sizeof(kFields) / sizeof(kFields[0]);
	return kFields;
}

inline void TuneClampAll(TuneParams& p)
{
	size_t n = 0;
	const TuneFloatField* fields = TuneFloatFields(n);
	for (size_t i = 0; i < n; ++i)
	{
		p.*(fields[i].member) = TuneClamp(p.*(fields[i].member), fields[i].lo, fields[i].hi);
	}
}

inline std::wstring TuneIniPath()
{
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring path(exePath);
	const auto slash = path.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
	{
		path.erase(slash + 1);
	}
	path += L"eo-map-carbon-neweden-tune.ini";
	return path;
}

inline int FormatTuneText(char* buf, size_t n, const TuneParams& p)
{
	return std::snprintf(
		buf,
		n,
		"; eo-map-carbon creator-mode settings\r\n"
		"; Interactive host loads this file on startup. --smoke never does.\r\n"
		"; Unknown keys are ignored. Non-finite values keep the previous field.\r\n"
		"; preset=Baseline is TuneDefaults() / F9.\r\n"
		"preset=Baseline\r\n"
		"starSize=%.2f\r\n"
		"starBrightness=%.2f\r\n"
		"starSaturation=%.2f\r\n"
		"starDepthDesat=%.2f\r\n"
		"bloomEnabled=%d\r\n"
		"bloomThreshold=%.2f\r\n"
		"bloomStrength=%.2f\r\n"
		"bloomRadius=%.2f\r\n"
		"nearStarAtten=%.2f\r\n"
		"farStarAtten=%.2f\r\n"
		"gateOpacity=%.2f\r\n"
		"gateDistanceAtten=%.2f\r\n"
		"gateTintR=%.2f\r\n"
		"gateTintG=%.2f\r\n"
		"gateTintB=%.2f\r\n"
		"exposure=%.2f\r\n"
		"saturation=%.2f\r\n"
		"contrast=%.2f\r\n"
		"blackLevel=%.2f\r\n"
		"gamma=%.2f\r\n"
		"vignette=%.2f\r\n"
		"bloomTintR=%.2f\r\n"
		"bloomTintG=%.2f\r\n"
		"bloomTintB=%.2f\r\n"
		"skyEnabled=%d\r\n"
		"skyIntensity=%.2f\r\n"
		"skyContrast=%.2f\r\n"
		"skyBand=%.2f\r\n"
		"skyStarAmount=%.2f\r\n"
		"skyStarBright=%.2f\r\n"
		"skyCoolR=%.3f\r\n"
		"skyCoolG=%.3f\r\n"
		"skyCoolB=%.3f\r\n"
		"skyWarmR=%.3f\r\n"
		"skyWarmG=%.3f\r\n"
		"skyWarmB=%.3f\r\n"
		"skyBaseR=%.3f\r\n"
		"skyBaseG=%.3f\r\n"
		"skyBaseB=%.3f\r\n"
		"ismEnabled=%d\r\n"
		"ismDensity=%.2f\r\n"
		"ismScale=%.2f\r\n"
		"ismRadius=%.2f\r\n"
		"ismThickness=%.2f\r\n"
		"ismEdgeSoft=%.2f\r\n"
		"ismLobe=%.2f\r\n"
		"ismDetail=%.2f\r\n"
		"ismContrast=%.2f\r\n"
		"ismEmission=%.3f\r\n"
		"ismRedden=%.2f\r\n"
		"ismStarExt=%.2f\r\n"
		"ismMinT=%.2f\r\n"
		"ismNearCut=%.2f\r\n"
		"ismDarkLane=%.2f\r\n"
		"ismDarkScale=%.2f\r\n"
		"ismLightLane=%.3f\r\n"
		"ismLightScale=%.2f\r\n"
		"ismPrimaryR=%.3f\r\n"
		"ismPrimaryG=%.3f\r\n"
		"ismPrimaryB=%.3f\r\n"
		"ismSecondaryR=%.3f\r\n"
		"ismSecondaryG=%.3f\r\n"
		"ismSecondaryB=%.3f\r\n"
		"ismHighlightR=%.3f\r\n"
		"ismHighlightG=%.3f\r\n"
		"ismHighlightB=%.3f\r\n"
		"ismScatter=%.2f\r\n"
		"ismSteps=%.0f\r\n"
		"ismDebug=%.0f\r\n"
		"glowEnabled=%d\r\n"
		"glowIntensity=%.2f\r\n"
		"glowScale=%.2f\r\n"
		"glowThreshold=%.2f\r\n"
		"flareEnabled=%d\r\n"
		"flareIntensity=%.2f\r\n"
		"flareThreshold=%.2f\r\n"
		"flareLength=%.2f\r\n"
		"flareChroma=%.2f\r\n"
		"regionEnabled=%d\r\n"
		"regionStarMix=%.2f\r\n"
		"regionIsmMix=%.2f\r\n",
		p.starSize,
		p.starBrightness,
		p.starSaturation,
		p.starDepthDesat,
		p.bloomEnabled ? 1 : 0,
		p.bloomThreshold,
		p.bloomStrength,
		p.bloomRadius,
		p.nearStarAtten,
		p.farStarAtten,
		p.gateOpacity,
		p.gateDistanceAtten,
		p.gateTintR,
		p.gateTintG,
		p.gateTintB,
		p.exposure,
		p.saturation,
		p.contrast,
		p.blackLevel,
		p.gamma,
		p.vignette,
		p.bloomTintR,
		p.bloomTintG,
		p.bloomTintB,
		p.skyEnabled ? 1 : 0,
		p.skyIntensity,
		p.skyContrast,
		p.skyBand,
		p.skyStarAmount,
		p.skyStarBright,
		p.skyCoolR,
		p.skyCoolG,
		p.skyCoolB,
		p.skyWarmR,
		p.skyWarmG,
		p.skyWarmB,
		p.skyBaseR,
		p.skyBaseG,
		p.skyBaseB,
		p.ismEnabled ? 1 : 0,
		p.ismDensity,
		p.ismScale,
		p.ismRadius,
		p.ismThickness,
		p.ismEdgeSoft,
		p.ismLobe,
		p.ismDetail,
		p.ismContrast,
		p.ismEmission,
		p.ismRedden,
		p.ismStarExt,
		p.ismMinT,
		p.ismNearCut,
		p.ismDarkLane,
		p.ismDarkScale,
		p.ismLightLane,
		p.ismLightScale,
		p.ismPrimaryR,
		p.ismPrimaryG,
		p.ismPrimaryB,
		p.ismSecondaryR,
		p.ismSecondaryG,
		p.ismSecondaryB,
		p.ismHighlightR,
		p.ismHighlightG,
		p.ismHighlightB,
		p.ismScatter,
		p.ismSteps,
		p.ismDebug,
		p.glowEnabled ? 1 : 0,
		p.glowIntensity,
		p.glowScale,
		p.glowThreshold,
		p.flareEnabled ? 1 : 0,
		p.flareIntensity,
		p.flareThreshold,
		p.flareLength,
		p.flareChroma,
		p.regionEnabled ? 1 : 0,
		p.regionStarMix,
		p.regionIsmMix);
}

inline bool ParseTuneKey(TuneParams& p, const char* key, const char* value)
{
	if (!key || !value || !key[0])
	{
		return false;
	}
	if (std::strcmp(key, "preset") == 0)
	{
		return true;
	}

	size_t boolCount = 0;
	const TuneBoolField* bools = TuneBoolFields(boolCount);
	for (size_t i = 0; i < boolCount; ++i)
	{
		if (std::strcmp(key, bools[i].key) == 0)
		{
			if (std::strcmp(value, "1") == 0 || _stricmp(value, "true") == 0 || _stricmp(value, "on") == 0)
			{
				p.*(bools[i].member) = true;
				return true;
			}
			if (std::strcmp(value, "0") == 0 || _stricmp(value, "false") == 0 || _stricmp(value, "off") == 0)
			{
				p.*(bools[i].member) = false;
				return true;
			}
			return false;
		}
	}

	char* end = nullptr;
	const float parsed = std::strtof(value, &end);
	if (!end || end == value || !TuneIsFinite(parsed))
	{
		return false;
	}

	size_t floatCount = 0;
	const TuneFloatField* floats = TuneFloatFields(floatCount);
	for (size_t i = 0; i < floatCount; ++i)
	{
		if (std::strcmp(key, floats[i].key) == 0)
		{
			p.*(floats[i].member) = TuneClamp(parsed, floats[i].lo, floats[i].hi);
			return true;
		}
	}
	return false; // unknown key
}

inline void ParseTuneText(TuneParams& p, const char* text)
{
	if (!text)
	{
		return;
	}
	const char* cursor = text;
	while (*cursor)
	{
		char line[256] = {};
		size_t n = 0;
		while (*cursor && *cursor != '\n' && n + 1 < sizeof(line))
		{
			if (*cursor != '\r')
			{
				line[n++] = *cursor;
			}
			++cursor;
		}
		if (*cursor == '\n')
		{
			++cursor;
		}
		line[n] = 0;
		if (n == 0 || line[0] == ';' || line[0] == '#')
		{
			continue;
		}
		char* eq = std::strchr(line, '=');
		if (!eq)
		{
			continue;
		}
		*eq = 0;
		const char* key = line;
		const char* value = eq + 1;
		while (*key == ' ' || *key == '\t')
		{
			++key;
		}
		while (*value == ' ' || *value == '\t')
		{
			++value;
		}
		ParseTuneKey(p, key, value);
	}
	TuneClampAll(p);
}

inline bool CopyTuneToClipboard(const char* text)
{
	if (!text || !OpenClipboard(nullptr))
	{
		return false;
	}
	EmptyClipboard();
	const size_t bytes = std::strlen(text) + 1;
	bool ok = false;
	if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes))
	{
		if (void* locked = GlobalLock(mem))
		{
			std::memcpy(locked, text, bytes);
			GlobalUnlock(mem);
			ok = SetClipboardData(CF_TEXT, mem) != nullptr;
			if (!ok)
			{
				GlobalFree(mem);
			}
		}
		else
		{
			GlobalFree(mem);
		}
	}
	CloseClipboard();
	return ok;
}

inline bool WriteTuneDump(const TuneParams& p, char* textOut, size_t textOutSize)
{
	char text[4096] = {};
	FormatTuneText(text, sizeof(text), p);
	if (textOut && textOutSize > 0)
	{
		std::strncpy(textOut, text, textOutSize - 1);
		textOut[textOutSize - 1] = 0;
	}

	const std::wstring path = TuneIniPath();
	FILE* file = nullptr;
	_wfopen_s(&file, path.c_str(), L"wb");
	bool wroteFile = false;
	if (file)
	{
		std::fputs(text, file);
		std::fclose(file);
		wroteFile = true;
	}

	CopyTuneToClipboard(text);
	OutputDebugStringA(text);
	return wroteFile;
}

inline bool LoadTuneFile(TuneParams& p, const wchar_t* path)
{
	FILE* file = nullptr;
	_wfopen_s(&file, path, L"rb");
	if (!file)
	{
		return false;
	}
	char text[8192] = {};
	const size_t read = std::fread(text, 1, sizeof(text) - 1, file);
	std::fclose(file);
	text[read] = 0;
	ParseTuneText(p, text);
	return true;
}

inline bool LoadTuneFromExeDir(TuneParams& p)
{
	const std::wstring path = TuneIniPath();
	return LoadTuneFile(p, path.c_str());
}

inline bool ValidateTunePersist(std::string& error)
{
	TuneParams parsed = TuneDefaults();
	ParseTuneText(parsed, "starSize=1.25\r\nunknownKey=9\r\nbloomEnabled=0\r\nexposure=notanumber\r\n");
	if (std::fabs(parsed.starSize - 1.25f) > 1.0e-4f)
	{
		error = "persist parse did not apply starSize";
		return false;
	}
	if (parsed.bloomEnabled)
	{
		error = "persist parse did not apply bloomEnabled=0";
		return false;
	}
	if (std::fabs(parsed.exposure - TuneDefaults().exposure) > 1.0e-4f)
	{
		error = "non-finite exposure must leave the default";
		return false;
	}

	TuneParams clamped = TuneDefaults();
	ParseTuneText(clamped, "starSize=99\r\nexposure=-3\r\nismMinT=0\r\n");
	if (std::fabs(clamped.starSize - 8.00f) > 1.0e-4f || std::fabs(clamped.exposure - 0.10f) > 1.0e-4f ||
		std::fabs(clamped.ismMinT - 0.05f) > 1.0e-4f)
	{
		error = "persist parse did not clamp out-of-range values";
		return false;
	}

	TuneParams roundtrip = TuneDefaults();
	roundtrip.skyIntensity = 0.77f;
	roundtrip.regionEnabled = true;
	char text[4096] = {};
	FormatTuneText(text, sizeof(text), roundtrip);
	TuneParams again = TuneDefaults();
	ParseTuneText(again, text);
	if (std::fabs(again.skyIntensity - 0.77f) > 0.02f || !again.regionEnabled)
	{
		error = "persist format/parse roundtrip failed";
		return false;
	}

	TuneParams empty = TuneDefaults();
	ParseTuneText(empty, "");
	ParseTuneText(empty, "; comment only\r\n\r\nbogus\r\n=novalue\r\n");
	if (std::fabs(empty.starSize - TuneDefaults().starSize) > 1.0e-4f)
	{
		error = "malformed persist text mutated defaults";
		return false;
	}
	return true;
}
