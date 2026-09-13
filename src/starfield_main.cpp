// Standalone TrinityAL DX11 starfield host.
// Window / device / Present sequence follows the Milestone 0 triangle and
// trinityal/tests (RenderWindow_Win32, WithValidRenderContextFixture, Rendering).
// Many-star path: Tr2RenderContextEnum::TOP_POINTS + one DrawPrimitive.
// Camera math is host-side; TrinityAL has no camera type.

#include <Windows.h>
#include <windowsx.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

typedef HWND Tr2WindowHandle;

#include <TrinityAL.h>

#include "orbit_camera.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

using namespace Tr2RenderContextEnum;

const char* g_moduleName = "eo-map-carbon-starfield";

namespace
{
const wchar_t* kWindowClass = L"eo-map-carbon-starfield";
const wchar_t* kWindowTitle = L"EO-Map Carbon starfield (TrinityAL DX11)";
const uint32_t kDefaultWidth = 1280;
const uint32_t kDefaultHeight = 720;
const uint32_t kStarCount = 25000;
const uint32_t kDrawCallsPerFrame = 1;
const uint32_t kSmokeFrames = 60;

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

struct HostState
{
	orbit::Camera camera;
	bool dragging = false;
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
	logPath += L"eo-map-carbon-starfield-smoke.log";
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
			state->dragging = true;
			state->lastMouseX = GET_X_LPARAM(lParam);
			state->lastMouseY = GET_Y_LPARAM(lParam);
			SetCapture(hwnd);
		}
		return 0;
	case WM_LBUTTONUP:
		if (state)
		{
			state->dragging = false;
			ReleaseCapture();
		}
		return 0;
	case WM_MOUSEMOVE:
		if (state && state->dragging)
		{
			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			state->camera.Orbit(float(x - state->lastMouseX), float(y - state->lastMouseY));
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

std::vector<StarVertex> GenerateStars()
{
	std::vector<StarVertex> stars(kStarCount);
	uint32_t rng = 0xC0FFEE01u;
	auto urand = [&]() -> float {
		rng = rng * 1664525u + 1013904223u;
		return float(rng >> 8) * (1.0f / 16777216.0f);
	};

	for (uint32_t i = 0; i < kStarCount; ++i)
	{
		const bool halo = urand() < 0.22f;
		const float rx = halo ? 90.0f : 42.0f;
		const float ry = halo ? 70.0f : 28.0f;
		const float rz = halo ? 110.0f : 58.0f;
		const float theta = urand() * 2.0f * orbit::kPi;
		const float phi = acosf(2.0f * urand() - 1.0f);
		const float r = cbrtf(urand());
		const float sp = sinf(phi);
		stars[i].x = rx * r * sp * cosf(theta);
		stars[i].y = ry * r * sp * sinf(theta);
		stars[i].z = rz * r * cosf(phi);
		stars[i].intensity = halo ? (0.40f + 0.35f * urand()) : (0.72f + 0.28f * urand());
	}
	return stars;
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

	Log(stdout, "eo-map-carbon-starfield starting\n");
	Log(stdout, "renderer: TrinityAL DX11\n");
	Log(stdout, "path: TOP_POINTS DrawPrimitive (one call)\n");
	Log(stdout, "star count: %u\n", kStarCount);
	Log(stdout, "draw calls per frame: %u\n", kDrawCallsPerFrame);

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

	const std::vector<StarVertex> stars = GenerateStars();
	const uint32_t stride = sizeof(StarVertex);
	Tr2BufferAL vertexBuffer;
	if (Failed("Create vertex buffer", vertexBuffer.Create(stride, kStarCount, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, stars.data(), *renderContext)))
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

	Log(stdout, "Rendering %u synthetic stars via TOP_POINTS. Left-drag orbits, wheel zooms. Close the window to exit.\n", kStarCount);

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
		if (Failed("SetStreamSource", renderContext->SetStreamSource(0, vertexBuffer, 0, stride)))
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
		if (Failed("SetTopology", renderContext->SetTopology(TOP_POINTS)))
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
		if (Failed("DrawPrimitive", renderContext->DrawPrimitive(0, kStarCount)))
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
			Log(stdout, "camera eye=%.1f,%.1f,%.1f distance=%.1f\n", eye.x, eye.y, eye.z, state.camera.distance);
		}
		if (fpsWindowMs >= 1000.0)
		{
			const double avgMs = fpsWindowMs / double(fpsWindowFrames);
			const double fps = (avgMs > 0.0) ? (1000.0 / avgMs) : 0.0;
			Log(stdout, "perf: stars=%u draw_calls=%u frame=%.2fms (%.0f fps) renderer=TrinityAL_DX11 TOP_POINTS\n",
				kStarCount,
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
		Log(stdout, "smoke: frames=%u stars=%u draw_calls/frame=%u avg_frame_ms=%.2f avg_fps=%.1f path=TrinityAL_DX11/TOP_POINTS\n",
			frames,
			kStarCount,
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
