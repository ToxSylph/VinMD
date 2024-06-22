#include "iostream"
#include "SDK.h"

HINSTANCE hInstance;
FILE* file;
bool running = false;


enum ConfigKeys
{
    ExitKey = VK_END,
    ForceQuitKey = VK_F4,
};

bool Exit()
{
    std::cout << "Unloading.." << std::endl;
    Sleep(3000);

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
        std::cout << "Base Address Not Found!" << std::endl;
        return false;
    }

    std::cout << "Base Address Found At: " << std::hex << SDK::baseAddress << std::endl;

	return true;
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
        if (GetAsyncKeyState(ConfigKeys::ForceQuitKey) & 1)
        {
            SDK::DuelEndStep();
		}
	}
    Exit();
}

BOOL APIENTRY DllMain( HMODULE hModule,
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

