// Standalone TrinityAL DX11 New Eden host (Milestone 1C + Jita-Amarr route highlight).
// Systems stay on the proven 1B TOP_POINTS path. Gates are one static TOP_LINES
// buffer of unique undirected Contract A pairs. The Jita-Amarr shortest hop route
// is a second static TOP_LINES buffer drawn on top. Camera controls are unchanged.

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
#include "new_eden_gates_expect.h"
#include "orbit_camera.h"

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
const wchar_t* kWindowTitle = L"EO-Map Carbon New Eden (TrinityAL DX11)";
const uint32_t kDefaultWidth = 1280;
const uint32_t kDefaultHeight = 720;
const uint32_t kDrawCallsPerFrame = 3;
const float kGateIntensity = 0.22f;
const float kRouteIntensity = 1.0f;
const uint32_t kSmokeFrames = 60;
const char* kDatasetId = "map_data_eo_3464040.db builder=1.5.0 SDE=3464040";

struct StarVertex
{
	float x;
	float y;
	float z;
	float intensity;
};

struct ViewProjConstants
{
	orbit::Mat4 viewProj;
};

enum class DragMode
{
	None,
	Orbit,
	Pan,
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
	Tr2PrimaryRenderContextAL* renderContext = nullptr;
	Tr2PresentParametersAL* presentParameters = nullptr;
	Tr2TextureAL* depthBuffer = nullptr;
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
	// Brightness only. Same display positions EO-Map uploads to three.js.
	const float extentLy = 101.195871877f;
	for (const neweden::System& system : catalog.systems)
	{
		const float dx = system.sceneX - neweden::kCentreX;
		const float dy = system.sceneY - neweden::kCentreY;
		const float dz = system.sceneZ - neweden::kCentreZ;
		const float norm = sqrtf(dx * dx + dy * dy + dz * dz) / extentLy;
		const float intensity = fmaxf(0.58f, 1.0f - powf(norm, 1.12f));
		stars.push_back({ system.sceneX, system.sceneY, system.sceneZ, intensity });
	}
	return stars;
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
		lines.push_back({ source->sceneX, source->sceneY, source->sceneZ, kGateIntensity });
		lines.push_back({ dest->sceneX, dest->sceneY, dest->sceneZ, kGateIntensity });
	}
	return lines;
}

const neweden::System* SystemById(const neweden::Catalog& catalog, uint32_t id)
{
	for (const neweden::System& system : catalog.systems)
	{
		if (system.id == id)
		{
			return &system;
		}
	}
	return nullptr;
}

std::string FormatRouteNames(const neweden::Catalog& catalog, const std::vector<uint32_t>& systemIds)
{
	std::string out;
	for (size_t i = 0; i < systemIds.size(); ++i)
	{
		if (i != 0)
		{
			out += " -> ";
		}
		const neweden::System* system = SystemById(catalog, systemIds[i]);
		out += system ? system->name : "?";
	}
	return out;
}

std::vector<StarVertex> MakeRouteVertices(const neweden::Catalog& catalog, const std::vector<uint32_t>& systemIds)
{
	std::unordered_map<uint32_t, const neweden::System*> byId;
	byId.reserve(catalog.systems.size());
	for (const neweden::System& system : catalog.systems)
	{
		byId.emplace(system.id, &system);
	}

	std::vector<StarVertex> lines;
	if (systemIds.size() < 2)
	{
		return lines;
	}
	lines.reserve((systemIds.size() - 1) * 2);
	for (size_t i = 1; i < systemIds.size(); ++i)
	{
		const neweden::System* source = byId.at(systemIds[i - 1]);
		const neweden::System* dest = byId.at(systemIds[i]);
		lines.push_back({ source->sceneX, source->sceneY, source->sceneZ, kRouteIntensity });
		lines.push_back({ dest->sceneX, dest->sceneY, dest->sceneZ, kRouteIntensity });
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
	if (Failed("SetDepthStencil after resize", state.renderContext->SetDepthStencil(*state.depthBuffer)))
	{
		return false;
	}

	Log(stdout, "resized swap chain to %ux%u\n", state.width, state.height);
	return true;
}

bool UpdateViewProj(Tr2ConstantBufferAL& cb, Tr2PrimaryRenderContextAL& renderContext, const orbit::Camera& camera, uint32_t width, uint32_t height)
{
	const float aspect = (height > 0) ? (float(width) / float(height)) : 1.0f;
	ViewProjConstants* data = nullptr;
	if (Failed("Lock constant buffer", cb.Lock(reinterpret_cast<void**>(&data), renderContext)))
	{
		return false;
	}
	data->viewProj = camera.ViewProjection(aspect);
	if (Failed("Unlock constant buffer", cb.Unlock(renderContext)))
	{
		return false;
	}
	return true;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int)
{
	const bool smoke = cmdLine && wcsstr(cmdLine, L"--smoke");
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
	Log(stdout, "path: TOP_LINES gates + TOP_LINES route + TOP_POINTS DrawPrimitive (three calls)\n");
	Log(stdout, "draw calls per frame: %u\n", kDrawCallsPerFrame);

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
	Log(stdout, "anchor check: Jita/Amarr/Dodixie/Rens/Hek scene coordinates match EO-Map transform\n");
	Log(stdout, "graph check: endpoints in catalogue, Jita/Amarr/Dodixie/Rens/Hek/Zarzakh adjacency, Jita-Amarr hops=11, Niarja unreachable\n");

	std::vector<uint32_t> routeIds;
	std::string routeError;
	if (!neweden::ShortestRoute(graph, neweden::kJitaId, neweden::kAmarrId, routeIds, routeError))
	{
		Log(stderr, "FAILED Jita-Amarr shortest route: %s\n", routeError.c_str());
		CloseSmokeLog();
		return 1;
	}
	const uint32_t routeHops = routeIds.empty() ? 0 : uint32_t(routeIds.size() - 1);
	const uint32_t routeSegmentCount = routeHops;
	const std::string routeNames = FormatRouteNames(catalog, routeIds);
	Log(stdout, "route: %s\n", routeNames.c_str());
	Log(stdout, "route hops: %u (systems=%u, segments=%u, expected hops=%u)\n",
		routeHops,
		uint32_t(routeIds.size()),
		routeSegmentCount,
		neweden::kExpectedJitaAmarrHops);
	if (routeHops != neweden::kExpectedJitaAmarrHops ||
		routeIds.empty() ||
		routeIds.front() != neweden::kJitaId ||
		routeIds.back() != neweden::kAmarrId)
	{
		Log(stderr, "FAILED Jita-Amarr reconstructed route does not match expected endpoints/hops\n");
		CloseSmokeLog();
		return 1;
	}

	unsigned adapterCount = 0;
	if (Failed("GetAdapterCount", Tr2VideoAdapterInfo::GetAdapterCount(adapterCount)) || adapterCount == 0)
	{
		Log(stderr, "No GPU adapter reported by TrinityAL.\n");
		CloseSmokeLog();
		return 1;
	}
	Log(stdout, "adapter count: %u\n", adapterCount);

	HostState state;
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
	// Smoke measures real submit cost. Interactive uses vsync like the triangle host.
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

	state.renderContext = renderContext;
	state.presentParameters = &presentParameters;
	state.depthBuffer = &depthBuffer;

	uint8_t vsBytecode[] = {
#include "Starfield_vs.h"
	};
	uint8_t psBytecode[] = {
#include "StarColor_ps.h"
	};

	Tr2ShaderAL vs;
	auto vsInput = Tr2ShaderSignatureAL()
					   .Add(Tr2VertexDefinition::POSITION, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 3)
					   .Add(Tr2VertexDefinition::TEXCOORD, 0, 1, Tr2ShaderPipelineInputAL::FLOAT, 1)
					   .Add(Tr2ShaderRegisterAL::CONSTANT_BUFFER, 0);
	if (Failed("Create VS", vs.Create(VERTEX_SHADER, vsBytecode, vsInput, "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL ps;
	if (Failed("Create PS", ps.Create(PIXEL_SHADER, psBytecode, Tr2ShaderSignatureAL(), "", *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ShaderAL shaders[] = { vs, ps };
	Tr2ShaderProgramAL shaderProgram;
	if (Failed("Create shader program", shaderProgram.Create(shaders, 2, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	const std::vector<StarVertex> stars = MakeSystemVertices(catalog);
	const std::vector<StarVertex> gates = MakeGateVertices(catalog, graph);
	const std::vector<StarVertex> routeLines = MakeRouteVertices(catalog, routeIds);
	const uint32_t stride = sizeof(StarVertex);
	const uint32_t gateVertexCount = uint32_t(gates.size());
	const uint32_t routeVertexCount = uint32_t(routeLines.size());
	if (routeVertexCount != routeSegmentCount * 2)
	{
		Log(stderr, "FAILED route vertex count %u != 2 * %u segments\n", routeVertexCount, routeSegmentCount);
		CloseSmokeLog();
		return 1;
	}
	if (gateVertexCount != edgeCount * 2)
	{
		Log(stderr, "FAILED gate vertex count %u != 2 * %u connections\n", gateVertexCount, edgeCount);
		CloseSmokeLog();
		return 1;
	}
	Tr2BufferAL vertexBuffer;
	if (Failed("Create vertex buffer", vertexBuffer.Create(stride, systemCount, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, stars.data(), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2BufferAL gateVertexBuffer;
	if (Failed("Create gate vertex buffer", gateVertexBuffer.Create(stride, gateVertexCount, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, gates.data(), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}
	Tr2BufferAL routeVertexBuffer;
	if (Failed("Create route vertex buffer", routeVertexBuffer.Create(stride, routeVertexCount, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, routeLines.data(), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2VertexDefinition definition;
	definition.Add(Tr2VertexDefinition::FLOAT32_3, Tr2VertexDefinition::POSITION);
	definition.Add(Tr2VertexDefinition::FLOAT32_1, Tr2VertexDefinition::TEXCOORD);
	Tr2VertexLayoutAL vertexLayout;
	if (Failed("Create vertex layout", vertexLayout.Create(definition, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2ConstantBufferAL cameraCb;
	if (Failed("Create constant buffer", cameraCb.Create(sizeof(ViewProjConstants), *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Log(stdout, "Rendering %u New Eden systems via TOP_POINTS, %u stargate connections via TOP_LINES, and the Jita-Amarr shortest route (%u hops) as a brighter TOP_LINES overlay. Left-drag orbits, right-drag pans, wheel zooms. Close the window to exit.\n", systemCount, edgeCount, routeHops);

	LARGE_INTEGER qpcFreq = {};
	QueryPerformanceFrequency(&qpcFreq);
	LARGE_INTEGER frameStart = {};
	QueryPerformanceCounter(&frameStart);
	double fpsWindowMs = 0.0;
	uint32_t fpsWindowFrames = 0;
	double totalFrameMs = 0.0;

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

		if (!UpdateViewProj(cameraCb, *renderContext, state.camera, state.width, state.height))
		{
			ok = false;
			break;
		}

		if (Failed("BeginScene", renderContext->BeginScene()))
		{
			ok = false;
			break;
		}
		if (Failed("Clear", renderContext->Clear(CLEARFLAGS_TARGET | CLEARFLAGS_ZBUFFER, 0xff05050c, 1.0f)))
		{
			ok = false;
			break;
		}
		if (Failed("SetConstants", renderContext->SetConstants(cameraCb, VERTEX_SHADER, 0)))
		{
			ok = false;
			break;
		}
		if (Failed("SetVertexLayout", renderContext->SetVertexLayout(vertexLayout)))
		{
			ok = false;
			break;
		}
		if (Failed("SetShaderProgram", renderContext->SetShaderProgram(shaderProgram)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_ZENABLE", renderContext->SetRenderState(RS_ZENABLE, 1)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_ZWRITEENABLE", renderContext->SetRenderState(RS_ZWRITEENABLE, 1)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_ZFUNC", renderContext->SetRenderState(RS_ZFUNC, CMP_LESSEQUAL)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_CULLMODE", renderContext->SetRenderState(RS_CULLMODE, CULLMODE_NONE)))
		{
			ok = false;
			break;
		}
		if (Failed("SetStreamSource gates", renderContext->SetStreamSource(0, gateVertexBuffer, 0, stride)))
		{
			ok = false;
			break;
		}
		if (Failed("SetTopology TOP_LINES", renderContext->SetTopology(TOP_LINES)))
		{
			ok = false;
			break;
		}
		if (Failed("DrawPrimitive gates", renderContext->DrawPrimitive(0, edgeCount)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_ZENABLE route overlay", renderContext->SetRenderState(RS_ZENABLE, 0)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_ZWRITEENABLE route overlay", renderContext->SetRenderState(RS_ZWRITEENABLE, 0)))
		{
			ok = false;
			break;
		}
		if (Failed("SetStreamSource route", renderContext->SetStreamSource(0, routeVertexBuffer, 0, stride)))
		{
			ok = false;
			break;
		}
		if (Failed("SetTopology TOP_LINES route", renderContext->SetTopology(TOP_LINES)))
		{
			ok = false;
			break;
		}
		if (Failed("DrawPrimitive route", renderContext->DrawPrimitive(0, routeSegmentCount)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_ZENABLE restore", renderContext->SetRenderState(RS_ZENABLE, 1)))
		{
			ok = false;
			break;
		}
		if (Failed("RS_ZWRITEENABLE restore", renderContext->SetRenderState(RS_ZWRITEENABLE, 1)))
		{
			ok = false;
			break;
		}
		if (Failed("SetStreamSource points", renderContext->SetStreamSource(0, vertexBuffer, 0, stride)))
		{
			ok = false;
			break;
		}
		if (Failed("SetTopology TOP_POINTS", renderContext->SetTopology(TOP_POINTS)))
		{
			ok = false;
			break;
		}
		if (Failed("DrawPrimitive points", renderContext->DrawPrimitive(0, systemCount)))
		{
			ok = false;
			break;
		}
		if (Failed("EndScene", renderContext->EndScene()))
		{
			ok = false;
			break;
		}
		if (Failed("Present", renderContext->Present()))
		{
			ok = false;
			break;
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
			Log(stdout, "perf: systems=%u connections=%u route_hops=%u draw_calls=%u (1 gate line + 1 route line + 1 point) frame=%.2fms (%.0f fps) renderer=TrinityAL_DX11 TOP_LINES+TOP_POINTS dataset=SDE3464040\n",
				systemCount,
				edgeCount,
				routeHops,
				kDrawCallsPerFrame,
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
		Log(stdout, "smoke: frames=%u systems=%u connections=%u known_space=%u other_space=%u route_hops=%u draw_calls/frame=%u point_draws=1 gate_draws=1 route_draws=1 avg_frame_ms=%.2f avg_fps=%.1f path=TrinityAL_DX11/TOP_LINES+TOP_POINTS dataset=SDE3464040\n",
			frames,
			systemCount,
			edgeCount,
			catalog.knownSpaceCount,
			catalog.otherSpaceCount,
			routeHops,
			kDrawCallsPerFrame,
			avgMs,
			fps);
	}

	Log(stdout, "exiting after %u frames\n", frames);
	renderContext->SetDepthStencil(Tr2TextureAL());
	renderContext->SetConstants(Tr2ConstantBufferAL(), VERTEX_SHADER, 0);
	renderContext->SetStreamSource(0, Tr2BufferAL(), 0, 0);
	renderContext->SetShaderProgram(Tr2ShaderProgramAL());
	renderContext->Destroy();
	delete renderContext;
	Tr2PrimaryRenderContextAL::SetPrimaryRenderContext(nullptr);
	DestroyWindow(hwnd);
	const int exitCode = (!ok || (smoke && frames < kSmokeFrames)) ? 1 : 0;
	CloseSmokeLog();
	return exitCode;
}
