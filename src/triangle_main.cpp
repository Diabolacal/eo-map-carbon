// Standalone TrinityAL DX11 host.
// Initialisation, window, device, and draw sequence are taken from
// carbonengine/trinity trinityal/tests (RenderWindow_Win32.cpp,
// WithValidRenderContextFixture.cpp, Rendering.cpp CanRenderASingleTriangle).

#include <Windows.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

typedef HWND Tr2WindowHandle;

#include <TrinityAL.h>

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <string>

using namespace Tr2RenderContextEnum;

// Required by CcpCore (see trinityal/tests/TrinityALTest.cpp).
const char* g_moduleName = "eo-map-carbon-triangle";

namespace
{
const wchar_t* kWindowClass = L"eo-map-carbon-triangle";
const wchar_t* kWindowTitle = L"EO-Map Carbon triangle (TrinityAL DX11)";
const uint32_t kWidth = 1280;
const uint32_t kHeight = 720;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
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
	RECT rect = { 0, 0, (LONG)kWidth, (LONG)kHeight };
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
	logPath += L"eo-map-carbon-triangle-smoke.log";
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
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmdLine, int)
{
	const bool smoke = cmdLine && wcsstr(cmdLine, L"--smoke");
	const uint32_t smokeFrames = 30;
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

	Log(stdout, "eo-map-carbon-triangle starting\n");
	Log(stdout, "renderer: TrinityAL DX11\n");

	unsigned adapterCount = 0;
	if (Failed("GetAdapterCount", Tr2VideoAdapterInfo::GetAdapterCount(adapterCount)) || adapterCount == 0)
	{
		Log(stderr, "No GPU adapter reported by TrinityAL.\n");
		CloseSmokeLog();
		return 1;
	}
	Log(stdout, "adapter count: %u\n", adapterCount);

	HWND hwnd = CreateHostWindow(instance);
	if (!hwnd)
	{
		Log(stderr, "CreateWindowW failed (%lu)\n", GetLastError());
		CloseSmokeLog();
		return 1;
	}
	Log(stdout, "window hwnd=%p %ux%u\n", hwnd, kWidth, kHeight);

	Tr2PrimaryRenderContextAL* renderContext = new Tr2PrimaryRenderContextAL();
	Tr2PrimaryRenderContextAL::SetPrimaryRenderContext(renderContext);

	Tr2PresentParametersAL presentParameters = {};
	if (Failed("GetAdapterDisplayMode", Tr2VideoAdapterInfo::GetAdapterDisplayMode(Tr2VideoAdapterInfo::DEFAULT_ADAPTER, presentParameters.mode)))
	{
		CloseSmokeLog();
		return 1;
	}
	presentParameters.mode.width = kWidth;
	presentParameters.mode.height = kHeight;
	presentParameters.backBufferCount = 1;
	presentParameters.msaaType = 0;
	presentParameters.msaaQuality = 0;
	presentParameters.swapEffect = SWAP_EFFECT_DISCARD;
	presentParameters.outputWindow = hwnd;
	presentParameters.windowed = true;
	presentParameters.software = false;
	presentParameters.presentInterval = PRESENT_INTERVAL_ONE;

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

	uint8_t vsBytecode[] = {
#include "PositionOnly_vs.h"
	};
	uint8_t psBytecode[] = {
#include "ConstantColor_ps.h"
	};

	Tr2ShaderAL vs;
	auto vsInput = Tr2ShaderSignatureAL().Add(Tr2VertexDefinition::POSITION, 0, 0, Tr2ShaderPipelineInputAL::FLOAT, 3);
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

	float vertices[] = {
		-0.5f, -0.5f, 0.0f,
		-0.5f, 0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
	};
	const uint32_t stride = 3 * sizeof(float);
	Tr2BufferAL vertexBuffer;
	if (Failed("Create vertex buffer", vertexBuffer.Create(stride, sizeof(vertices) / stride, Tr2GpuUsage::VERTEX_BUFFER, Tr2CpuUsage::NONE, vertices, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Tr2VertexDefinition definition;
	definition.Add(Tr2VertexDefinition::FLOAT32_3, Tr2VertexDefinition::POSITION);
	Tr2VertexLayoutAL vertexLayout;
	if (Failed("Create vertex layout", vertexLayout.Create(definition, *renderContext)))
	{
		CloseSmokeLog();
		return 1;
	}

	Log(stdout, "Rendering a red triangle. Close the window to exit.\n");

	uint32_t frames = 0;
	while (PumpMessages())
	{
		if (Failed("BeginScene", renderContext->BeginScene()))
		{
			break;
		}
		// Dark blue-grey clear so the red triangle is obvious.
		if (Failed("Clear", renderContext->Clear(CLEARFLAGS_TARGET, 0xff202040, 1.0f)))
		{
			break;
		}
		if (Failed("SetStreamSource", renderContext->SetStreamSource(0, vertexBuffer, 0, stride)))
		{
			break;
		}
		if (Failed("SetVertexLayout", renderContext->SetVertexLayout(vertexLayout)))
		{
			break;
		}
		if (Failed("SetShaderProgram", renderContext->SetShaderProgram(shaderProgram)))
		{
			break;
		}
		if (Failed("SetTopology", renderContext->SetTopology(TOP_TRIANGLES)))
		{
			break;
		}
		if (Failed("RS_ZENABLE", renderContext->SetRenderState(RS_ZENABLE, 0)))
		{
			break;
		}
		if (Failed("RS_CULLMODE", renderContext->SetRenderState(RS_CULLMODE, CULLMODE_NONE)))
		{
			break;
		}
		if (Failed("DrawPrimitive", renderContext->DrawPrimitive(0, 1)))
		{
			break;
		}
		if (Failed("EndScene", renderContext->EndScene()))
		{
			break;
		}
		if (Failed("Present", renderContext->Present()))
		{
			break;
		}
		++frames;
		if (frames == 1)
		{
			Log(stdout, "first Present completed\n");
		}
		if (smoke && frames >= smokeFrames)
		{
			Log(stdout, "smoke test reached %u frames, exiting\n", frames);
			break;
		}
	}

	Log(stdout, "exiting after %u frames\n", frames);
	renderContext->Destroy();
	delete renderContext;
	Tr2PrimaryRenderContextAL::SetPrimaryRenderContext(nullptr);
	DestroyWindow(hwnd);
	const int exitCode = (smoke && frames < smokeFrames) ? 1 : 0;
	CloseSmokeLog();
	return exitCode;
}
