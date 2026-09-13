#include "tune_panel.h"

#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <cstdio>

namespace
{
const wchar_t* kPanelClass = L"eo-map-carbon-tune";
const wchar_t* kPageClass = L"eo-map-carbon-tune-page";

enum TabId
{
	TAB_STARS = 0,
	TAB_GATES,
	TAB_SKY,
	TAB_ISM,
	TAB_GLOW,
	TAB_COLOUR,
	TAB_POST,
	TAB_COUNT,
};

const wchar_t* kTabNames[TAB_COUNT] = {
	L"Stars",
	L"Gates",
	L"Background",
	L"Medium",
	L"Glow",
	L"Colour",
	L"Post",
};

enum ControlId
{
	IDC_TABS = 900,
	IDC_BLOOM_ENABLE = 1021,
	IDC_SKY_ENABLE = 1022,
	IDC_ISM_ENABLE = 1023,
	IDC_GLOW_ENABLE = 1024,
	IDC_FLARE_ENABLE = 1025,
	IDC_REGION_ENABLE = 1026,
	IDC_RESET = 1201,
	IDC_DUMP = 1202,
	IDC_SAVE = 1203,
	IDC_LOAD = 1204,
};

struct SliderDesc
{
	int tab;
	int id;
	int valueId;
	const wchar_t* label;
	float lo;
	float hi;
	float TuneParams::* field;
};

struct CheckDesc
{
	int tab;
	int id;
	const wchar_t* label;
	bool TuneParams::* field;
};

struct ColorDesc
{
	int tab;
	int id;
	const wchar_t* label;
	float TuneParams::* r;
	float TuneParams::* g;
	float TuneParams::* b;
};

const SliderDesc kSliders[] = {
	{ TAB_STARS, 2001, 2002, L"Star size", 0.10f, 8.00f, &TuneParams::starSize },
	{ TAB_STARS, 2003, 2004, L"Star brightness", 0.00f, 4.00f, &TuneParams::starBrightness },
	{ TAB_STARS, 2005, 2006, L"Star colour sat", 0.50f, 3.00f, &TuneParams::starSaturation },
	{ TAB_STARS, 2007, 2008, L"Depth desaturate", 0.00f, 1.00f, &TuneParams::starDepthDesat },
	{ TAB_STARS, 2009, 2010, L"Near-star atten", 0.00f, 4.00f, &TuneParams::nearStarAtten },
	{ TAB_STARS, 2011, 2012, L"Far-star atten", 0.00f, 4.00f, &TuneParams::farStarAtten },

	{ TAB_GATES, 2101, 2102, L"Gate opacity", 0.00f, 1.00f, &TuneParams::gateOpacity },
	{ TAB_GATES, 2103, 2104, L"Gate dist atten", 0.00f, 4.00f, &TuneParams::gateDistanceAtten },

	{ TAB_SKY, 2201, 2202, L"Sky intensity", 0.00f, 2.00f, &TuneParams::skyIntensity },
	{ TAB_SKY, 2203, 2204, L"Sky contrast", 0.50f, 4.00f, &TuneParams::skyContrast },
	{ TAB_SKY, 2205, 2206, L"Galactic band", 0.00f, 1.00f, &TuneParams::skyBand },
	{ TAB_SKY, 2207, 2208, L"BG star density", 0.00f, 2.00f, &TuneParams::skyStarAmount },
	{ TAB_SKY, 2209, 2210, L"BG star brightness", 0.00f, 2.00f, &TuneParams::skyStarBright },

	{ TAB_ISM, 2301, 2302, L"Density", 0.00f, 4.00f, &TuneParams::ismDensity },
	{ TAB_ISM, 2303, 2304, L"Scale (structure)", 0.20f, 4.00f, &TuneParams::ismScale },
	{ TAB_ISM, 2331, 2332, L"Radius / extent", 12.00f, 80.00f, &TuneParams::ismRadius },
	{ TAB_ISM, 2333, 2334, L"Thickness", 1.00f, 24.00f, &TuneParams::ismThickness },
	{ TAB_ISM, 2335, 2336, L"Edge softness", 0.40f, 2.50f, &TuneParams::ismEdgeSoft },
	{ TAB_ISM, 2337, 2338, L"Lobe strength", 0.00f, 2.00f, &TuneParams::ismLobe },
	{ TAB_ISM, 2305, 2306, L"Detail / ridge", 0.00f, 1.00f, &TuneParams::ismDetail },
	{ TAB_ISM, 2307, 2308, L"Contrast", 0.50f, 6.00f, &TuneParams::ismContrast },
	{ TAB_ISM, 2309, 2310, L"Emission", 0.00f, 0.60f, &TuneParams::ismEmission },
	{ TAB_ISM, 2311, 2312, L"Reddening", 0.00f, 1.00f, &TuneParams::ismRedden },
	{ TAB_ISM, 2313, 2314, L"Star extinction", 0.00f, 1.00f, &TuneParams::ismStarExt },
	{ TAB_ISM, 2315, 2316, L"Min transmittance", 0.05f, 1.00f, &TuneParams::ismMinT },
	{ TAB_ISM, 2317, 2318, L"Near cut", 0.15f, 0.60f, &TuneParams::ismNearCut },
	{ TAB_ISM, 2319, 2320, L"Dark-lane strength", 0.00f, 2.00f, &TuneParams::ismDarkLane },
	{ TAB_ISM, 2321, 2322, L"Dark-lane scale", 0.20f, 4.00f, &TuneParams::ismDarkScale },
	{ TAB_ISM, 2323, 2324, L"Light-wisp strength", 0.00f, 1.00f, &TuneParams::ismLightLane },
	{ TAB_ISM, 2325, 2326, L"Light-wisp scale", 0.20f, 4.00f, &TuneParams::ismLightScale },
	{ TAB_ISM, 2327, 2328, L"Star scatter", 0.00f, 2.00f, &TuneParams::ismScatter },
	{ TAB_ISM, 2329, 2330, L"Quality steps", 8.00f, 48.00f, &TuneParams::ismSteps },
	{ TAB_ISM, 2339, 2340, L"Debug view", 0.00f, 6.00f, &TuneParams::ismDebug },

	{ TAB_GLOW, 2401, 2402, L"Glow intensity", 0.00f, 2.00f, &TuneParams::glowIntensity },
	{ TAB_GLOW, 2403, 2404, L"Glow scale", 0.50f, 6.00f, &TuneParams::glowScale },
	{ TAB_GLOW, 2405, 2406, L"Glow threshold", 0.00f, 2.00f, &TuneParams::glowThreshold },
	{ TAB_GLOW, 2407, 2408, L"Flare intensity", 0.00f, 2.00f, &TuneParams::flareIntensity },
	{ TAB_GLOW, 2409, 2410, L"Flare threshold", 0.00f, 2.00f, &TuneParams::flareThreshold },
	{ TAB_GLOW, 2411, 2412, L"Flare length", 0.50f, 8.00f, &TuneParams::flareLength },
	{ TAB_GLOW, 2413, 2414, L"Flare chroma", 0.00f, 1.00f, &TuneParams::flareChroma },

	{ TAB_COLOUR, 2501, 2502, L"Region star mix", 0.00f, 1.00f, &TuneParams::regionStarMix },
	{ TAB_COLOUR, 2503, 2504, L"Region ISM mix", 0.00f, 1.00f, &TuneParams::regionIsmMix },

	{ TAB_POST, 2601, 2602, L"Bloom threshold", 0.00f, 2.00f, &TuneParams::bloomThreshold },
	{ TAB_POST, 2603, 2604, L"Bloom strength", 0.00f, 4.00f, &TuneParams::bloomStrength },
	{ TAB_POST, 2605, 2606, L"Bloom radius", 0.10f, 8.00f, &TuneParams::bloomRadius },
	{ TAB_POST, 2607, 2608, L"Exposure", 0.10f, 4.00f, &TuneParams::exposure },
	{ TAB_POST, 2609, 2610, L"Saturation", 0.00f, 2.00f, &TuneParams::saturation },
	{ TAB_POST, 2611, 2612, L"Contrast", 0.50f, 2.00f, &TuneParams::contrast },
	{ TAB_POST, 2613, 2614, L"Black level", 0.00f, 0.25f, &TuneParams::blackLevel },
	{ TAB_POST, 2615, 2616, L"Gamma", 0.40f, 2.20f, &TuneParams::gamma },
	{ TAB_POST, 2617, 2618, L"Vignette", 0.00f, 1.50f, &TuneParams::vignette },
};

const CheckDesc kChecks[] = {
	{ TAB_SKY, IDC_SKY_ENABLE, L"Background enabled", &TuneParams::skyEnabled },
	{ TAB_ISM, IDC_ISM_ENABLE, L"Interstellar medium enabled", &TuneParams::ismEnabled },
	{ TAB_GLOW, IDC_GLOW_ENABLE, L"Star glow enabled", &TuneParams::glowEnabled },
	{ TAB_GLOW, IDC_FLARE_ENABLE, L"Flares enabled", &TuneParams::flareEnabled },
	{ TAB_COLOUR, IDC_REGION_ENABLE, L"Region tint enabled", &TuneParams::regionEnabled },
	{ TAB_POST, IDC_BLOOM_ENABLE, L"Bloom enabled", &TuneParams::bloomEnabled },
};

const ColorDesc kColors[] = {
	{ TAB_GATES, 3101, L"Gate tint", &TuneParams::gateTintR, &TuneParams::gateTintG, &TuneParams::gateTintB },
	{ TAB_SKY, 3201, L"Sky cool", &TuneParams::skyCoolR, &TuneParams::skyCoolG, &TuneParams::skyCoolB },
	{ TAB_SKY, 3202, L"Sky warm", &TuneParams::skyWarmR, &TuneParams::skyWarmG, &TuneParams::skyWarmB },
	{ TAB_SKY, 3203, L"Sky base", &TuneParams::skyBaseR, &TuneParams::skyBaseG, &TuneParams::skyBaseB },
	{ TAB_ISM, 3301, L"Medium primary", &TuneParams::ismPrimaryR, &TuneParams::ismPrimaryG, &TuneParams::ismPrimaryB },
	{ TAB_ISM, 3302, L"Medium secondary", &TuneParams::ismSecondaryR, &TuneParams::ismSecondaryG, &TuneParams::ismSecondaryB },
	{ TAB_ISM, 3303, L"Medium highlight", &TuneParams::ismHighlightR, &TuneParams::ismHighlightG, &TuneParams::ismHighlightB },
	{ TAB_POST, 3601, L"Bloom tint", &TuneParams::bloomTintR, &TuneParams::bloomTintG, &TuneParams::bloomTintB },
};

TunePanel* PanelFromHwnd(HWND hwnd)
{
	return reinterpret_cast<TunePanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void SetValueText(HWND page, int valueId, float v)
{
	wchar_t text[32];
	swprintf_s(text, L"%.2f", v);
	SetDlgItemTextW(page, valueId, text);
}

COLORREF RgbToColor(float r, float g, float b)
{
	const int ir = int(TuneClamp(r, 0.0f, 1.0f) * 255.0f + 0.5f);
	const int ig = int(TuneClamp(g, 0.0f, 1.0f) * 255.0f + 0.5f);
	const int ib = int(TuneClamp(b, 0.0f, 1.0f) * 255.0f + 0.5f);
	return RGB(ir, ig, ib);
}

void PaintColorButton(HWND button, float r, float g, float b)
{
	HDC dc = GetDC(button);
	if (!dc)
	{
		return;
	}
	RECT rc = {};
	GetClientRect(button, &rc);
	HBRUSH brush = CreateSolidBrush(RgbToColor(r, g, b));
	FillRect(dc, &rc, brush);
	DeleteObject(brush);
	ReleaseDC(button, dc);
}

void SyncSlider(HWND page, const SliderDesc& slider, float value)
{
	const int ticks = TuneToTicks(value, slider.lo, slider.hi);
	SendDlgItemMessageW(page, slider.id, TBM_SETPOS, TRUE, ticks);
	SetValueText(page, slider.valueId, TuneFromTicks(ticks, slider.lo, slider.hi));
}

void SyncAll(const TunePanel& panel)
{
	if (!panel.params)
	{
		return;
	}
	for (const SliderDesc& slider : kSliders)
	{
		if (panel.pages[slider.tab])
		{
			SyncSlider(panel.pages[slider.tab], slider, panel.params->*slider.field);
		}
	}
	for (const CheckDesc& check : kChecks)
	{
		if (panel.pages[check.tab])
		{
			SendDlgItemMessageW(
				panel.pages[check.tab],
				check.id,
				BM_SETCHECK,
				(panel.params->*check.field) ? BST_CHECKED : BST_UNCHECKED,
				0);
		}
	}
	for (const ColorDesc& color : kColors)
	{
		if (HWND button = panel.pages[color.tab] ? GetDlgItem(panel.pages[color.tab], color.id) : nullptr)
		{
			PaintColorButton(button, panel.params->*color.r, panel.params->*color.g, panel.params->*color.b);
		}
	}
}

const SliderDesc* FindSlider(int id)
{
	for (const SliderDesc& slider : kSliders)
	{
		if (slider.id == id)
		{
			return &slider;
		}
	}
	return nullptr;
}

const CheckDesc* FindCheck(int id)
{
	for (const CheckDesc& check : kChecks)
	{
		if (check.id == id)
		{
			return &check;
		}
	}
	return nullptr;
}

const ColorDesc* FindColor(int id)
{
	for (const ColorDesc& color : kColors)
	{
		if (color.id == id)
		{
			return &color;
		}
	}
	return nullptr;
}

void ScrollPage(HWND page, int delta)
{
	SCROLLINFO si = { sizeof(si), SIF_ALL };
	GetScrollInfo(page, SB_VERT, &si);
	const int old = si.nPos;
	const int maxPos = (std::max)(0, int(si.nMax) - int(si.nPage));
	int next = old - delta;
	if (next < 0)
	{
		next = 0;
	}
	if (next > maxPos)
	{
		next = maxPos;
	}
	si.nPos = next;
	si.fMask = SIF_POS;
	SetScrollInfo(page, SB_VERT, &si, TRUE);
	GetScrollInfo(page, SB_VERT, &si);
	if (si.nPos != old)
	{
		ScrollWindow(page, 0, old - si.nPos, nullptr, nullptr);
	}
}

void ShowTab(TunePanel& panel, int tab)
{
	panel.currentTab = tab;
	for (int i = 0; i < TAB_COUNT; ++i)
	{
		if (panel.pages[i])
		{
			ShowWindow(panel.pages[i], i == tab ? SW_SHOW : SW_HIDE);
		}
	}
}

void PickColor(HWND owner, TuneParams& params, const ColorDesc& color)
{
	static COLORREF custom[16] = {};
	CHOOSECOLORW cc = {};
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = owner;
	cc.rgbResult = RgbToColor(params.*color.r, params.*color.g, params.*color.b);
	cc.lpCustColors = custom;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	if (ChooseColorW(&cc))
	{
		params.*color.r = float(GetRValue(cc.rgbResult)) / 255.0f;
		params.*color.g = float(GetGValue(cc.rgbResult)) / 255.0f;
		params.*color.b = float(GetBValue(cc.rgbResult)) / 255.0f;
	}
}

LRESULT CALLBACK PageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TunePanel* panel = PanelFromHwnd(GetParent(hwnd));
	switch (msg)
	{
	case WM_HSCROLL:
	{
		if (!panel || !panel->params)
		{
			break;
		}
		const HWND track = reinterpret_cast<HWND>(lParam);
		const int id = GetDlgCtrlID(track);
		if (const SliderDesc* slider = FindSlider(id))
		{
			const int ticks = int(SendMessageW(track, TBM_GETPOS, 0, 0));
			const float value = TuneFromTicks(ticks, slider->lo, slider->hi);
			panel->params->*slider->field = value;
			SetValueText(hwnd, slider->valueId, value);
		}
		return 0;
	}
	case WM_COMMAND:
		if (!panel || !panel->params)
		{
			break;
		}
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (const CheckDesc* check = FindCheck(LOWORD(wParam)))
			{
				panel->params->*check->field = SendDlgItemMessageW(hwnd, check->id, BM_GETCHECK, 0, 0) == BST_CHECKED;
				return 0;
			}
			if (const ColorDesc* color = FindColor(LOWORD(wParam)))
			{
				PickColor(GetParent(hwnd), *panel->params, *color);
				PaintColorButton(GetDlgItem(hwnd, color->id), panel->params->*color->r, panel->params->*color->g, panel->params->*color->b);
				return 0;
			}
		}
		break;
	case WM_DRAWITEM:
	{
		const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
		if (panel && panel->params && dis)
		{
			if (const ColorDesc* color = FindColor(int(dis->CtlID)))
			{
				HBRUSH brush = CreateSolidBrush(RgbToColor(panel->params->*color->r, panel->params->*color->g, panel->params->*color->b));
				FillRect(dis->hDC, &dis->rcItem, brush);
				DeleteObject(brush);
				DrawEdge(dis->hDC, const_cast<RECT*>(&dis->rcItem), EDGE_SUNKEN, BF_RECT);
				return TRUE;
			}
		}
		break;
	}
	case WM_VSCROLL:
	{
		int delta = 0;
		switch (LOWORD(wParam))
		{
		case SB_LINEUP:
			delta = 24;
			break;
		case SB_LINEDOWN:
			delta = -24;
			break;
		case SB_PAGEUP:
			delta = 120;
			break;
		case SB_PAGEDOWN:
			delta = -120;
			break;
		default:
			break;
		}
		if (delta != 0)
		{
			ScrollPage(hwnd, delta);
		}
		return 0;
	}
	case WM_MOUSEWHEEL:
		ScrollPage(hwnd, GET_WHEEL_DELTA_WPARAM(wParam) / 4);
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TunePanel* panel = PanelFromHwnd(hwnd);
	switch (msg)
	{
	case WM_NOTIFY:
		if (panel && reinterpret_cast<NMHDR*>(lParam)->idFrom == IDC_TABS &&
			reinterpret_cast<NMHDR*>(lParam)->code == TCN_SELCHANGE)
		{
			ShowTab(*panel, int(SendMessageW(panel->tabs, TCM_GETCURSEL, 0, 0)));
			return 0;
		}
		break;
	case WM_COMMAND:
		if (!panel || !panel->params)
		{
			break;
		}
		if (HIWORD(wParam) == BN_CLICKED)
		{
			switch (LOWORD(wParam))
			{
			case IDC_RESET:
				*panel->params = TuneDefaults();
				SyncAll(*panel);
				return 0;
			case IDC_DUMP:
			{
				char text[4096] = {};
				WriteTuneDump(*panel->params, text, sizeof(text));
				return 0;
			}
			case IDC_SAVE:
			{
				char text[4096] = {};
				WriteTuneDump(*panel->params, text, sizeof(text));
				return 0;
			}
			case IDC_LOAD:
				if (LoadTuneFromExeDir(*panel->params))
				{
					SyncAll(*panel);
				}
				return 0;
			default:
				break;
			}
		}
		break;
	case WM_KEYDOWN:
		if (panel && panel->params)
		{
			if (wParam == VK_F9)
			{
				*panel->params = TuneDefaults();
				SyncAll(*panel);
				return 0;
			}
			if (wParam == VK_F8)
			{
				char text[4096] = {};
				WriteTuneDump(*panel->params, text, sizeof(text));
				return 0;
			}
			if (wParam == VK_ESCAPE)
			{
				ShowWindow(hwnd, SW_HIDE);
				return 0;
			}
		}
		break;
	case WM_CLOSE:
		ShowWindow(hwnd, SW_HIDE);
		return 0;
	case WM_DESTROY:
		if (panel)
		{
			panel->hwnd = nullptr;
			panel->tabs = nullptr;
			for (int i = 0; i < TAB_COUNT; ++i)
			{
				panel->pages[i] = nullptr;
			}
		}
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND MakeLabel(HWND parent, HINSTANCE instance, int x, int y, int w, int h, const wchar_t* text, int id = 0)
{
	return CreateWindowW(
		L"STATIC",
		text,
		WS_CHILD | WS_VISIBLE,
		x,
		y,
		w,
		h,
		parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		instance,
		nullptr);
}

HWND MakeTrackbar(HWND parent, HINSTANCE instance, int id, int x, int y, int w, int h, int maxTicks)
{
	HWND bar = CreateWindowW(
		TRACKBAR_CLASSW,
		L"",
		WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ | TBS_NOTICKS,
		x,
		y,
		w,
		h,
		parent,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
		instance,
		nullptr);
	SendMessageW(bar, TBM_SETRANGEMIN, FALSE, 0);
	SendMessageW(bar, TBM_SETRANGEMAX, TRUE, maxTicks);
	SendMessageW(bar, TBM_SETLINESIZE, 0, 1);
	SendMessageW(bar, TBM_SETPAGESIZE, 0, 10);
	return bar;
}

int PopulatePage(HWND page, HINSTANCE instance, int tab)
{
	int y = 8;
	for (const CheckDesc& check : kChecks)
	{
		if (check.tab != tab)
		{
			continue;
		}
		CreateWindowW(
			L"BUTTON",
			check.label,
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			12,
			y,
			360,
			22,
			page,
			reinterpret_cast<HMENU>(static_cast<INT_PTR>(check.id)),
			instance,
			nullptr);
		y += 28;
	}
	for (const ColorDesc& color : kColors)
	{
		if (color.tab != tab)
		{
			continue;
		}
		MakeLabel(page, instance, 12, y + 4, 160, 18, color.label);
		CreateWindowW(
			L"BUTTON",
			L"",
			WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
			180,
			y,
			120,
			22,
			page,
			reinterpret_cast<HMENU>(static_cast<INT_PTR>(color.id)),
			instance,
			nullptr);
		y += 28;
	}
	for (const SliderDesc& slider : kSliders)
	{
		if (slider.tab != tab)
		{
			continue;
		}
		MakeLabel(page, instance, 12, y, 220, 16, slider.label);
		MakeLabel(page, instance, 340, y + 18, 80, 18, L"0.00", slider.valueId);
		const int maxTicks = TuneToTicks(slider.hi, slider.lo, slider.hi);
		MakeTrackbar(page, instance, slider.id, 12, y + 16, 320, 28, maxTicks);
		y += 48;
	}
	if (tab == TAB_COLOUR)
	{
		MakeLabel(page, instance, 12, y, 400, 48, L"Region tint uses Contract A region_id and EO-Map's 11-stop atlas. Off by default so stellar colour stays intact.");
	}
	if (tab == TAB_ISM)
	{
		MakeLabel(
			page,
			instance,
			12,
			y,
			400,
			64,
			L"Radius/Thickness/Edge size the galactic disc. Scale is noise structure, not size. Debug: 0 final, 1 envelope, 2 density, 3 dark lanes, 4 emission, 5 transmittance.");
		y += 68;
	}
	if (tab == TAB_SKY)
	{
		MakeLabel(page, instance, 12, y, 400, 48, L"Background stars are procedural and view-locked. They must not slide with New Eden.");
		y += 52;
	}
	SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS };
	si.nMin = 0;
	si.nMax = y + 8;
	si.nPage = 700;
	si.nPos = 0;
	SetScrollInfo(page, SB_VERT, &si, TRUE);
	return y;
}

LRESULT CALLBACK ColorDrawProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DRAWITEM)
	{
		const DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
		TunePanel* panel = PanelFromHwnd(hwnd);
		if (panel && panel->params && dis)
		{
			if (const ColorDesc* color = FindColor(int(dis->CtlID)))
			{
				HBRUSH brush = CreateSolidBrush(RgbToColor(panel->params->*color->r, panel->params->*color->g, panel->params->*color->b));
				FillRect(dis->hDC, &dis->rcItem, brush);
				DeleteObject(brush);
				DrawEdge(dis->hDC, const_cast<RECT*>(&dis->rcItem), EDGE_SUNKEN, BF_RECT);
				return TRUE;
			}
		}
	}
	return PanelProc(hwnd, msg, wParam, lParam);
}
}

bool TunePanel_Create(HINSTANCE instance, HWND owner, TuneParams* params, TunePanel& out)
{
	out = TunePanel();
	out.params = params;

	INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES };
	InitCommonControlsEx(&icc);

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = ColorDrawProc;
	wc.hInstance = instance;
	wc.lpszClassName = kPanelClass;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	RegisterClassExW(&wc);

	WNDCLASSEXW pageWc = {};
	pageWc.cbSize = sizeof(pageWc);
	pageWc.lpfnWndProc = PageProc;
	pageWc.hInstance = instance;
	pageWc.lpszClassName = kPageClass;
	pageWc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	pageWc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	RegisterClassExW(&pageWc);

	RECT ownerRect = {};
	GetWindowRect(owner, &ownerRect);
	const int width = 500;
	const int height = 860;
	out.hwnd = CreateWindowExW(
		WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
		kPanelClass,
		L"Creator / Visual lab",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
		ownerRect.right + 8,
		ownerRect.top,
		width,
		height,
		owner,
		nullptr,
		instance,
		nullptr);
	if (!out.hwnd)
	{
		return false;
	}
	SetWindowLongPtrW(out.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&out));

	MakeLabel(out.hwnd, instance, 12, 8, 470, 18, L"F8 save+copy   F9 baseline   Load/Save INI next to the exe");

	out.tabs = CreateWindowW(
		WC_TABCONTROLW,
		L"",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		8,
		30,
		476,
		740,
		out.hwnd,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TABS)),
		instance,
		nullptr);

	TCITEMW item = {};
	item.mask = TCIF_TEXT;
	for (int i = 0; i < TAB_COUNT; ++i)
	{
		item.pszText = const_cast<wchar_t*>(kTabNames[i]);
		SendMessageW(out.tabs, TCM_INSERTITEM, i, reinterpret_cast<LPARAM>(&item));
	}

	RECT tabInner = { 8, 30, 484, 770 };
	SendMessageW(out.tabs, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&tabInner));

	for (int i = 0; i < TAB_COUNT; ++i)
	{
		out.pages[i] = CreateWindowExW(
			0,
			kPageClass,
			L"",
			WS_CHILD | (i == 0 ? WS_VISIBLE : 0) | WS_VSCROLL,
			tabInner.left,
			tabInner.top,
			tabInner.right - tabInner.left,
			tabInner.bottom - tabInner.top,
			out.hwnd,
			nullptr,
			instance,
			nullptr);
		PopulatePage(out.pages[i], instance, i);
	}

	CreateWindowW(L"BUTTON", L"Baseline (F9)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 8, 780, 114, 28, out.hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RESET)), instance, nullptr);
	CreateWindowW(L"BUTTON", L"Load INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 128, 780, 86, 28, out.hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOAD)), instance, nullptr);
	CreateWindowW(L"BUTTON", L"Save INI", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 220, 780, 86, 28, out.hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SAVE)), instance, nullptr);
	CreateWindowW(L"BUTTON", L"Copy / F8", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 312, 780, 172, 28, out.hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DUMP)), instance, nullptr);

	SyncAll(out);
	ShowTab(out, 0);
	return true;
}

void TunePanel_Destroy(TunePanel& panel)
{
	if (panel.hwnd)
	{
		DestroyWindow(panel.hwnd);
		panel.hwnd = nullptr;
	}
}

void TunePanel_SyncFromParams(const TunePanel& panel)
{
	SyncAll(panel);
}

void TunePanel_Show(const TunePanel& panel, bool show)
{
	if (panel.hwnd)
	{
		ShowWindow(panel.hwnd, show ? SW_SHOW : SW_HIDE);
	}
}
