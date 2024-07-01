#include "SDK.h"

#include <iostream>

#include "Util/cIcons.h"
#include <filesystem>

#include "Config.h"

#include <d3d11.h>
#include <dxgi.h>
#include "kiero/kiero.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include "ImGui/imgui_internal.h"
#include "kiero/minhook/include/MinHook.h"

#define safe_release(x) if(x) { x->Release(); x = nullptr; }
namespace fs = std::filesystem;


HINSTANCE hInstance;
uintptr_t dxgiBase = NULL;
DWORD dxgiSize = NULL;

Config::Configuration* cfg = &Config::cfg;

HRESULT presentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
typedef HRESULT(*DX11Present)(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags);
DX11Present oDX11Present = nullptr;

HRESULT resizeBufferHook(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
typedef HRESULT(*DX11ResizeBuffer)(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags);
DX11ResizeBuffer oDX11ResizeBuffer = nullptr;

ID3D11Device* device = nullptr;
ID3D11DeviceContext* context = nullptr;
ID3D11RenderTargetView* rtv = nullptr;
HWND g_TargetWindow = NULL;

WNDPROC oWindowProc = nullptr;

LRESULT CALLBACK windowProcHookEx(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
HCURSOR WINAPI hkSetCursor(HCURSOR hCursor);
BOOL WINAPI hkSetCursorPos(int x, int y);
typedef HCURSOR(WINAPI* fnSetCursor)(HCURSOR hCursor);
typedef BOOL(WINAPI* fnSetCursorPos)(int x, int y);
fnSetCursor oSetCursor = nullptr;
fnSetCursorPos oSetCursorPos = nullptr;

void clearDX11Objects();

void render(ImDrawList* drawList);
void RenderText(ImDrawList* drawList, const char* text, const ImVec2& screen, const bool centered = false, const bool outlined = false, const float size = 16.0f, const ImVec4& color = cfg->client.ClientDefaultColor);
void RenderPin(ImDrawList* drawList, const ImVec2& pos, const ImVec4& color, const float radius);
void Render2DBox(ImDrawList* drawList, const ImVec2& top, const ImVec2& bottom, const float height, const float width, const ImVec4& color);

void UpdateGameSpeed(float value);
void ForceQuitMatch();
void SetPlayerType();
void ToggleStallTime();

FILE* file;
bool running = false;
bool showMenu = false;
bool bIsIngame = false;

int tickCounter = 0;

static std::chrono::steady_clock::time_point lastForceQuit;

enum ConfigKeys
{
	OpenMenuKey = VK_INSERT,
	ExitKey = VK_END,
	ForceQuitKey = VK_F5, //0x51,
	GameSpeed1Key = VK_F1, //0x41,
	GameSpeed2Key = VK_F2, //0x53,
	GameSpeed3Key = VK_F3, //0x44,
	GameSpeed4Key = VK_F4, //0x46,

	SetPlayerAsCPUKey = VK_F7,
	StallTimeKey = VK_F8,




	// SOME DEBUG KEYS
	ToggleGetDuelTimeInfoKey = VK_F6, //0x52,

};

bool Exit()
{
	std::cout << "Unloading.." << std::endl;
	Sleep(3000);

	kiero::unbind(8);
	kiero::unbind(13);
	SetWindowLongPtr(g_TargetWindow, GWLP_WNDPROC, (LONG_PTR)oWindowProc);
	kiero::shutdown();

	clearDX11Objects();

	fclose(file);
	FreeConsole();
	FreeLibraryAndExitThread(hInstance, 0);
	return true;
}

bool Initialize()
{
	AllocConsole();
	freopen_s(&file, "CONOUT$", "w", stdout);
	std::cout << "Loading.." << std::endl;

	SDK::baseAddress = (uintptr_t)GetModuleHandle(L"GameAssembly.dll");
	if (!SDK::baseAddress)
	{
		std::cout << "GameAssembly.dll Base Address Not Found!" << std::endl;
		return false;
	}
	SDK::unityPlayerBaseAddress = (uintptr_t)GetModuleHandle(L"UnityPlayer.dll");
	if (!SDK::unityPlayerBaseAddress)
	{
		std::cout << "UnityPlayer.dll Base Address Not Found!" << std::endl;
		return false;
	}
	std::cout << "GameAssembly.dll Base Address Found At: " << std::hex << SDK::baseAddress << std::endl;
	std::cout << "UnityPlayer.dll Base Address Found At: " << std::hex << SDK::unityPlayerBaseAddress << std::endl;

	// DirectX
	HMODULE dxgiHandle = GetModuleHandle(L"dxgi.dll");
	if (!dxgiHandle)
	{
		std::cout << "dxgi.dll Not Found!" << std::endl;
		return false;
	}
	MODULEINFO hmInfo;
	GetModuleInformation(GetCurrentProcess(), dxgiHandle, &hmInfo, sizeof(MODULEINFO));
	dxgiBase = (uintptr_t)hmInfo.lpBaseOfDll;
	dxgiSize = hmInfo.SizeOfImage;

	// HookPresent

	if (kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success)
	{
		std::cout << "Failed to initialize Kiero!" << std::endl;
		return false;
	}

	kiero::bind(8, (void**)&oDX11Present, presentHook);
	std::cout << "HookPresent Hooked At: " << std::hex << oDX11Present << std::endl;

	kiero::bind(13, (void**)&oDX11ResizeBuffer, resizeBufferHook);
	std::cout << "HookResizeBuffer Hooked At: " << std::hex << oDX11ResizeBuffer << std::endl;

	if (MH_CreateHook(&SetCursor, &hkSetCursor, (LPVOID*)&oSetCursor) == MH_OK)
	{
		MH_EnableHook(reinterpret_cast<LPVOID*>(&SetCursor));
	}
	if (MH_CreateHook(&SetCursorPos, &hkSetCursorPos, (LPVOID*)&oSetCursorPos) == MH_OK)
	{
		MH_EnableHook(reinterpret_cast<LPVOID*>(&SetCursorPos));
	}

	// Hook Engine.Create/Destroy To know when we are in a duel
	LPVOID* EngineCreateAddr = (LPVOID*)(SDK::baseAddress + SDK::oEngineCreate);
	if (MH_CreateHook((LPVOID*)(EngineCreateAddr), &SDK::hkEngineCreateFunc, (LPVOID*)&SDK::ohkEngineCreateFunc) == MH_OK)
	{
		MH_EnableHook(reinterpret_cast<LPVOID*>(EngineCreateAddr));
	}
	LPVOID* EngineDestroyAddr = (LPVOID*)(SDK::baseAddress + SDK::oEngineDestroy);
	if (MH_CreateHook((LPVOID*)(EngineDestroyAddr), &SDK::hkEngineDestroyFunc, (LPVOID*)&SDK::ohkEngineDestroyFunc) == MH_OK)
	{
		MH_EnableHook(reinterpret_cast<LPVOID*>(EngineDestroyAddr));
	}

	SDK::bIsInGame = &bIsIngame;
	lastForceQuit = std::chrono::steady_clock::now();

	return true;
}

void UpdateGameSpeed(float value)
{
	if (value == cfg->client.CustomTimeScaleValCurrent) return;

	cfg->client.CustomTimeScaleValCurrent = value;

	bool success = SDK::SetGameSpeed(value);
	if (success)
	{
		std::cout << "Game Speed Set To: " << value * 100 << "%." << std::endl;
	}
	else
	{
		std::cout << "Failed To Set Game Speed!" << std::endl;
	}
}

void ForceQuitMatch()
{
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastForceQuit);

	if (elapsed.count() < 5) {
		std::cout << "Wait a moment before using this again." << std::endl;
		return;
	}
	else
	{
		if (bIsIngame && SDK::DuelEndStep())
		{
			std::cout << "Removed from duel!" << std::endl;
			lastForceQuit = now;
		}
		else
		{
			std::cout << "Failed to remove from duel! Are you in a match?" << std::endl;
		}
	}
}

void SetPlayerType()
{
	if (bIsIngame)
	{
		if (Config::cfg.game.bIsPlayerCPU)
		{
			SDK::SetPlayerType(SDK::Myself(), 1);
			std::cout << "Toggled to CPU mode" << std::endl;
		}
		else
		{
			SDK::SetPlayerType(SDK::Myself(), 0);
			std::cout << "Toggled to Human mode" << std::endl;
		}
		Config::cfg.game.bIsPlayerCPU = !Config::cfg.game.bIsPlayerCPU;
	}
	else
	{
		std::cout << "You are not in a match!" << std::endl;
	}
}

void ToggleStallTime()
{
	cfg->game.bIsBusyCheckBypass = !cfg->game.bIsBusyCheckBypass;
	SDK::SetIsBusyCheckBypass(cfg->game.bIsBusyCheckBypass);


	/*uintptr_t busyCheckAddr = SDK::baseAddress + SDK::oEngineIsBusyCheckBypass;
	BYTE buffer[2];

	if (cfg->game.bIsBusyCheckBypass)
	{
		SDK::Nop((PBYTE)busyCheckAddr, 2, buffer);
	}
	else
	{
		BYTE opc_jz[2] = { 0x74, 0x07 };
		SDK::HP((PBYTE)busyCheckAddr, opc_jz, 2, buffer);
	}*/
}

void Run()
{
	running = true;
	if (!Initialize()) running = false;

	while (running)
	{
		if (GetAsyncKeyState(ConfigKeys::ExitKey) & 1)
		{
			running = false;
			continue;
		}
		if (GetAsyncKeyState(ConfigKeys::ToggleGetDuelTimeInfoKey) & 1)
		{
			cfg->client.bDebugMode = !cfg->client.bDebugMode;
		}
		if (GetAsyncKeyState(ConfigKeys::OpenMenuKey) & 1)
		{
			showMenu = !showMenu;
		}
		if (GetAsyncKeyState(ConfigKeys::ForceQuitKey) & 1)
		{
			ForceQuitMatch();
		}
		if (GetAsyncKeyState(ConfigKeys::SetPlayerAsCPUKey) & 1)
		{
			SetPlayerType();
		}

		if (cfg->client.bCustomTimeScale)
		{
			UpdateGameSpeed(cfg->client.CustomTimeScaleVal0);
		}
		else if (GetAsyncKeyState(ConfigKeys::GameSpeed1Key) & 1)
		{
			UpdateGameSpeed(cfg->client.CustomTimeScaleVal1);
		}
		else if (GetAsyncKeyState(ConfigKeys::GameSpeed2Key) & 1)
		{
			UpdateGameSpeed(cfg->client.CustomTimeScaleVal2);
		}
		else if (GetAsyncKeyState(ConfigKeys::GameSpeed3Key) & 1)
		{
			UpdateGameSpeed(cfg->client.CustomTimeScaleVal3);
		}
		else if (GetAsyncKeyState(ConfigKeys::GameSpeed4Key) & 1)
		{
			UpdateGameSpeed(cfg->client.CustomTimeScaleVal4);
		}

		if (GetAsyncKeyState(ConfigKeys::StallTimeKey) & 1)
		{
			ToggleStallTime();
		}
		Sleep(10);
	}
	Exit();
}

void clearDX11Objects()
{
	safe_release(rtv);
	safe_release(context);
	safe_release(device);
	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK windowProcHookEx(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (showMenu)
	{
		if (ImGui::GetCurrentContext() != NULL)
		{
			ImGuiIO& io = ImGui::GetIO();
			switch (msg)
			{
			case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
			case WM_XBUTTONDOWN: case WM_XBUTTONDBLCLK:
			{
				int button = 0;
				if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) { button = 0; }
				if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) { button = 1; }
				if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK) { button = 2; }
				if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONDBLCLK) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				if (!ImGui::IsAnyMouseDown() && GetCapture() == NULL)
					SetCapture(hWnd);
				io.MouseDown[button] = true;
				break;
			}
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
			case WM_XBUTTONUP:
			{
				int button = 0;
				if (msg == WM_LBUTTONUP) { button = 0; }
				if (msg == WM_RBUTTONUP) { button = 1; }
				if (msg == WM_MBUTTONUP) { button = 2; }
				if (msg == WM_XBUTTONUP) { button = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? 3 : 4; }
				io.MouseDown[button] = false;
				if (!ImGui::IsAnyMouseDown() && GetCapture() == hWnd)
					ReleaseCapture();
				break;
			}
			case WM_MOUSEWHEEL:
				io.MouseWheel += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
				break;
			case WM_MOUSEHWHEEL:
				io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
				break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				if (wParam < 256)
					io.KeysDown[wParam] = 1;
				break;
			case WM_KEYUP:
			case WM_SYSKEYUP:
				if (wParam < 256)
					io.KeysDown[wParam] = 0;
				break;
			case WM_CHAR:
				if (wParam > 0 && wParam < 0x10000)
					io.AddInputCharacterUTF16((unsigned short)wParam);
				break;
			}

		}
		ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
		LPTSTR win32_cursor = IDC_ARROW;
		switch (imgui_cursor)
		{
		case ImGuiMouseCursor_Arrow:        win32_cursor = IDC_ARROW; break;
		case ImGuiMouseCursor_TextInput:    win32_cursor = IDC_IBEAM; break;
		case ImGuiMouseCursor_ResizeAll:    win32_cursor = IDC_SIZEALL; break;
		case ImGuiMouseCursor_ResizeEW:     win32_cursor = IDC_SIZEWE; break;
		case ImGuiMouseCursor_ResizeNS:     win32_cursor = IDC_SIZENS; break;
		case ImGuiMouseCursor_ResizeNESW:   win32_cursor = IDC_SIZENESW; break;
		case ImGuiMouseCursor_ResizeNWSE:   win32_cursor = IDC_SIZENWSE; break;
		case ImGuiMouseCursor_Hand:         win32_cursor = IDC_HAND; break;
		case ImGuiMouseCursor_NotAllowed:   win32_cursor = IDC_NO; break;
		}
		oSetCursor(::LoadCursor(NULL, win32_cursor));
		if (msg == WM_KEYUP) return CallWindowProc(oWindowProc, hWnd, msg, wParam, lParam);
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return CallWindowProc(oWindowProc, hWnd, msg, wParam, lParam);
}

void RenderText(ImDrawList* drawList, const char* text, const ImVec2& screen, const bool centered, const bool outlined, const float size, const ImVec4& color)
{
	auto window = ImGui::GetCurrentWindow();
	float fSize = size * cfg->client.ClientTextSize;
	if (fSize == 0) fSize = size;

	auto ImScreen = *reinterpret_cast<const ImVec2*>(&screen);
	if (centered)
	{
		auto size = ImGui::CalcTextSize(text);
		ImScreen.x -= size.x * 0.5f;
	}

	if (outlined)
	{
		drawList->AddText(nullptr, fSize, ImVec2(ImScreen.x - 1.f, ImScreen.y - 1.f), ImGui::GetColorU32(IM_COL32_BLACK), text);
		drawList->AddText(nullptr, fSize, ImVec2(ImScreen.x + 1.f, ImScreen.y + 1.f), ImGui::GetColorU32(IM_COL32_BLACK), text);
	}
	drawList->AddText(nullptr, fSize, ImScreen, ImGui::GetColorU32(color), text);

}

void RenderPin(ImDrawList* drawList, const ImVec2& ImScreen, const ImVec4& color, const float radius)
{
	drawList->AddCircle(ImVec2(ImScreen.x, ImScreen.y), radius, ImGui::GetColorU32(color), 32, 7.f);
}

void Render2DBox(ImDrawList* drawList, const ImVec2& top, const ImVec2& bottom, const float height, const float width, const ImVec4& color)
{
	drawList->AddRect({ top.x - width * 0.5f, top.y }, { top.x + width * 0.5f, bottom.y }, ImGui::GetColorU32(color), 0.f, 15, 1.5f);
}

void render(ImDrawList* drawList)
{
	if (cfg->client.enable)
	{
		ImVec2 screen = ImVec2(100, 100);
		RenderText(drawList, "Master Duel Tool Client ON", screen);
	}
	if (cfg->esp.enable)
	{
		ImVec2 screen = ImVec2(100, 120);
		RenderText(drawList, "Master Duel Tool ESP ON", screen);

		if (cfg->esp.bShowDuelTime && bIsIngame)
		{
			UINT TimeLeft = SDK::PVP_DuelInfoTimeLeft();
			UINT TimeTotal = SDK::PVP_DuelInfoTimeTotal();

			screen = ImVec2(100, 160);
			char BufferDuelTimer[0x64];
			ZeroMemory(BufferDuelTimer, sizeof(BufferDuelTimer));
			sprintf_s(BufferDuelTimer, sizeof(BufferDuelTimer), "Duel Time [%d/%d]", TimeLeft, TimeTotal);
			RenderText(drawList, BufferDuelTimer, screen);
		}
	}

}

HRESULT presentHook(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
	if (!running) return oDX11Present(swapChain, syncInterval, flags);

	if (!device)
	{
		ID3D11Texture2D* buffer;

		swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer));
		swapChain->GetDevice(IID_PPV_ARGS(&device));
		device->CreateRenderTargetView(buffer, nullptr, &rtv);
		device->GetImmediateContext(&context);

		safe_release(buffer);

		DXGI_SWAP_CHAIN_DESC desc;
		swapChain->GetDesc(&desc);
		auto& window = desc.OutputWindow;
		g_TargetWindow = window;

		if (oWindowProc && g_TargetWindow)
		{
			SetWindowLongPtrA(g_TargetWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWindowProc));
			oWindowProc = nullptr;
		}
		if (g_TargetWindow) {
			oWindowProc = (WNDPROC)SetWindowLongPtr(g_TargetWindow, GWLP_WNDPROC, (LONG_PTR)windowProcHookEx);
			std::cout << "Original WndProc function: " << std::hex << oWindowProc << std::endl;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		ImGuiIO& io = ImGui::GetIO();
		static const ImWchar icons_ranges[] = { 0xf000, 0xf3ff, 0 };
		ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Georgia.ttf", 20.f);
		ImFontConfig config;
		config.MergeMode = true;
		io.Fonts->AddFontFromMemoryCompressedTTF(font_awesome_data, font_awesome_size, 25.f, &config, icons_ranges);
		io.Fonts->Build();

		ImGui_ImplWin32_Init(g_TargetWindow);
		ImGui_ImplDX11_Init(device, context);
		ImGui_ImplDX11_CreateDeviceObjects();
	}


	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();


	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
	ImGui::Begin("overlay", nullptr, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar);
	auto& io = ImGui::GetIO();
	ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);

	auto drawList = ImGui::GetCurrentWindow()->DrawList;

	render(drawList);

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	if (showMenu)
	{
		ImGui::SetNextWindowSize(ImVec2(1280, 720), ImGuiCond_Once);
		ImGui::Begin("Master Duel Tool", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
		ImGuiStyle* style = &ImGui::GetStyle();
		style->WindowPadding = ImVec2(8, 8);
		style->WindowRounding = 7.0f;
		style->FramePadding = ImVec2(4, 3);
		style->FrameRounding = 0.0f;
		style->ItemSpacing = ImVec2(6, 4);
		style->ItemInnerSpacing = ImVec2(4, 4);
		style->IndentSpacing = 20.0f;
		style->ScrollbarSize = 14.0f;
		style->ScrollbarRounding = 9.0f;
		style->GrabMinSize = 5.0f;
		style->GrabRounding = 0.0f;
		style->WindowBorderSize = 0;
		style->WindowTitleAlign = ImVec2(0.0f, 0.5f);
		style->FramePadding = ImVec2(4, 3);
		style->Colors[ImGuiCol_TitleBg] = ImColor(75, 5, 150, 225);
		style->Colors[ImGuiCol_TitleBgActive] = ImColor(75, 75, 150, 225);
		style->Colors[ImGuiCol_Button] = ImColor(75, 5, 150, 225);
		style->Colors[ImGuiCol_ButtonActive] = ImColor(75, 75, 150, 225);
		style->Colors[ImGuiCol_ButtonHovered] = ImColor(41, 40, 41, 255);
		style->Colors[ImGuiCol_Separator] = ImColor(70, 70, 70, 255);
		style->Colors[ImGuiCol_SeparatorActive] = ImColor(76, 76, 76, 255);
		style->Colors[ImGuiCol_SeparatorHovered] = ImColor(76, 76, 76, 255);
		style->Colors[ImGuiCol_Tab] = ImColor(75, 5, 150, 225);
		style->Colors[ImGuiCol_TabHovered] = ImColor(75, 75, 150, 225);
		style->Colors[ImGuiCol_TabActive] = ImColor(110, 110, 250, 225);
		style->Colors[ImGuiCol_SliderGrab] = ImColor(75, 75, 150, 225);
		style->Colors[ImGuiCol_SliderGrabActive] = ImColor(110, 110, 250, 225);
		style->Colors[ImGuiCol_MenuBarBg] = ImColor(76, 76, 76, 255);
		style->Colors[ImGuiCol_FrameBg] = ImColor(37, 36, 37, 255);
		style->Colors[ImGuiCol_FrameBgActive] = ImColor(37, 36, 37, 255);
		style->Colors[ImGuiCol_FrameBgHovered] = ImColor(37, 36, 37, 255);
		style->Colors[ImGuiCol_Header] = ImColor(0, 0, 0, 0);
		style->Colors[ImGuiCol_HeaderActive] = ImColor(0, 0, 0, 0);
		style->Colors[ImGuiCol_HeaderHovered] = ImColor(46, 46, 46, 255);


		if (ImGui::BeginTabBar("Bars"))
		{
			if (ImGui::BeginTabItem(ICON_FA_SLIDERS_H "Client"))
			{
				ImGui::Text("Global Client");
				if (ImGui::BeginChild("Global Client", ImVec2(400.f, 100.f), false, 0))
				{
					ImGui::Checkbox("Enable", &Config::cfg.client.enable);
					ImGui::Checkbox("Enable Debug Mode (DEV)", &Config::cfg.client.bDebugMode);

				}
				ImGui::EndChild();

				if (ImGui::BeginChild("cGlobal", ImVec2(0.f, 0.f), false, 0))
				{
					ImGui::SliderFloat("Render Text", &Config::cfg.client.ClientTextSize, 0.5f, 2.5f, "%.1f");
					ImGui::ColorEdit4("Render Color", &Config::cfg.client.ClientDefaultColor.x, 0);
					ImGui::SliderFloat("Custom Speed Value 1 (F1)", &Config::cfg.client.CustomTimeScaleVal1, 0.f, 10.f, "%.2f");
					ImGui::SliderFloat("Custom Speed Value 2 (F2)", &Config::cfg.client.CustomTimeScaleVal2, 0.f, 10.f, "%.02f");
					ImGui::SliderFloat("Custom Speed Value 3 (F3)", &Config::cfg.client.CustomTimeScaleVal3, 0.f, 10.f, "%.2f");
					ImGui::SliderFloat("Custom Speed Value 4 (F4)", &Config::cfg.client.CustomTimeScaleVal4, 0.f, 10.f, "%.2f");
					ImGui::Checkbox("Use Custom Speed Vaue", &Config::cfg.client.bCustomTimeScale);
					ImGui::SliderFloat("Custom Speed Value", &Config::cfg.client.CustomTimeScaleVal0, 0.f, 10.f, "%.2f");
				}

				ImGui::Text("");
				ImGui::Separator();
				ImGui::Text("");
				if (ImGui::Button("Save Settings"))
				{
					do {
						wchar_t buf[MAX_PATH];
						GetModuleFileNameW(hInstance, buf, MAX_PATH);
						fs::path path = fs::path(buf).remove_filename() / ".settings";
						auto file = CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
						if (file == INVALID_HANDLE_VALUE) break;
						DWORD written;
						if (WriteFile(file, &Config::cfg, sizeof(Config::cfg), &written, 0)) ImGui::OpenPopup("##SettingsSaved");
						CloseHandle(file);
					} while (false);
				}
				ImGui::SameLine();
				if (ImGui::Button("Load Settings"))
				{
					do {
						wchar_t buf[MAX_PATH];
						GetModuleFileNameW(hInstance, buf, MAX_PATH);
						fs::path path = fs::path(buf).remove_filename() / ".settings";
						auto file = CreateFileW(path.wstring().c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
						if (file == INVALID_HANDLE_VALUE) break;
						DWORD readed;
						if (ReadFile(file, &Config::cfg, sizeof(Config::cfg), &readed, 0))  ImGui::OpenPopup("##SettingsLoaded");
						CloseHandle(file);
					} while (false);
				}
				ImGui::SameLine();
				if (ImGui::Button("Exit (END)"))
				{
					running = false;
				}

				ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
				ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("##SettingsSaved", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
				{
					ImGui::Text("\nSettings Saved!\n\n");
					ImGui::Separator();
					if (ImGui::Button("OK", { 170.f , 0.f })) { ImGui::CloseCurrentPopup(); }
					ImGui::EndPopup();
				}
				ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
				if (ImGui::BeginPopupModal("##SettingsLoaded", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
				{
					ImGui::Text("\nSettings Loaded!\n\n");
					ImGui::Separator();
					if (ImGui::Button("OK", { 170.f , 0.f })) { ImGui::CloseCurrentPopup(); }
					ImGui::EndPopup();
				}

				ImGui::EndChild();

				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem(ICON_FA_EYE "ESP"))
			{
				ImGui::Text("Global ESP");
				if (ImGui::BeginChild("Global ESP", ImVec2(200.f, 50.f), false, 0))
				{
					ImGui::Checkbox("Enable", &Config::cfg.esp.enable);
				}
				ImGui::EndChild();

				if (ImGui::BeginChild("cESP", ImVec2(0.f, 0.f), true, 0))
				{
					ImGui::Checkbox("Show Duel Time", &Config::cfg.esp.bShowDuelTime);

				}
				ImGui::EndChild();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem(ICON_FA_GLOBE "Game"))
			{
				ImGui::Text("Global Game");
				if (ImGui::BeginChild("Global Game", ImVec2(200.f, 50.f), false, 0))
				{
					ImGui::Checkbox("Enable", &Config::cfg.game.enable);

				}
				ImGui::EndChild();

				if (ImGui::BeginChild("cGame", ImVec2(0.f, 0.f), true, 0))
				{

					if (ImGui::Button("1>"))
					{
						ForceQuitMatch();
					}
					ImGui::SameLine();
					ImGui::Text("Quit Match (F5)");

					if (ImGui::Button("2>"))
					{
						SetPlayerType();
					}
					ImGui::SameLine();
					if (Config::cfg.game.bIsPlayerCPU)
						ImGui::Text("Player As Human (F7)");
					else
						ImGui::Text("Player As CPU (F7)");

					if (ImGui::Button("3>"))
					{
						ToggleStallTime();
					}
					ImGui::SameLine();
					ImGui::Text("Stall Time (F8)");
				}
				ImGui::EndChild();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}
	ImGui::Render();
	context->OMSetRenderTargets(1, &rtv, nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	return oDX11Present(swapChain, syncInterval, flags);
}

HRESULT resizeBufferHook(IDXGISwapChain* swapChain, UINT bufferCount, UINT width, UINT height, DXGI_FORMAT newFormat, UINT swapChainFlags)
{
	clearDX11Objects();
	return oDX11ResizeBuffer(swapChain, bufferCount, width, height, newFormat, swapChainFlags);
}

HCURSOR WINAPI hkSetCursor(HCURSOR hCursor)
{
	if (showMenu)
	{
		return NULL;
	}
	return oSetCursor(hCursor);
}

BOOL WINAPI hkSetCursorPos(int x, int y)
{
	if (showMenu)
	{
		return false;
	}
	return oSetCursorPos(x, y);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		hInstance = hModule;
		DisableThreadLibraryCalls(hModule);
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Run, 0, 0, 0);
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

