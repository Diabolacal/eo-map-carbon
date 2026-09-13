// New Eden visual lab (TrinityAL DX11).
// Systems: instanced camera-facing discs. Gates: TOP_LINES with distance fade.
// Bloom: host TrinityAL HDR extract / blur / composite. Tuning: Win32 panel.

#include <Windows.h>
#include <windowsx.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

typedef HWND Tr2WindowHandle;

#include <TrinityAL.h>

#include "new_eden_anchors.h"
#include "new_eden_catalog.h"
#include "new_eden_gates.h"
#include "new_eden_regions.h"
#include "new_eden_star_visuals.h"
#include "orbit_camera.h"
#include "star_color.h"
#include "tune_panel.h"
#include "tune_params.h"
#include "visual_lab_math.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <unordered_map>
#include <vector>

using namespace Tr2RenderContextEnum;

const char* g_moduleName = "eo-map-carbon-neweden";

namespace
{
const wchar_t* kWindowClass = L"eo-map-carbon-neweden";
const wchar_t* kWindowTitle = L"EO-Map Carbon New Eden Creator Mode (TrinityAL DX11)";
const uint32_t kDefaultWidth = 1280;
const uint32_t kDefaultHeight = 720;
const uint32_t kSmokeFrames = 60;
const uint32_t kSmokeBloomOnFrames = 40;
const uint32_t kClearArgb = 0xff020208;
const float kMinPx = 2.0f;
const float kMaxPx = 18.0f;
const float kRefDistanceLy = 165.0f;
const char* kDatasetId = "map_data_eo_3464040.db builder=1.5.0 SDE=3464040";

struct StarVertex
{
	float x;
	float y;
	float z;
	float intensity;
};

struct StarInstance
{
	float x;
	float y;
	float z;
	float size;
	float r;
	float g;
	float b;
	float emissive;
	float regionR;
	float regionG;
	float regionB;
	float pad;
};

struct FrameConstants
{
	orbit::Mat4 viewProj;
	float viewRight[4]; // xyz = camera right, w = star colour saturation
	float viewUp[4];
	float cameraPos[4];
	float sizeParams[4];
	float fadeParams[4];
	float gateParams[4];
	float extraParams[4]; // x=depthDesat, y=regionMix
	float gateTint[4];
};

struct BloomConstants
{
	float threshold;
	float strength;
	float radius;
	float exposure;
	float texelX;
	float texelY;
	float blurDirX;
	float blurDirY;
	float saturation;
	float contrast;
	float blackLevel;
	float gamma;
	float vignette;
	float bloomTintR;
	float bloomTintG;
	float bloomTintB;
	float ismStarExt;
	float ismMinT;
	float ismRedden;
	float ismEnable;
};

struct SkyConstants
{
	float viewRight[4];
	float viewUp[4];
	float viewFwd[4];
	float cameraPos[4];
	float cool[4];
	float warm[4];
	float stars[4];
	float baseCol[4];
};

struct IsmConstants
{
	float viewRight[4];
	float viewUp[4];
	float viewFwd[4];
	float cameraPos[4];
	float centre[4];
	float envelope[4];
	float field[4];
	float extinct[4];
	float emission[4];
	float lanes[4];
	float mixp[4];
	float cool[4];
	float warm[4];
	float high[4];
};

struct GlowConstants
{
	float glow[4];
	float flare[4];
};

static_assert(sizeof(FrameConstants) == 192, "FrameConstants must stay 16-byte aligned");
static_assert(sizeof(BloomConstants) == 80, "BloomConstants must stay 16-byte aligned");
static_assert(sizeof(SkyConstants) == 128, "SkyConstants must stay 16-byte aligned");
static_assert(sizeof(IsmConstants) == 224, "IsmConstants must stay 16-byte aligned");
static_assert(sizeof(GlowConstants) == 32, "GlowConstants must stay 16-byte aligned");
static_assert(sizeof(StarInstance) == 48, "StarInstance must stay 16-byte aligned");

enum class DragMode
{
	None,
	Orbit,
	Pan,
};

struct Offscreen
{
	Tr2TextureAL scene;
	Tr2TextureAL extract;
	Tr2TextureAL blurA;
	Tr2TextureAL blurB;
	Tr2TextureAL dummy;
	Tr2TextureAL ism;
	Tr2ResourceSetAL extractSet;
	Tr2ResourceSetAL blurFromExtract;
	Tr2ResourceSetAL blurFromA;
	Tr2ResourceSetAL ismFromBloom;
	Tr2ResourceSetAL ismFromDummy;
	Tr2ResourceSetAL compositeOn;
	Tr2ResourceSetAL compositeOff;
	Tr2ResourceSetAL unbind;
	uint32_t width = 0;
	uint32_t height = 0;
};

struct HostState
{
	orbit::Camera camera;
	DragMode drag = DragMode::None;
	int lastMouseX = 0;
	int lastMouseY = 0;
	uint32_t width = kDefaultWidth;
	uint32_t height = kDefaultHeight;
	bool resizePending = false;
	bool minimized = false;
	bool debugPoints = false;
	TuneParams tune;
	TunePanel panel;
	Tr2PrimaryRenderContextAL* renderContext = nullptr;
	Tr2PresentParametersAL* presentParameters = nullptr;
	Tr2TextureAL* depthBuffer = nullptr;
	Offscreen* offscreen = nullptr;
	Tr2SamplerStateAL* sampler = nullptr;
	Tr2ShaderProgramAL* extractProgram = nullptr;
	Tr2ShaderProgramAL* blurProgram = nullptr;
	Tr2ShaderProgramAL* compositeProgram = nullptr;
	Tr2ShaderProgramAL* ismProgram = nullptr;
};

FILE* g_logFile = nullptr;

void Log(FILE* stream, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	std::vfprintf(stream, fmt, ap);
	va_end(ap);
	std::fflush(stream);
	if (g_logFile && g_logFile != stream)
	{
		va_start(ap, fmt);
		std::vfprintf(g_logFile, fmt, ap);
		va_end(ap);
		std::fflush(g_logFile);
	}
}

void OpenSmokeLog()
{
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring logPath(exePath);
	const auto slash = logPath.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
	{
		logPath.erase(slash + 1);
	}
	logPath += L"eo-map-carbon-neweden-smoke.log";
	_wfopen_s(&g_logFile, logPath.c_str(), L"w");
}

void CloseSmokeLog()
{
	if (g_logFile)
	{
		std::fclose(g_logFile);
		g_logFile = nullptr;
	}
}

void LogFail(const char* what, const ALResult& result)
{
	Log(stderr, "FAILED %s hr=0x%08lx\n", what, static_cast<unsigned long>(result.GetResult()));
}

bool Failed(const char* what, const ALResult& result)
{
	if (FAILED(result))
	{
		LogFail(what, result);
		return true;
	}
	return false;
}

void DumpTune(const TuneParams& tune)
{
	char text[4096] = {};
	const bool wrote = WriteTuneDump(tune, text, sizeof(text));
	Log(stdout, "%s", text);
	Log(stdout, wrote ? "wrote eo-map-carbon-neweden-tune.ini next to the exe\n" : "tune dump file write failed\n");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto* state = reinterpret_cast<HostState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_LBUTTONDOWN:
		if (state)
		{
			state->drag = DragMode::Orbit;
			state->lastMouseX = GET_X_LPARAM(lParam);
			state->lastMouseY = GET_Y_LPARAM(lParam);
			SetCapture(hwnd);
		}
		return 0;
	case WM_RBUTTONDOWN:
		if (state)
		{
			state->drag = DragMode::Pan;
			state->lastMouseX = GET_X_LPARAM(lParam);
			state->lastMouseY = GET_Y_LPARAM(lParam);
			SetCapture(hwnd);
		}
		return 0;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		if (state)
		{
			const bool left = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
			const bool right = (GetKeyState(VK_RBUTTON) & 0x8000) != 0;
			if (right)
			{
				state->drag = DragMode::Pan;
			}
			else if (left)
			{
				state->drag = DragMode::Orbit;
			}
			else
			{
				state->drag = DragMode::None;
				ReleaseCapture();
			}
		}
		return 0;
	case WM_CAPTURECHANGED:
		if (state && reinterpret_cast<HWND>(lParam) != hwnd)
		{
			state->drag = DragMode::None;
		}
		return 0;
	case WM_CONTEXTMENU:
		return 0;
	case WM_MOUSEMOVE:
		if (state && state->drag != DragMode::None)
		{
			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			const float dx = float(x - state->lastMouseX);
			const float dy = float(y - state->lastMouseY);
			if (state->drag == DragMode::Orbit)
			{
				state->camera.Orbit(dx, dy);
			}
			else if (state->drag == DragMode::Pan)
			{
				state->camera.Pan(dx, dy, state->height);
			}
			state->lastMouseX = x;
			state->lastMouseY = y;
		}
		return 0;
	case WM_MOUSEWHEEL:
		if (state)
		{
			state->camera.Zoom(GET_WHEEL_DELTA_WPARAM(wParam));
		}
		return 0;
	case WM_KEYDOWN:
		if (state)
		{
			if (wParam == VK_F1)
			{
				TunePanel_Show(state->panel, true);
				return 0;
			}
			if (wParam == VK_F7)
			{
				state->debugPoints = !state->debugPoints;
				Log(stdout, "debug 1-pixel points: %s\n", state->debugPoints ? "on" : "off");
				return 0;
			}
			if (wParam == VK_F8)
			{
				DumpTune(state->tune);
				return 0;
			}
			if (wParam == VK_F9)
			{
				state->tune = TuneDefaults();
				TunePanel_SyncFromParams(state->panel);
				Log(stdout, "visual lab reset to defaults\n");
				return 0;
			}
		}
		return 0;
	case WM_SIZE:
		if (state)
		{
			state->minimized = (wParam == SIZE_MINIMIZED);
			if (!state->minimized)
			{
				const uint32_t w = LOWORD(lParam);
				const uint32_t h = HIWORD(lParam);
				if (w > 0 && h > 0 && (w != state->width || h != state->height))
				{
					state->width = w;
					state->height = h;
					state->resizePending = true;
				}
			}
		}
		return 0;
	default:
		break;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND CreateHostWindow(HINSTANCE instance)
{
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = instance;
	wc.lpszClassName = kWindowClass;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClassExW(&wc);

	DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	RECT rect = { 0, 0, (LONG)kDefaultWidth, (LONG)kDefaultHeight };
	AdjustWindowRect(&rect, style, FALSE);

	HWND hwnd = CreateWindowW(
		kWindowClass,
		kWindowTitle,
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		instance,
		nullptr);
	if (hwnd)
	{
		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);
	}
	return hwnd;
}

bool PumpMessages()
{
	MSG msg;
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			return false;
		}
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return true;
}

std::vector<StarVertex> MakeSystemVertices(const neweden::Catalog& catalog)
{
	std::vector<StarVertex> stars;
	stars.reserve(catalog.systems.size());
	for (const neweden::System& system : catalog.systems)
	{
		stars.push_back({ system.sceneX, system.sceneY, system.sceneZ, 1.0f });
	}
	return stars;
}

std::vector<StarInstance> MakeStarInstances(
	const neweden::Catalog& catalog,
	const neweden::StarVisuals& visuals,
	const neweden::RegionTable& regions)
{
	std::vector<StarInstance> instances;
	instances.reserve(catalog.systems.size());
	for (size_t i = 0; i < catalog.systems.size(); ++i)
	{
		const neweden::System& system = catalog.systems[i];
		const neweden::StarVisual& visual = visuals.records[i];
		const starcolor::Rgb rgb = starcolor::TemperatureRgb(visual.temperatureK);
		const neweden::Rgb region = neweden::RegionAtlasColor(regions.records[i].regionId);
		StarInstance instance = {};
		instance.x = system.sceneX;
		instance.y = system.sceneY;
		instance.z = system.sceneZ;
		instance.size = starcolor::AnchorSize(system.id);
		instance.r = rgb.r;
		instance.g = rgb.g;
		instance.b = rgb.b;
		instance.emissive = starcolor::EmissiveForTemperature(visual.temperatureK);
		instance.regionR = region.r;
		instance.regionG = region.g;
		instance.regionB = region.b;
		instances.push_back(instance);
	}
	return instances;
}

std::vector<StarVertex> MakeGateVertices(const neweden::Catalog& catalog, const neweden::Graph& graph)
{
	std::unordered_map<uint32_t, const neweden::System*> byId;
	byId.reserve(catalog.systems.size());
	for (const neweden::System& system : catalog.systems)
	{
		byId.emplace(system.id, &system);
	}

	std::vector<StarVertex> lines;
	lines.reserve(graph.edges.size() * 2);
	for (const neweden::Edge& edge : graph.edges)
	{
		const neweden::System* source = byId.at(edge.sourceId);
		const neweden::System* dest = byId.at(edge.destId);
		lines.push_back({ source->sceneX, source->sceneY, source->sceneZ, 1.0f });
		lines.push_back({ dest->sceneX, dest->sceneY, dest->sceneZ, 1.0f });
	}
	return lines;
}

bool CreateDepthBuffer(Tr2PrimaryRenderContextAL& renderContext, Tr2TextureAL& depthBuffer, uint32_t width, uint32_t height)
{
	depthBuffer = Tr2TextureAL();
	return !Failed(
		"Create depth buffer",
		depthBuffer.Create(
			Tr2BitmapDimensions(width, height, 1, PIXEL_FORMAT_D24_UNORM_S8_UINT),
			Tr2GpuUsage::DEPTH_STENCIL,
			renderContext));
}

bool CreateColorTarget(Tr2PrimaryRenderContextAL& renderContext, Tr2TextureAL& texture, uint32_t width, uint32_t height)
{
	texture = Tr2TextureAL();
	return !Failed(
		"Create color target",
		texture.Create(
			Tr2BitmapDimensions(width, height, 1, PIXEL_FORMAT_R16G16B16A16_FLOAT),
			Tr2GpuUsage::RENDER_TARGET | Tr2GpuUsage::SHADER_RESOURCE,
			renderContext));
}

bool MakeSingleSrvSet(
	Tr2ResourceSetAL& set,
	const Tr2ShaderProgramAL& program,
	const Tr2TextureAL& texture,
	const Tr2SamplerStateAL& sampler,
	Tr2PrimaryRenderContextAL& renderContext,
	const char* what)
{
	Tr2ResourceSetDescriptionAL desc(program);
	if (!desc.SetSrv(PIXEL_SHADER, 0, texture) || !desc.SetSampler(PIXEL_SHADER, 0, sampler))
	{
		Log(stderr, "FAILED %s: SetSrv/SetSampler\n", what);
		return false;
	}
	set = Tr2ResourceSetAL();
	return !Failed(what, set.Create(desc, program, renderContext));
}

bool MakeDualSrvSet(
	Tr2ResourceSetAL& set,
	const Tr2ShaderProgramAL& program,
	const Tr2TextureAL& scene,
	const Tr2TextureAL& bloom,
	const Tr2SamplerStateAL& sampler,
	Tr2PrimaryRenderContextAL& renderContext,
	const char* what)
{
	Tr2ResourceSetDescriptionAL desc(program);
	if (!desc.SetSrv(PIXEL_SHADER, 0, scene) || !desc.SetSrv(PIXEL_SHADER, 1, bloom) || !desc.SetSampler(PIXEL_SHADER, 0, sampler))
	{
		Log(stderr, "FAILED %s: SetSrv/SetSampler\n", what);
		return false;
	}
	set = Tr2ResourceSetAL();
	return !Failed(what, set.Create(desc, program, renderContext));
}

bool MakeTripleSrvSet(
	Tr2ResourceSetAL& set,
	const Tr2ShaderProgramAL& program,
	const Tr2TextureAL& scene,
	const Tr2TextureAL& bloom,
	const Tr2TextureAL& ism,
	const Tr2SamplerStateAL& sampler,
	Tr2PrimaryRenderContextAL& renderContext,
	const char* what)
{
	Tr2ResourceSetDescriptionAL desc(program);
	if (!desc.SetSrv(PIXEL_SHADER, 0, scene) || !desc.SetSrv(PIXEL_SHADER, 1, bloom) ||
		!desc.SetSrv(PIXEL_SHADER, 2, ism) || !desc.SetSampler(PIXEL_SHADER, 0, sampler))
	{
		Log(stderr, "FAILED %s: SetSrv/SetSampler\n", what);
		return false;
	}
	set = Tr2ResourceSetAL();
	return !Failed(what, set.Create(desc, program, renderContext));
}

bool RecreateOffscreen(HostState& state)
{
	if (!state.renderContext || !state.offscreen || !state.sampler || !state.extractProgram || !state.blurProgram ||
		!state.compositeProgram || !state.ismProgram)
	{
		return false;
	}
	Offscreen& off = *state.offscreen;
	const uint32_t w = state.width;
	const uint32_t h = state.height;
	const uint32_t hw = (std::max)(1u, w / 2);
	const uint32_t hh = (std::max)(1u, h / 2);
	if (!CreateColorTarget(*state.renderContext, off.scene, w, h) ||
		!CreateColorTarget(*state.renderContext, off.extract, hw, hh) ||
		!CreateColorTarget(*state.renderContext, off.blurA, hw, hh) ||
		!CreateColorTarget(*state.renderContext, off.blurB, hw, hh) ||
		!CreateColorTarget(*state.renderContext, off.ism, hw, hh))
	{
		return false;
	}
	if (!off.dummy.IsValid())
	{
		if (!CreateColorTarget(*state.renderContext, off.dummy, 1, 1))
		{
			return false;
		}
		if (Failed("Set dummy RT", state.renderContext->SetRenderTarget(off.dummy)) ||
			Failed("Clear dummy", state.renderContext->Clear(CLEARFLAGS_TARGET, 0xff000000, 1.0f)))
		{
			return false;
		}
	}
	if (!MakeSingleSrvSet(off.extractSet, *state.extractProgram, off.scene, *state.sampler, *state.renderContext, "extract resource set") ||
		!MakeSingleSrvSet(off.blurFromExtract, *state.blurProgram, off.extract, *state.sampler, *state.renderContext, "blur-from-extract set") ||
		!MakeSingleSrvSet(off.blurFromA, *state.blurProgram, off.blurA, *state.sampler, *state.renderContext, "blur-from-A set") ||
		!MakeSingleSrvSet(off.ismFromBloom, *state.ismProgram, off.blurB, *state.sampler, *state.renderContext, "ism-from-bloom set") ||
		!MakeSingleSrvSet(off.ismFromDummy, *state.ismProgram, off.dummy, *state.sampler, *state.renderContext, "ism-from-dummy set") ||
		!MakeTripleSrvSet(off.compositeOn, *state.compositeProgram, off.scene, off.blurB, off.ism, *state.sampler, *state.renderContext, "composite-on set") ||
		!MakeTripleSrvSet(off.compositeOff, *state.compositeProgram, off.scene, off.dummy, off.ism, *state.sampler, *state.renderContext, "composite-off set") ||
		!MakeTripleSrvSet(off.unbind, *state.compositeProgram, off.dummy, off.dummy, off.dummy, *state.sampler, *state.renderContext, "unbind set"))
	{
		return false;
	}
	off.width = w;
	off.height = h;
	return true;
}

bool ApplyResize(HostState& state)
{
	if (!state.renderContext || !state.presentParameters || !state.depthBuffer)
	{
		return true;
	}
	if (state.width == 0 || state.height == 0)
	{
		return true;
	}

	state.presentParameters->mode.width = state.width;
	state.presentParameters->mode.height = state.height;
	if (Failed("SetPresentParameters", state.renderContext->SetPresentParameters(0, *state.presentParameters)))
	{
		return false;
	}

	const auto& backBuffer = state.renderContext->GetDefaultBackBuffer();
	if (!CreateDepthBuffer(*state.renderContext, *state.depthBuffer, backBuffer.GetWidth(), backBuffer.GetHeight()))
	{
		return false;
	}
	if (!RecreateOffscreen(state))
	{
		return false;
	}
	if (Failed("SetDepthStencil after resize", state.renderContext->SetDepthStencil(*state.depthBuffer)))
	{
		return false;
	}

	Log(stdout, "resized swap chain to %ux%u\n", state.width, state.height);
	return true;
}

bool UpdateFrameConstants(
	Tr2ConstantBufferAL& cb,
	Tr2PrimaryRenderContextAL& renderContext,
	const HostState& state)
{
	const float aspect = (state.height > 0) ? (float(state.width) / float(state.height)) : 1.0f;
	const orbit::Vec3 eye = state.camera.Eye();
	const orbit::Vec3 back = orbit::Normalize(orbit::Sub(eye, state.camera.target));
	const orbit::Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
	const orbit::Vec3 right = orbit::Normalize(orbit::Cross(worldUp, back));
	const orbit::Vec3 up = orbit::Cross(back, right);

	FrameConstants* data = nullptr;
	if (Failed("Lock frame constants", cb.Lock(reinterpret_cast<void**>(&data), renderContext)))
	{
		return false;
	}
	data->viewProj = state.camera.ViewProjection(aspect);
	data->viewRight[0] = right.x;
	data->viewRight[1] = right.y;
	data->viewRight[2] = right.z;
	data->viewRight[3] = state.tune.starSaturation;
	data->viewUp[0] = up.x;
	data->viewUp[1] = up.y;
	data->viewUp[2] = up.z;
	data->viewUp[3] = 0.0f;
	data->cameraPos[0] = eye.x;
	data->cameraPos[1] = eye.y;
	data->cameraPos[2] = eye.z;
	data->cameraPos[3] = state.camera.distance;
	data->sizeParams[0] = state.tune.starSize;
	data->sizeParams[1] = kMinPx;
	data->sizeParams[2] = kMaxPx;
	data->sizeParams[3] = tanf(state.camera.fovY * 0.5f);
	data->fadeParams[0] = state.tune.nearStarAtten;
	data->fadeParams[1] = state.tune.farStarAtten;
	data->fadeParams[2] = kRefDistanceLy;
	data->fadeParams[3] = state.tune.starBrightness;
	data->gateParams[0] = state.tune.gateOpacity;
	data->gateParams[1] = state.tune.gateDistanceAtten;
	data->gateParams[2] = float(state.width);
	data->gateParams[3] = float(state.height);
	data->extraParams[0] = state.tune.starDepthDesat;
	data->extraParams[1] = state.tune.regionEnabled ? state.tune.regionStarMix : 0.0f;
	data->extraParams[2] = 0.0f;
	data->extraParams[3] = 0.0f;
	data->gateTint[0] = state.tune.gateTintR;
	data->gateTint[1] = state.tune.gateTintG;
	data->gateTint[2] = state.tune.gateTintB;
	data->gateTint[3] = 0.0f;
	if (Failed("Unlock frame constants", cb.Unlock(renderContext)))
	{
		return false;
	}
	return true;
}

bool UpdateBloomConstants(
	Tr2ConstantBufferAL& cb,
	Tr2PrimaryRenderContextAL& renderContext,
	const TuneParams& tune,
	float texelX,
	float texelY,
	float dirX,
	float dirY,
	float strength)
{
	BloomConstants* data = nullptr;
	if (Failed("Lock bloom constants", cb.Lock(reinterpret_cast<void**>(&data), renderContext)))
	{
		return false;
	}
	data->threshold = tune.bloomThreshold;
	data->strength = strength;
	data->radius = tune.bloomRadius;
	data->exposure = tune.exposure;
	data->texelX = texelX;
	data->texelY = texelY;
	data->blurDirX = dirX;
	data->blurDirY = dirY;
	data->saturation = tune.saturation;
	data->contrast = tune.contrast;
	data->blackLevel = tune.blackLevel;
	data->gamma = tune.gamma;
	data->vignette = tune.vignette;
	data->bloomTintR = tune.bloomTintR;
	data->bloomTintG = tune.bloomTintG;
	data->bloomTintB = tune.bloomTintB;
	data->ismStarExt = tune.ismStarExt;
	data->ismMinT = tune.ismMinT;
	data->ismRedden = tune.ismRedden;
	data->ismEnable = tune.ismEnabled ? 1.0f : 0.0f;
	if (Failed("Unlock bloom constants", cb.Unlock(renderContext)))
	{
		return false;
	}
	return true;
}

bool FillViewBasis(const HostState& state, float right[4], float up[4], float fwd[4], float cam[4])
{
	const float aspect = (state.height > 0) ? (float(state.width) / float(state.height)) : 1.0f;
	const orbit::Vec3 eye = state.camera.Eye();
	const orbit::Vec3 back = orbit::Normalize(orbit::Sub(eye, state.camera.target));
	const orbit::Vec3 worldUp = { 0.0f, 1.0f, 0.0f };
	const orbit::Vec3 r = orbit::Normalize(orbit::Cross(worldUp, back));
	const orbit::Vec3 u = orbit::Cross(back, r);
	const orbit::Vec3 f = { -back.x, -back.y, -back.z };
	right[0] = r.x;
	right[1] = r.y;
	right[2] = r.z;
	right[3] = aspect;
	up[0] = u.x;
	up[1] = u.y;
	up[2] = u.z;
	up[3] = tanf(state.camera.fovY * 0.5f);
	fwd[0] = f.x;
	fwd[1] = f.y;
	fwd[2] = f.z;
	fwd[3] = 0.0f;
	cam[0] = eye.x;
	cam[1] = eye.y;
	cam[2] = eye.z;
	cam[3] = 0.0f;
	return true;
}

bool UpdateSkyConstants(Tr2ConstantBufferAL& cb, Tr2PrimaryRenderContextAL& renderContext, const HostState& state)
{
	SkyConstants* data = nullptr;
	if (Failed("Lock sky constants", cb.Lock(reinterpret_cast<void**>(&data), renderContext)))
	{
		return false;
	}
	FillViewBasis(state, data->viewRight, data->viewUp, data->viewFwd, data->cameraPos);
	data->viewFwd[3] = state.tune.skyIntensity;
	data->cool[0] = state.tune.skyCoolR;
	data->cool[1] = state.tune.skyCoolG;
	data->cool[2] = state.tune.skyCoolB;
	data->cool[3] = state.tune.skyContrast;
	data->warm[0] = state.tune.skyWarmR;
	data->warm[1] = state.tune.skyWarmG;
	data->warm[2] = state.tune.skyWarmB;
	data->warm[3] = state.tune.skyBand;
	data->stars[0] = state.tune.skyStarAmount;
	data->stars[1] = state.tune.skyStarBright;
	data->stars[2] = 0.0f;
	data->stars[3] = 0.0f;
	data->baseCol[0] = state.tune.skyBaseR;
	data->baseCol[1] = state.tune.skyBaseG;
	data->baseCol[2] = state.tune.skyBaseB;
	data->baseCol[3] = 0.0f;
	return !Failed("Unlock sky constants", cb.Unlock(renderContext));
}

bool UpdateIsmConstants(Tr2ConstantBufferAL& cb, Tr2PrimaryRenderContextAL& renderContext, const HostState& state)
{
	IsmConstants* data = nullptr;
	if (Failed("Lock ism constants", cb.Lock(reinterpret_cast<void**>(&data), renderContext)))
	{
		return false;
	}
	FillViewBasis(state, data->viewRight, data->viewUp, data->viewFwd, data->cameraPos);
	data->viewFwd[3] = state.camera.distance;
	data->cameraPos[3] = state.tune.ismNearCut;
	data->centre[0] = neweden::kCentreX;
	data->centre[1] = neweden::kCentreY;
	data->centre[2] = neweden::kCentreZ;
	data->centre[3] = 46.0f;
	data->envelope[0] = 22.0f;
	data->envelope[1] = 6.0f;
	data->envelope[2] = 5.5f;
	data->envelope[3] = 0.16f;
	data->field[0] = state.tune.ismDensity;
	data->field[1] = state.tune.ismContrast;
	data->field[2] = state.tune.ismDetail;
	data->field[3] = state.tune.ismScale;
	data->extinct[0] = 0.030f;
	data->extinct[1] = 2.20f;
	data->extinct[2] = state.tune.ismRedden;
	data->extinct[3] = 0.0f;
	data->emission[0] = state.tune.ismEmission;
	data->emission[1] = 0.055f;
	data->emission[2] = 0.0f;
	data->emission[3] = state.tune.ismScatter;
	data->lanes[0] = state.tune.ismDarkLane;
	data->lanes[1] = state.tune.ismDarkScale;
	data->lanes[2] = state.tune.ismLightLane;
	data->lanes[3] = state.tune.ismLightScale;
	data->mixp[0] = state.tune.ismStarExt;
	data->mixp[1] = state.tune.ismMinT;
	data->mixp[2] = state.tune.ismSteps;
	data->mixp[3] = state.tune.regionEnabled ? state.tune.regionIsmMix : 0.0f;
	data->cool[0] = state.tune.ismPrimaryR;
	data->cool[1] = state.tune.ismPrimaryG;
	data->cool[2] = state.tune.ismPrimaryB;
	data->cool[3] = 0.0f;
	data->warm[0] = state.tune.ismSecondaryR;
	data->warm[1] = state.tune.ismSecondaryG;
	data->warm[2] = state.tune.ismSecondaryB;
	data->warm[3] = 0.0f;
	data->high[0] = state.tune.ismHighlightR;
	data->high[1] = state.tune.ismHighlightG;
	data->high[2] = state.tune.ismHighlightB;
	data->high[3] = 0.0f;
	return !Failed("Unlock ism constants", cb.Unlock(renderContext));
}

bool UpdateGlowConstants(Tr2ConstantBufferAL& cb, Tr2PrimaryRenderContextAL& renderContext, const TuneParams& tune)
{
	GlowConstants* data = nullptr;
	if (Failed("Lock glow constants", cb.Lock(reinterpret_cast<void**>(&data), renderContext)))
	{
		return false;
	}
	data->glow[0] = tune.glowIntensity;
	data->glow[1] = tune.glowScale;
	data->glow[2] = tune.glowThreshold;
	data->glow[3] = 0.0f;
	data->flare[0] = tune.flareIntensity;
	data->flare[1] = tune.flareThreshold;
	data->flare[2] = tune.flareLength;
	data->flare[3] = tune.flareChroma;
	return !Failed("Unlock glow constants", cb.Unlock(renderContext));
}

bool DrawFullscreen(
	Tr2PrimaryRenderContextAL& renderContext,
	Tr2VertexLayoutAL& layout,
	Tr2BufferAL& vb,
	uint32_t stride,
	Tr2ShaderProgramAL& program,
	Tr2ResourceSetAL& resources,
	Tr2ConstantBufferAL& cb)
{
	if (Failed("FS SetVertexLayout", renderContext.SetVertexLayout(layout)) ||
		Failed("FS SetShaderProgram", renderContext.SetShaderProgram(program)) ||
		Failed("FS SetStreamSource", renderContext.SetStreamSource(0, vb, 0, stride)) ||
		Failed("FS SetStreamSource1", renderContext.SetStreamSource(1, Tr2BufferAL(), 0, 0)) ||
		Failed("FS SetTopology", renderContext.SetTopology(TOP_TRIANGLES)) ||
		Failed("FS SetConstants", renderContext.SetConstants(cb, PIXEL_SHADER, 0)) ||
		Failed("FS SetResourceSet", renderContext.SetResourceSet(resources)) ||
		Failed("FS RS_ZENABLE", renderContext.SetRenderState(RS_ZENABLE, 0)) ||
		Failed("FS RS_ZWRITEENABLE", renderContext.SetRenderState(RS_ZWRITEENABLE, 0)) ||
		Failed("FS RS_ALPHABLENDENABLE", renderContext.SetRenderState(RS_ALPHABLENDENABLE, 0)) ||
		Failed("FS RS_CULLMODE", renderContext.SetRenderState(RS_CULLMODE, CULLMODE_NONE)) ||
		Failed("FS DrawPrimitive", renderContext.DrawPrimitive(0, 2)))
	{
		return false;
	}
	return true;
}

bool DrawFullscreenNoSrv(
	Tr2PrimaryRenderContextAL& renderContext,
	Tr2VertexLayoutAL& layout,
	Tr2BufferAL& vb,
	uint32_t stride,
	Tr2ShaderProgramAL& program,
	Tr2ConstantBufferAL& cb)
{
	if (Failed("FSNS SetVertexLayout", renderContext.SetVertexLayout(layout)) ||
		Failed("FSNS SetShaderProgram", renderContext.SetShaderProgram(program)) ||
		Failed("FSNS SetStreamSource", renderContext.SetStreamSource(0, vb, 0, stride)) ||
		Failed("FSNS SetStreamSource1", renderContext.SetStreamSource(1, Tr2BufferAL(), 0, 0)) ||
		Failed("FSNS SetTopology", renderContext.SetTopology(TOP_TRIANGLES)) ||
		Failed("FSNS SetConstants", renderContext.SetConstants(cb, PIXEL_SHADER, 0)) ||
		Failed("FSNS RS_ZENABLE", renderContext.SetRenderState(RS_ZENABLE, 0)) ||
		Failed("FSNS RS_ZWRITEENABLE", renderContext.SetRenderState(RS_ZWRITEENABLE, 0)) ||
		Failed("FSNS RS_ALPHABLENDENABLE", renderContext.SetRenderState(RS_ALPHABLENDENABLE, 0)) ||
		Failed("FSNS RS_CULLMODE", renderContext.SetRenderState(RS_CULLMODE, CULLMODE_NONE)) ||
		Failed("FSNS DrawPrimitive", renderContext.DrawPrimitive(0, 2)))
	{
		return false;
	}
	return true;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int)
{
	const bool smoke = cmdLine && wcsstr(cmdLine, L"--smoke");
	const bool startBloomOff = cmdLine && wcsstr(cmdLine, L"--bloom-off");
	const bool startPoints = cmdLine && wcsstr(cmdLine, L"--points");
	if (smoke)
	{
		OpenSmokeLog();
	}
	else
	{
		AllocConsole();
		FILE* unused = nullptr;
		freopen_s(&unused, "CONOUT$", "w", stdout);
		freopen_s(&unused, "CONOUT$", "w", stderr);
	}

	Log(stdout, "eo-map-carbon-neweden starting\n");
	Log(stdout, "renderer: TrinityAL DX11\n");
	Log(stdout, "path: instanced star quads + TOP_LINES + Creator Mode TrinityAL passes\n");

	neweden::Catalog catalog;
	std::string catalogError;
	if (!neweden::FindAndLoadCatalog(catalog, catalogError))
	{
		Log(stderr, "FAILED load New Eden catalogue: %s\n", catalogError.c_str());
		CloseSmokeLog();
		return 1;
	}
	if (!neweden::ValidateAnchors(catalog, catalogError))
	{
		Log(stderr, "FAILED New Eden anchor check: %s\n", catalogError.c_str());
		CloseSmokeLog();
		return 1;
	}
	neweden::Graph graph;
	std::string graphError;
	if (!neweden::FindAndLoadGraph(graph, graphError))
	{
		Log(stderr, "FAILED load New Eden stargate graph: %s\n", graphError.c_str());
		CloseSmokeLog();
		return 1;
	}
	if (!neweden::ValidateGraph(catalog, graph, graphError))
	{
		Log(stderr, "FAILED New Eden stargate check: %s\n", graphError.c_str());
		CloseSmokeLog();
		return 1;
	}
	neweden::StarVisuals visuals;
	std::string visualError;
	if (!neweden::FindAndLoadStarVisuals(visuals, visualError))
	{
		Log(stderr, "FAILED load New Eden star visuals: %s\n", visualError.c_str());
		CloseSmokeLog();
		return 1;
	}
	std::vector<uint32_t> systemIds;
	systemIds.reserve(catalog.systems.size());
	for (const neweden::System& system : catalog.systems)
	{
		systemIds.push_back(system.id);
	}
	if (!neweden::ValidateStarVisuals(visuals, systemIds, visualError))
	{
		Log(stderr, "FAILED New Eden star visual check: %s\n", visualError.c_str());
		CloseSmokeLog();
		return 1;
	}

	const uint32_t systemCount = uint32_t(catalog.systems.size());
	const uint32_t edgeCount = uint32_t(graph.edges.size());
	Log(stdout, "dataset: %s\n", kDatasetId);
	Log(stdout, "dataset file: %s\n", catalog.loadedPath.c_str());
	Log(stdout, "dataset source sha256: %s\n", catalog.sourceSha256.c_str());
	Log(stdout, "system count: %u (known-space %u, other-space %u)\n",
		systemCount,
		catalog.knownSpaceCount,
		catalog.otherSpaceCount);
	Log(stdout, "stargate file: %s\n", graph.loadedPath.c_str());
	Log(stdout, "connection count: %u (undirected; source directed rows=13978)\n", edgeCount);
	Log(stdout, "star visuals file: %s\n", visuals.loadedPath.c_str());
	Log(stdout, "star temperature window: %.0f-%.0f K (Jita=7305 K F)\n", visuals.minTemperatureK, visuals.maxTemperatureK);
	Log(stdout, "anchor check: Jita/Amarr/Dodixie/Rens/Hek scene coordinates match EO-Map transform\n");
	Log(stdout, "graph check: endpoints in catalogue, Jita/Amarr/Dodixie/Rens/Hek/Zarzakh adjacency, Jita-Amarr hops=11, Niarja unreachable\n");
	Log(stdout, "visual check: 5485 temperatures, ids match catalogue, Jita F / 7305 K\n");

	std::string mathError;
	char mathLog[768] = {};
	if (!vislab::ValidateVisualLabMath(mathError, mathLog, sizeof(mathLog)))
	{
		Log(stderr, "FAILED visual lab math: %s\n", mathError.c_str());
		CloseSmokeLog();
		return 1;
	}
	Log(stdout, "%s", mathLog);

	neweden::RegionTable regions;
	std::string regionError;
	if (!neweden::FindAndLoadRegions(regions, regionError))
	{
		Log(stderr, "FAILED load New Eden regions: %s\n", regionError.c_str());
		CloseSmokeLog();
		return 1;
	}
	if (!neweden::ValidateRegions(regions, systemIds, regionError))
	{
		Log(stderr, "FAILED New Eden region check: %s\n", regionError.c_str());
		CloseSmokeLog();
		return 1;
	}

	std::string persistError;
	if (!ValidateTunePersist(persistError))
	{
		Log(stderr, "FAILED tune persist: %s\n", persistError.c_str());
		CloseSmokeLog();
		return 1;
	}
	Log(stdout, "region check: 5485 ids, 70 regions, Jita region_id=10000002\n");
	Log(stdout, "persist check: parse/clamp/unknown-key/roundtrip ok\n");

	unsigned adapterCount = 0;
	if (Failed("GetAdapterCount", Tr2VideoAdapterInfo::GetAdapterCount(adapterCount)) || adapterCount == 0)
	{
		Log(stderr, "No GPU adapter reported by TrinityAL.\n");
		CloseSmokeLog();
		return 1;
	}
	Log(stdout, "adapter count: %u\n", adapterCount);

	HostState state;
	state.debugPoints = startPoints;
	if (!smoke && LoadTuneFromExeDir(state.tune))
	{
		Log(stdout, "loaded creator settings from eo-map-carbon-neweden-tune.ini\n");
	}
	if (startBloomOff)
	{
		state.tune.bloomEnabled = false;
	}
	HWND hwnd = CreateHostWindow(instance);
	if (!hwnd)
	{
		Log(stderr, "CreateWindowW failed (%lu)\n", GetLastError());
		CloseSmokeLog();
		return 1;
	}

	RECT client = {};
	GetClientRect(hwnd, &client);
	if (client.right > 0 && client.bottom > 0)
	{
		state.width = uint32_t(client.right);
		state.height = uint32_t(client.bottom);
	}
	SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
	state.camera.target = { neweden::kCentreX, neweden::kCentreY, neweden::kCentreZ };
	state.camera.yaw = 0.35f;
	state.camera.pitch = 0.62f;
	state.camera.distance = 190.0f;
	state.camera.nearZ = 0.2f;
	state.camera.farZ = 2500.0f;
	Log(stdout, "window hwnd=%p %ux%u\n", hwnd, state.width, state.height);

	Tr2PrimaryRenderContextAL* renderContext = new Tr2PrimaryRenderContextAL();
	Tr2PrimaryRenderContextAL::SetPrimaryRenderContext(renderContext);

	Tr2PresentParametersAL presentParameters = {};
	if (Failed("GetAdapterDisplayMode", Tr2VideoAdapterInfo::GetAdapterDisplayMode(Tr2VideoAdapterInfo::DEFAULT_ADAPTER, presentParameters.mode)))
	{
		CloseSmokeLog();
		return 1;
	}
	presentParameters.mode.width = state.width;
	presentParameters.mode.height = state.height;
	presentParameters.backBufferCount = 1;
	presentParameters.msaaType = 0;
	presentParameters.msaaQuality = 0;
	presentParameters.swapEffect = SWAP_EFFECT_DISCARD;
	presentParameters.outputWindow = hwnd;
	presentParameters.windowed = true;
	presentParameters.software = false;
	presentParameters.presentInterval = smoke ? PRESENT_INTERVAL_IMMEDIATE : PRESENT_INTERVAL_ONE;

	if (Failed("CreateDevice", renderContext->CreateDevice(0, hwnd, presentParameters)))
	{
		CloseSmokeLog();
		return 1;
	}
	if (!renderContext->IsValid())
	{
		Log(stderr, "CreateDevice returned success but render context is not valid\n");
		CloseSmokeLog();
		return 1;
	}
	Log(stdout, "TrinityAL CreateDevice succeeded\n");

	Tr2TextureAL depthBuffer;
	if (!CreateDepthBuffer(*renderContext, depthBuffer, state.width, state.height))
	{
		CloseSmokeLog();
		return 1;
	}
	if (Failed("SetDepthStencil", renderContext->SetDepthStencil(depthBuffer)))
	{
		CloseSmokeLog();
		return 1;
	}

	uint8_t pointVsBytecode[] = {
#include "Starfield_vs.h"
	};
	uint8_t pointPsBytecode[] = {
#include "StarColor_ps.h"
	};
	uint8_t spriteVsBytecode[] = {
#include "StarSprite_vs.h"
	};
	uint8_t spritePsBytecode[] = {
#include "StarSprite_ps.h"
	};
	uint8_t gateVsBytecode[] = {
#include "GateLine_vs.h"
	};
	uint8_t gatePsBytecode[] = {
#include "GateLine_ps.h"
	};
	uint8_t fsVsBytecode[] = {
#include "Fullscreen_vs.h"
	};
	uint8_t extractPsBytecode[] = {
#include "BloomExtract_ps.h"
	};
	uint8_t blurPsBytecode[] = {
#include "BloomBlur_ps.h"
	};
	uint8_t compositePsBytecode[] = {
#include "BloomComposite_ps.h"
	};
	uint8_t skyPsBytecode[] = {
#include "DeepSpace_ps.h"
	};
	uint8_t ismPsBytecode[] = {
#include "IsmField_ps.h"
	};
	uint8_t glowVsBytecode[] = {
#include "StarGlow_vs.h"
	};
	uint8_t glowPsBytecode[] = {
#include "StarGlow_ps.h"
	};
	uint8_t flareVsBytecode[] = {
#include "StarFlare_vs.h"
	};
	uint8_t flarePsBytecode[] = {
#include "StarFlare_ps.h"
	};

	Tr2ShaderAL pointVs;
	auto pointVsInput = Tr2ShaderSignatureAL()
							.Add(Tr2VertexDefinition::POSITION, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 3)
							.Add(Tr2VertexDefinition::TEXCOORD, 0, 1, Tr2ShaderPipelineInputAL::FLOAT, 1)
							.Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	if (Failed("Create point VS", pointVs.Create(VERTEX_SHADER, pointVsBytecode, pointVsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL pointPs;
	if (Failed("Create point PS", pointPs.Create(PIXEL_SHADER, pointPsBytecode, Tr2ShaderSignatureAL(), "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL pointShaders[] = { pointVs, pointPs };
	Tr2ShaderProgramAL pointProgram;
	if (Failed("Create point program", pointProgram.Create(pointShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL spriteVs;
	auto spriteVsInput = Tr2ShaderSignatureAL()
							 .Add(Tr2VertexDefinition::TEXCOORD, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 2)
							 .Add(Tr2VertexDefinition::POSITION, 0, 1, Tr2ShaderPipelineInputAL::FLOAT, 3)
							 .Add(Tr2VertexDefinition::TEXCOORD, 1, 2, Tr2ShaderPipelineInputAL::FLOAT, 1)
							 .Add(Tr2VertexDefinition::COLOR, 0, 3, Tr2ShaderPipelineInputAL::FLOAT, 4)
							 .Add(Tr2VertexDefinition::TEXCOORD, 2, 4, Tr2ShaderPipelineInputAL::FLOAT, 3)
							 .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	if (Failed("Create sprite VS", spriteVs.Create(VERTEX_SHADER, spriteVsBytecode, spriteVsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL spritePs;
	if (Failed("Create sprite PS", spritePs.Create(PIXEL_SHADER, spritePsBytecode, Tr2ShaderSignatureAL(), "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL spriteShaders[] = { spriteVs, spritePs };
	Tr2ShaderProgramAL spriteProgram;
	if (Failed("Create sprite program", spriteProgram.Create(spriteShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL gateVs;
	auto gateVsInput = Tr2ShaderSignatureAL()
						   .Add(Tr2VertexDefinition::POSITION, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 3)
						   .Add(Tr2VertexDefinition::TEXCOORD, 0, 1, Tr2ShaderPipelineInputAL::FLOAT, 1)
						   .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	if (Failed("Create gate VS", gateVs.Create(VERTEX_SHADER, gateVsBytecode, gateVsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL gatePs;
	auto gatePsInput = Tr2ShaderSignatureAL().Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	if (Failed("Create gate PS", gatePs.Create(PIXEL_SHADER, gatePsBytecode, gatePsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL gateShaders[] = { gateVs, gatePs };
	Tr2ShaderProgramAL gateProgram;
	if (Failed("Create gate program", gateProgram.Create(gateShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL fsVs;
	auto fsVsInput = Tr2ShaderSignatureAL()
						 .Add(Tr2VertexDefinition::TEXCOORD, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 2)
						 .Add(Tr2VertexDefinition::POSITION, 0, 1, Tr2ShaderPipelineInputAL::FLOAT, 3);
	if (Failed("Create fullscreen VS", fsVs.Create(VERTEX_SHADER, fsVsBytecode, fsVsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	auto sampleOne = Tr2ShaderSignatureAL()
						 .Add(Tr2ShaderRegisterAL::SRV_TEXTURE2D, 0)
						 .Add(Tr2ShaderRegisterAL::SAMPLER, 0)
						 .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	auto sampleTwo = Tr2ShaderSignatureAL()
						 .Add(Tr2ShaderRegisterAL::SRV_TEXTURE2D, 0)
						 .Add(Tr2ShaderRegisterAL::SRV_TEXTURE2D, 1)
						 .Add(Tr2ShaderRegisterAL::SAMPLER, 0)
						 .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	auto sampleThree = Tr2ShaderSignatureAL()
						   .Add(Tr2ShaderRegisterAL::SRV_TEXTURE2D, 0)
						   .Add(Tr2ShaderRegisterAL::SRV_TEXTURE2D, 1)
						   .Add(Tr2ShaderRegisterAL::SRV_TEXTURE2D, 2)
						   .Add(Tr2ShaderRegisterAL::SAMPLER, 0)
						   .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	auto skyPsInput = Tr2ShaderSignatureAL().Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	auto haloVsInput = Tr2ShaderSignatureAL()
						   .Add(Tr2VertexDefinition::TEXCOORD, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 2)
						   .Add(Tr2VertexDefinition::POSITION, 0, 1, Tr2ShaderPipelineInputAL::FLOAT, 3)
						   .Add(Tr2VertexDefinition::TEXCOORD, 1, 2, Tr2ShaderPipelineInputAL::FLOAT, 1)
						   .Add(Tr2VertexDefinition::COLOR, 0, 3, Tr2ShaderPipelineInputAL::FLOAT, 4)
						   .Add(Tr2VertexDefinition::TEXCOORD, 2, 4, Tr2ShaderPipelineInputAL::FLOAT, 3)
						   .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0)
						   .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 1);
	Tr2ShaderAL extractPs;
	if (Failed("Create extract PS", extractPs.Create(PIXEL_SHADER, extractPsBytecode, sampleOne, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL blurPs;
	if (Failed("Create blur PS", blurPs.Create(PIXEL_SHADER, blurPsBytecode, sampleOne, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL compositePs;
	if (Failed("Create composite PS", compositePs.Create(PIXEL_SHADER, compositePsBytecode, sampleThree, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL extractShaders[] = { fsVs, extractPs };
	Tr2ShaderProgramAL extractProgram;
	if (Failed("Create extract program", extractProgram.Create(extractShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL blurShaders[] = { fsVs, blurPs };
	Tr2ShaderProgramAL blurProgram;
	if (Failed("Create blur program", blurProgram.Create(blurShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL compositeShaders[] = { fsVs, compositePs };
	Tr2ShaderProgramAL compositeProgram;
	if (Failed("Create composite program", compositeProgram.Create(compositeShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL skyPs;
	if (Failed("Create sky PS", skyPs.Create(PIXEL_SHADER, skyPsBytecode, skyPsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL skyShaders[] = { fsVs, skyPs };
	Tr2ShaderProgramAL skyProgram;
	if (Failed("Create sky program", skyProgram.Create(skyShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL ismPs;
	if (Failed("Create ism PS", ismPs.Create(PIXEL_SHADER, ismPsBytecode, sampleOne, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL ismShaders[] = { fsVs, ismPs };
	Tr2ShaderProgramAL ismProgram;
	if (Failed("Create ism program", ismProgram.Create(ismShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL glowVs;
	if (Failed("Create glow VS", glowVs.Create(VERTEX_SHADER, glowVsBytecode, haloVsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL glowPs;
	if (Failed("Create glow PS", glowPs.Create(PIXEL_SHADER, glowPsBytecode, Tr2ShaderSignatureAL(), "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL glowShaders[] = { glowVs, glowPs };
	Tr2ShaderProgramAL glowProgram;
	if (Failed("Create glow program", glowProgram.Create(glowShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL flareVs;
	if (Failed("Create flare VS", flareVs.Create(VERTEX_SHADER, flareVsBytecode, haloVsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL flarePs;
	if (Failed("Create flare PS", flarePs.Create(PIXEL_SHADER, flarePsBytecode, Tr2ShaderSignatureAL(), "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ShaderAL flareShaders[] = { flareVs, flarePs };
	Tr2ShaderProgramAL flareProgram;
	if (Failed("Create flare program", flareProgram.Create(flareShaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	const std::vector<StarVertex> pointStars = MakeSystemVertices(catalog);
	const std::vector<StarInstance> starInstances = MakeStarInstances(catalog, visuals, regions);
	const std::vector<StarVertex> gates = MakeGateVertices(catalog, graph);
	const uint32_t gateVertexCount = uint32_t(gates.size());
	if (gateVertexCount != edgeCount * 2 || starInstances.size() != systemCount)
	{
		Log(stderr, "FAILED geometry counts\n");
		CloseSmokeLog();
		return 1;
	}

	Tr2BufferAL pointVertexBuffer;
	if (Failed("Create point VB", pointVertexBuffer.Create(sizeof(StarVertex), systemCount, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, pointStars.data(), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2BufferAL gateVertexBuffer;
	if (Failed("Create gate VB", gateVertexBuffer.Create(sizeof(StarVertex), gateVertexCount, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, gates.data(), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	const float quadCorners[] = { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f };
	Tr2BufferAL quadVb;
	if (Failed("Create quad VB", quadVb.Create(sizeof(float) * 2, 4, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, quadCorners, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	const uint16_t quadIndices[] = { 0, 1, 2, 1, 2, 3 };
	Tr2BufferAL quadIb;
	if (Failed("Create quad IB", quadIb.Create(PIXEL_FORMAT_R16_UINT, 6, Tr2GpuUsage::INDEX_BUFFER, Tr2CpuUsage::NONE, quadIndices, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2BufferAL instanceVb;
	if (Failed("Create instance VB", instanceVb.Create(sizeof(StarInstance), systemCount, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, starInstances.data(), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	const float fullscreenVerts[] = {
		-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
		1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
	};
	const uint32_t fsStride = 5 * sizeof(float);
	Tr2BufferAL fullscreenVb;
	if (Failed("Create fullscreen VB", fullscreenVb.Create(fsStride, 6, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, fullscreenVerts, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2VertexDefinition pointDef;
	pointDef.Add(Tr2VertexDefinition::FLOAT32_3, Tr2VertexDefinition::POSITION);
	pointDef.Add(Tr2VertexDefinition::FLOAT32_1, Tr2VertexDefinition::TEXCOORD);
	Tr2VertexLayoutAL pointLayout;
	if (Failed("Create point layout", pointLayout.Create(pointDef, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2VertexDefinition spriteDef;
	spriteDef.Add(Tr2VertexDefinition::FLOAT32_2, Tr2VertexDefinition::TEXCOORD);
	spriteDef.Add(Tr2VertexDefinition::FLOAT32_3, Tr2VertexDefinition::POSITION, 0, 1, 1);
	spriteDef.Add(Tr2VertexDefinition::FLOAT32_1, Tr2VertexDefinition::TEXCOORD, 1, 1, 1);
	spriteDef.Add(Tr2VertexDefinition::FLOAT32_4, Tr2VertexDefinition::COLOR, 0, 1, 1);
	spriteDef.Add(Tr2VertexDefinition::FLOAT32_3, Tr2VertexDefinition::TEXCOORD, 2, 1, 1);
	Tr2VertexLayoutAL spriteLayout;
	if (Failed("Create sprite layout", spriteLayout.Create(spriteDef, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2VertexDefinition fsDef;
	fsDef.Add(Tr2VertexDefinition::FLOAT32_3, Tr2VertexDefinition::POSITION);
	fsDef.Add(Tr2VertexDefinition::FLOAT32_2, Tr2VertexDefinition::TEXCOORD);
	Tr2VertexLayoutAL fsLayout;
	if (Failed("Create fullscreen layout", fsLayout.Create(fsDef, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ConstantBufferAL frameCb;
	if (Failed("Create frame CB", frameCb.Create(sizeof(FrameConstants), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ConstantBufferAL bloomCb;
	if (Failed("Create bloom CB", bloomCb.Create(sizeof(BloomConstants), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ConstantBufferAL skyCb;
	if (Failed("Create sky CB", skyCb.Create(sizeof(SkyConstants), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ConstantBufferAL ismCb;
	if (Failed("Create ism CB", ismCb.Create(sizeof(IsmConstants), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2ConstantBufferAL glowCb;
	if (Failed("Create glow CB", glowCb.Create(sizeof(GlowConstants), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2SamplerStateAL linearClamp;
	if (Failed("Create sampler", linearClamp.Create(Tr2SamplerDescription(TF_LINEAR, TA_CLAMP), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Offscreen offscreen;
	state.renderContext = renderContext;
	state.presentParameters = &presentParameters;
	state.depthBuffer = &depthBuffer;
	state.offscreen = &offscreen;
	state.sampler = &linearClamp;
	state.extractProgram = &extractProgram;
	state.blurProgram = &blurProgram;
	state.compositeProgram = &compositeProgram;
	state.ismProgram = &ismProgram;
	if (!RecreateOffscreen(state))
	{
		CloseSmokeLog();
		return 1;
	}

	if (!smoke)
	{
		if (!TunePanel_Create(instance, hwnd, &state.tune, state.panel))
		{
			Log(stderr, "WARNING: visual lab panel failed to create; sliders unavailable\n");
		}
		SetForegroundWindow(hwnd);
	}

	Log(stdout, "Creator Mode: sky, ISM, glow/flare, region tint, persist. F1 panel, F7 1px points, F8 save+copy, F9 baseline.\n");
	if (smoke)
	{
		Log(stdout, "smoke will present %u bloom-on frames then %u bloom-off frames\n", kSmokeBloomOnFrames, kSmokeFrames - kSmokeBloomOnFrames);
	}

	LARGE_INTEGER qpcFreq = {};
	QueryPerformanceFrequency(&qpcFreq);
	LARGE_INTEGER frameStart = {};
	QueryPerformanceCounter(&frameStart);
	double fpsWindowMs = 0.0;
	uint32_t fpsWindowFrames = 0;
	double totalFrameMs = 0.0;
	uint32_t lastDraws = 0;
	uint32_t lastPp = 0;
	uint32_t bloomOnDraws = 0;
	uint32_t bloomOnPp = 0;
	uint32_t bloomOffCreatorDraws = 0;
	uint32_t bloomOffCreatorPp = 0;

	uint32_t frames = 0;
	bool ok = true;
	while (PumpMessages())
	{
		if (state.resizePending)
		{
			state.resizePending = false;
			if (!ApplyResize(state))
			{
				ok = false;
				break;
			}
		}
		if (state.minimized)
		{
			Sleep(16);
			continue;
		}

		TuneParams frameTune = state.tune;
		if (smoke && frames >= 50)
		{
			frameTune.skyEnabled = false;
			frameTune.ismEnabled = false;
			frameTune.glowEnabled = false;
			frameTune.flareEnabled = false;
			frameTune.bloomEnabled = false;
		}
		const bool bloomOn = smoke ? (frames < kSmokeBloomOnFrames) : frameTune.bloomEnabled;
		const bool skyOn = frameTune.skyEnabled;
		const bool ismOn = frameTune.ismEnabled;
		const bool glowOn = frameTune.glowEnabled && !state.debugPoints && frameTune.glowIntensity > 0.001f;
		const bool flareOn = frameTune.flareEnabled && !state.debugPoints && frameTune.flareIntensity > 0.001f;
		if (!UpdateFrameConstants(frameCb, *renderContext, state) ||
			!UpdateSkyConstants(skyCb, *renderContext, state) ||
			!UpdateIsmConstants(ismCb, *renderContext, state) ||
			!UpdateGlowConstants(glowCb, *renderContext, frameTune))
		{
			ok = false;
			break;
		}

		if (Failed("BeginScene", renderContext->BeginScene()) ||
			Failed("Unbind SRVs", renderContext->SetResourceSet(offscreen.unbind)) ||
			Failed("Set scene RT", renderContext->SetRenderTarget(offscreen.scene)) ||
			Failed("SetDepthStencil scene", renderContext->SetDepthStencil(depthBuffer)) ||
			Failed("Clear scene", renderContext->Clear(CLEARFLAGS_TARGET | CLEARFLAGS_ZBUFFER, kClearArgb, 1.0f)))
		{
			ok = false;
			break;
		}

		uint32_t draws = 0;
		uint32_t ppPasses = 0;
		if (skyOn)
		{
			if (!DrawFullscreenNoSrv(*renderContext, fsLayout, fullscreenVb, fsStride, skyProgram, skyCb))
			{
				ok = false;
				break;
			}
			++draws;
			++ppPasses;
		}

		if (Failed("Set frame VS CB", renderContext->SetConstants(frameCb, VERTEX_SHADER, 0)) ||
			Failed("Set frame PS CB", renderContext->SetConstants(frameCb, PIXEL_SHADER, 0)))
		{
			ok = false;
			break;
		}

		if (Failed("RS_ZENABLE", renderContext->SetRenderState(RS_ZENABLE, 1)) ||
			Failed("RS_ZWRITEENABLE", renderContext->SetRenderState(RS_ZWRITEENABLE, 1)) ||
			Failed("RS_ZFUNC", renderContext->SetRenderState(RS_ZFUNC, CMP_LESSEQUAL)) ||
			Failed("RS_CULLMODE", renderContext->SetRenderState(RS_CULLMODE, CULLMODE_NONE)) ||
			Failed("RS_ALPHABLENDENABLE gates", renderContext->SetRenderState(RS_ALPHABLENDENABLE, 1)) ||
			Failed("RS_SRCBLEND gates", renderContext->SetRenderState(RS_SRCBLEND, BM_SRCALPHA)) ||
			Failed("RS_DESTBLEND gates", renderContext->SetRenderState(RS_DESTBLEND, BM_INVSRCALPHA)) ||
			// Gate RGB stays unpremultiplied light grey; alpha is the only fade.
			Failed("Set gate layout", renderContext->SetVertexLayout(pointLayout)) ||
			Failed("Set gate program", renderContext->SetShaderProgram(gateProgram)) ||
			Failed("Set gate stream", renderContext->SetStreamSource(0, gateVertexBuffer, 0, sizeof(StarVertex))) ||
			Failed("SetTopology TOP_LINES", renderContext->SetTopology(TOP_LINES)) ||
			Failed("DrawPrimitive gates", renderContext->DrawPrimitive(0, edgeCount)))
		{
			ok = false;
			break;
		}
		++draws;

		if (state.debugPoints)
		{
			if (Failed("RS_ALPHABLENDENABLE points", renderContext->SetRenderState(RS_ALPHABLENDENABLE, 0)) ||
				Failed("Set point program", renderContext->SetShaderProgram(pointProgram)) ||
				Failed("Set point stream", renderContext->SetStreamSource(0, pointVertexBuffer, 0, sizeof(StarVertex))) ||
				Failed("SetTopology TOP_POINTS", renderContext->SetTopology(TOP_POINTS)) ||
				Failed("DrawPrimitive points", renderContext->DrawPrimitive(0, systemCount)))
			{
				ok = false;
				break;
			}
			++draws;
		}
		else
		{
			if (Failed("RS_ZWRITEENABLE stars", renderContext->SetRenderState(RS_ZWRITEENABLE, 0)) ||
				Failed("RS_ALPHABLENDENABLE stars", renderContext->SetRenderState(RS_ALPHABLENDENABLE, 1)) ||
				Failed("RS_SRCBLEND stars", renderContext->SetRenderState(RS_SRCBLEND, BM_ONE)) ||
				Failed("RS_DESTBLEND stars", renderContext->SetRenderState(RS_DESTBLEND, BM_ONE)) ||
				Failed("RS_BLENDOP stars", renderContext->SetRenderState(RS_BLENDOP, BO_ADD)) ||
				Failed("Set sprite layout", renderContext->SetVertexLayout(spriteLayout)) ||
				Failed("Set sprite program", renderContext->SetShaderProgram(spriteProgram)) ||
				Failed("Set quad stream", renderContext->SetStreamSource(0, quadVb, 0, sizeof(float) * 2)) ||
				Failed("Set instance stream", renderContext->SetStreamSource(1, instanceVb, 0, sizeof(StarInstance))) ||
				Failed("SetIndices", renderContext->SetIndices(quadIb)) ||
				Failed("SetTopology sprites", renderContext->SetTopology(TOP_TRIANGLES)) ||
				Failed("DrawIndexedInstanced stars", renderContext->DrawIndexedInstanced(4, 0, 2, systemCount)))
			{
				ok = false;
				break;
			}
			++draws;
		}

		if (glowOn)
		{
			if (Failed("Set glow VS0", renderContext->SetConstants(frameCb, VERTEX_SHADER, 0)) ||
				Failed("Set glow VS1", renderContext->SetConstants(glowCb, VERTEX_SHADER, 1)) ||
				Failed("Set glow program", renderContext->SetShaderProgram(glowProgram)) ||
				Failed("Draw glow", renderContext->DrawIndexedInstanced(4, 0, 2, systemCount)))
			{
				ok = false;
				break;
			}
			++draws;
		}
		if (flareOn)
		{
			if (Failed("Set flare program", renderContext->SetShaderProgram(flareProgram)) ||
				Failed("Draw flare", renderContext->DrawIndexedInstanced(4, 0, 2, systemCount)))
			{
				ok = false;
				break;
			}
			++draws;
		}

		if (Failed("Unbind depth for PP", renderContext->SetDepthStencil(Tr2TextureAL())))
		{
			ok = false;
			break;
		}

		const uint32_t hw = (std::max)(1u, state.width / 2);
		const uint32_t hh = (std::max)(1u, state.height / 2);
		const float halfTexelX = 1.0f / float(hw);
		const float halfTexelY = 1.0f / float(hh);

		if (bloomOn)
		{
			if (!UpdateBloomConstants(bloomCb, *renderContext, frameTune, halfTexelX, halfTexelY, 1.0f, 0.0f, frameTune.bloomStrength) ||
				Failed("Set extract RT", renderContext->SetRenderTarget(offscreen.extract)) ||
				!DrawFullscreen(*renderContext, fsLayout, fullscreenVb, fsStride, extractProgram, offscreen.extractSet, bloomCb) ||
				Failed("Set blurA RT", renderContext->SetRenderTarget(offscreen.blurA)) ||
				!DrawFullscreen(*renderContext, fsLayout, fullscreenVb, fsStride, blurProgram, offscreen.blurFromExtract, bloomCb))
			{
				ok = false;
				break;
			}
			if (!UpdateBloomConstants(bloomCb, *renderContext, frameTune, halfTexelX, halfTexelY, 0.0f, 1.0f, frameTune.bloomStrength) ||
				Failed("Set blurB RT", renderContext->SetRenderTarget(offscreen.blurB)) ||
				!DrawFullscreen(*renderContext, fsLayout, fullscreenVb, fsStride, blurProgram, offscreen.blurFromA, bloomCb))
			{
				ok = false;
				break;
			}
			draws += 3;
			ppPasses += 3;
		}

		if (Failed("Set ism RT", renderContext->SetRenderTarget(offscreen.ism)) ||
			Failed("Clear ism", renderContext->Clear(CLEARFLAGS_TARGET, 0xff000000, 1.0f)))
		{
			ok = false;
			break;
		}
		if (ismOn)
		{
			if (!UpdateIsmConstants(ismCb, *renderContext, state) ||
				!DrawFullscreen(
					*renderContext,
					fsLayout,
					fullscreenVb,
					fsStride,
					ismProgram,
					bloomOn ? offscreen.ismFromBloom : offscreen.ismFromDummy,
					ismCb))
			{
				ok = false;
				break;
			}
			++draws;
			++ppPasses;
		}

		if (!UpdateBloomConstants(bloomCb, *renderContext, frameTune, halfTexelX, halfTexelY, 0.0f, 0.0f, bloomOn ? frameTune.bloomStrength : 0.0f) ||
			Failed("Set backbuffer", renderContext->SetRenderTarget(renderContext->GetDefaultBackBuffer())) ||
			!DrawFullscreen(
				*renderContext,
				fsLayout,
				fullscreenVb,
				fsStride,
				compositeProgram,
				bloomOn ? offscreen.compositeOn : offscreen.compositeOff,
				bloomCb))
		{
			ok = false;
			break;
		}
		++draws;
		++ppPasses;

		if (Failed("EndScene", renderContext->EndScene()) || Failed("Present", renderContext->Present()))
		{
			ok = false;
			break;
		}

		lastDraws = draws;
		lastPp = ppPasses;
		if (bloomOn)
		{
			bloomOnDraws = draws;
			bloomOnPp = ppPasses;
		}
		else if (smoke && frames == 49)
		{
			bloomOffCreatorDraws = draws;
			bloomOffCreatorPp = ppPasses;
		}
		++frames;
		LARGE_INTEGER frameEnd = {};
		QueryPerformanceCounter(&frameEnd);
		const double frameMs = double(frameEnd.QuadPart - frameStart.QuadPart) * 1000.0 / double(qpcFreq.QuadPart);
		frameStart = frameEnd;
		fpsWindowMs += frameMs;
		++fpsWindowFrames;
		totalFrameMs += frameMs;

		if (frames == 1)
		{
			Log(stdout, "first Present completed\n");
			const orbit::Vec3 eye = state.camera.Eye();
			Log(stdout, "camera eye=%.1f,%.1f,%.1f target=%.1f,%.1f,%.1f distance=%.1f\n",
				eye.x,
				eye.y,
				eye.z,
				state.camera.target.x,
				state.camera.target.y,
				state.camera.target.z,
				state.camera.distance);
		}
		if (fpsWindowMs >= 1000.0)
		{
			const double avgMs = fpsWindowMs / double(fpsWindowFrames);
			const double fps = (avgMs > 0.0) ? (1000.0 / avgMs) : 0.0;
			Log(stdout, "perf: systems=%u connections=%u draw_calls=%u pp_passes=%u bloom=%s stars=%s frame=%.2fms (%.0f fps) renderer=TrinityAL_DX11 dataset=SDE3464040\n",
				systemCount,
				edgeCount,
				lastDraws,
				lastPp,
				bloomOn ? "on" : "off",
				state.debugPoints ? "TOP_POINTS" : "instanced_quads",
				avgMs,
				fps);
			fpsWindowMs = 0.0;
			fpsWindowFrames = 0;
		}
		if (smoke && frames >= kSmokeFrames)
		{
			Log(stdout, "smoke test reached %u frames, exiting\n", frames);
			break;
		}
	}

	if (smoke && frames >= kSmokeFrames && totalFrameMs > 0.0)
	{
		const double avgMs = totalFrameMs / double(frames);
		const double fps = (avgMs > 0.0) ? (1000.0 / avgMs) : 0.0;
		Log(stdout, "smoke: frames=%u systems=%u connections=%u known_space=%u other_space=%u bloom_on_draws=%u bloom_on_pp=%u bloom_off_creator_draws=%u bloom_off_creator_pp=%u creator_off_draws=%u creator_off_pp=%u star_draws=1 gate_draws=1 bloom_on_frames=%u avg_frame_ms=%.2f avg_fps=%.1f path=TrinityAL_DX11/creator-mode dataset=SDE3464040\n",
			frames,
			systemCount,
			edgeCount,
			catalog.knownSpaceCount,
			catalog.otherSpaceCount,
			bloomOnDraws,
			bloomOnPp,
			bloomOffCreatorDraws,
			bloomOffCreatorPp,
			lastDraws,
			lastPp,
			kSmokeBloomOnFrames,
			avgMs,
			fps);
		const uint32_t expectBloomOnDraws = startPoints ? 8u : 9u;
		const uint32_t expectBloomOffDraws = startPoints ? 5u : 6u;
		if (bloomOnDraws != expectBloomOnDraws || bloomOnPp != 6 || bloomOffCreatorDraws != expectBloomOffDraws ||
			bloomOffCreatorPp != 3 || lastDraws != 3 || lastPp != 1)
		{
			Log(stderr, "FAILED smoke draw/pass counts (expected %u/6, %u/3, 3/1)\n", expectBloomOnDraws, expectBloomOffDraws);
			ok = false;
		}
	}

	Log(stdout, "exiting after %u frames\n", frames);
	renderContext->SetResourceSet(Tr2ResourceSetAL());
	renderContext->SetDepthStencil(Tr2TextureAL());
	renderContext->SetRenderTarget(renderContext->GetDefaultBackBuffer());
	renderContext->SetConstants(Tr2ConstantBufferAL(), VERTEX_SHADER, 0);
	renderContext->SetConstants(Tr2ConstantBufferAL(), PIXEL_SHADER, 0);
	renderContext->SetStreamSource(0, Tr2BufferAL(), 0, 0);
	renderContext->SetStreamSource(1, Tr2BufferAL(), 0, 0);
	renderContext->SetIndices(Tr2BufferAL());
	renderContext->SetShaderProgram(Tr2ShaderProgramAL());
	renderContext->Destroy();
	delete renderContext;
	Tr2PrimaryRenderContextAL::SetPrimaryRenderContext(nullptr);
	DestroyWindow(hwnd);
	const int exitCode = (!ok || (smoke && frames < kSmokeFrames)) ? 1 : 0;
	CloseSmokeLog();
	return exitCode;
}
