#pragma once

#include <Windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

struct TuneParams
{
	float starSize = 1.00f;
	float starBrightness = 1.00f;
	float starSaturation = 1.70f;
	bool bloomEnabled = true;
	float bloomThreshold = 0.90f;
	float bloomStrength = 0.55f;
	float bloomRadius = 1.60f;
	float nearStarAtten = 1.15f;
	float farStarAtten = 0.40f;
	float gateOpacity = 0.38f;
	float gateDistanceAtten = 1.00f;
	float exposure = 1.10f;
};

inline TuneParams TuneDefaults()
{
	return TuneParams();
}

inline float TuneClamp(float v, float lo, float hi)
{
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

inline int TuneToTicks(float v, float lo, float hi)
{
	const float clamped = TuneClamp(v, lo, hi);
	return int(floorf((clamped - lo) / 0.01f + 0.5f));
}

inline float TuneFromTicks(int ticks, float lo, float hi)
{
	return TuneClamp(lo + float(ticks) * 0.01f, lo, hi);
}

inline int FormatTuneText(char* buf, size_t n, const TuneParams& p)
{
	return std::snprintf(
		buf,
		n,
		"; eo-map-carbon visual lab dump (not loaded on startup)\r\n"
		"starSize=%.2f\r\n"
		"starBrightness=%.2f\r\n"
		"starSaturation=%.2f\r\n"
		"bloomEnabled=%d\r\n"
		"bloomThreshold=%.2f\r\n"
		"bloomStrength=%.2f\r\n"
		"bloomRadius=%.2f\r\n"
		"nearStarAtten=%.2f\r\n"
		"farStarAtten=%.2f\r\n"
		"gateOpacity=%.2f\r\n"
		"gateDistanceAtten=%.2f\r\n"
		"exposure=%.2f\r\n",
		p.starSize,
		p.starBrightness,
		p.starSaturation,
		p.bloomEnabled ? 1 : 0,
		p.bloomThreshold,
		p.bloomStrength,
		p.bloomRadius,
		p.nearStarAtten,
		p.farStarAtten,
		p.gateOpacity,
		p.gateDistanceAtten,
		p.exposure);
}

inline bool WriteTuneDump(const TuneParams& p, char* textOut, size_t textOutSize)
{
	char text[1024] = {};
	FormatTuneText(text, sizeof(text), p);
	if (textOut && textOutSize > 0)
	{
		std::strncpy(textOut, text, textOutSize - 1);
		textOut[textOutSize - 1] = 0;
	}

	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring path(exePath);
	const auto slash = path.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
	{
		path.erase(slash + 1);
	}
	path += L"eo-map-carbon-neweden-tune.ini";

	FILE* file = nullptr;
	_wfopen_s(&file, path.c_str(), L"wb");
	bool wroteFile = false;
	if (file)
	{
		std::fputs(text, file);
		std::fclose(file);
		wroteFile = true;
	}

	if (OpenClipboard(nullptr))
	{
		EmptyClipboard();
		const size_t bytes = std::strlen(text) + 1;
		if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes))
		{
			if (void* locked = GlobalLock(mem))
			{
				std::memcpy(locked, text, bytes);
				GlobalUnlock(mem);
				SetClipboardData(CF_TEXT, mem);
			}
			else
			{
				GlobalFree(mem);
			}
		}
		CloseClipboard();
	}

	OutputDebugStringA(text);
	return wroteFile;
}
