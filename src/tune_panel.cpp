#include "tune_panel.h"

#include <commctrl.h>

#include <cstdio>

namespace
{
const wchar_t* kPanelClass = L"eo-map-carbon-tune";

enum ControlId
{
	IDC_STAR_SIZE = 1001,
	IDC_STAR_SIZE_VAL = 1002,
	IDC_STAR_BRIGHT = 1011,
	IDC_STAR_BRIGHT_VAL = 1012,
	IDC_BLOOM_ENABLE = 1021,
	IDC_BLOOM_THRESH = 1031,
	IDC_BLOOM_THRESH_VAL = 1032,
	IDC_BLOOM_STRENGTH = 1041,
	IDC_BLOOM_STRENGTH_VAL = 1042,
	IDC_BLOOM_RADIUS = 1051,
	IDC_BLOOM_RADIUS_VAL = 1052,
	IDC_NEAR_ATTEN = 1061,
	IDC_NEAR_ATTEN_VAL = 1062,
	IDC_FAR_ATTEN = 1071,
	IDC_FAR_ATTEN_VAL = 1072,
	IDC_GATE_OPACITY = 1081,
	IDC_GATE_OPACITY_VAL = 1082,
	IDC_GATE_DIST_ATTEN = 1091,
	IDC_GATE_DIST_ATTEN_VAL = 1092,
	IDC_EXPOSURE = 1101,
	IDC_EXPOSURE_VAL = 1102,
	IDC_RESET = 1201,
	IDC_DUMP = 1202,
};

struct SliderDesc
{
	int id;
	int valueId;
	const wchar_t* label;
	float lo;
	float hi;
	float TuneParams::* field;
};

const SliderDesc kSliders[] = {
	{ IDC_STAR_SIZE, IDC_STAR_SIZE_VAL, L"Star size", 0.10f, 8.00f, &TuneParams::starSize },
	{ IDC_STAR_BRIGHT, IDC_STAR_BRIGHT_VAL, L"Star brightness", 0.00f, 4.00f, &TuneParams::starBrightness },
	{ IDC_BLOOM_THRESH, IDC_BLOOM_THRESH_VAL, L"Bloom threshold", 0.00f, 2.00f, &TuneParams::bloomThreshold },
	{ IDC_BLOOM_STRENGTH, IDC_BLOOM_STRENGTH_VAL, L"Bloom strength", 0.00f, 4.00f, &TuneParams::bloomStrength },
	{ IDC_BLOOM_RADIUS, IDC_BLOOM_RADIUS_VAL, L"Bloom radius", 0.10f, 8.00f, &TuneParams::bloomRadius },
	{ IDC_NEAR_ATTEN, IDC_NEAR_ATTEN_VAL, L"Near-star atten", 0.00f, 4.00f, &TuneParams::nearStarAtten },
	{ IDC_FAR_ATTEN, IDC_FAR_ATTEN_VAL, L"Far-star atten", 0.00f, 4.00f, &TuneParams::farStarAtten },
	{ IDC_GATE_OPACITY, IDC_GATE_OPACITY_VAL, L"Gate opacity", 0.00f, 1.00f, &TuneParams::gateOpacity },
	{ IDC_GATE_DIST_ATTEN, IDC_GATE_DIST_ATTEN_VAL, L"Gate dist atten", 0.00f, 4.00f, &TuneParams::gateDistanceAtten },
	{ IDC_EXPOSURE, IDC_EXPOSURE_VAL, L"Exposure", 0.10f, 4.00f, &TuneParams::exposure },
};

TunePanel* PanelFromHwnd(HWND hwnd)
{
	return reinterpret_cast<TunePanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void SetValueText(HWND panel, int valueId, float v)
{
	wchar_t text[32];
	swprintf_s(text, L"%.2f", v);
	SetDlgItemTextW(panel, valueId, text);
}

void EnableBloomSliders(HWND panel, bool enabled)
{
	EnableWindow(GetDlgItem(panel, IDC_BLOOM_THRESH), enabled);
	EnableWindow(GetDlgItem(panel, IDC_BLOOM_STRENGTH), enabled);
	EnableWindow(GetDlgItem(panel, IDC_BLOOM_RADIUS), enabled);
}

void SyncSlider(HWND panel, const SliderDesc& slider, float value)
{
	const int ticks = TuneToTicks(value, slider.lo, slider.hi);
	SendDlgItemMessageW(panel, slider.id, TBM_SETPOS, TRUE, ticks);
	SetValueText(panel, slider.valueId, TuneFromTicks(ticks, slider.lo, slider.hi));
}

void SyncAll(const TunePanel& panel)
{
	if (!panel.hwnd || !panel.params)
	{
		return;
	}
	for (const SliderDesc& slider : kSliders)
	{
		SyncSlider(panel.hwnd, slider, panel.params->*slider.field);
	}
	SendDlgItemMessageW(panel.hwnd, IDC_BLOOM_ENABLE, BM_SETCHECK, panel.params->bloomEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
	EnableBloomSliders(panel.hwnd, panel.params->bloomEnabled);
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

void Dump(const TunePanel& panel)
{
	if (!panel.params)
	{
		return;
	}
	char text[1024] = {};
	WriteTuneDump(*panel.params, text, sizeof(text));
}

LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	TunePanel* panel = PanelFromHwnd(hwnd);
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
			switch (LOWORD(wParam))
			{
			case IDC_BLOOM_ENABLE:
				panel->params->bloomEnabled = SendDlgItemMessageW(hwnd, IDC_BLOOM_ENABLE, BM_GETCHECK, 0, 0) == BST_CHECKED;
				EnableBloomSliders(hwnd, panel->params->bloomEnabled);
				return 0;
			case IDC_RESET:
				*panel->params = TuneDefaults();
				SyncAll(*panel);
				return 0;
			case IDC_DUMP:
				Dump(*panel);
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
				Dump(*panel);
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
}

bool TunePanel_Create(HINSTANCE instance, HWND owner, TuneParams* params, TunePanel& out)
{
	out = TunePanel();
	out.params = params;

	INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
	InitCommonControlsEx(&icc);

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = PanelProc;
	wc.hInstance = instance;
	wc.lpszClassName = kPanelClass;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	RegisterClassExW(&wc);

	RECT ownerRect = {};
	GetWindowRect(owner, &ownerRect);
	const int width = 440;
	const int height = 760;
	out.hwnd = CreateWindowExW(
		WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
		kPanelClass,
		L"Visual lab",
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

	int y = 10;
	MakeLabel(out.hwnd, instance, 12, y, 400, 18, L"Developer visual tuning  (F8 dump, F9 reset)");
	y += 28;

	CreateWindowW(
		L"BUTTON",
		L"Bloom enabled",
		WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
		12,
		y,
		200,
		22,
		out.hwnd,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_BLOOM_ENABLE)),
		instance,
		nullptr);
	y += 30;

	for (const SliderDesc& slider : kSliders)
	{
		MakeLabel(out.hwnd, instance, 12, y, 200, 16, slider.label);
		MakeLabel(out.hwnd, instance, 320, y + 18, 88, 18, L"0.00", slider.valueId);
		const int maxTicks = TuneToTicks(slider.hi, slider.lo, slider.hi);
		MakeTrackbar(out.hwnd, instance, slider.id, 12, y + 16, 300, 28, maxTicks);
		y += 50;
	}

	CreateWindowW(
		L"BUTTON",
		L"Reset to defaults",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		12,
		y,
		190,
		28,
		out.hwnd,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RESET)),
		instance,
		nullptr);
	CreateWindowW(
		L"BUTTON",
		L"Print / copy settings",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		214,
		y,
		190,
		28,
		out.hwnd,
		reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DUMP)),
		instance,
		nullptr);

	SyncAll(out);
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
