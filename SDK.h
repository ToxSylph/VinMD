#pragma once
#include <windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <vector>

namespace SDK
{

	typedef int(*GetIntegerNoParamsFunc)();

	typedef void(*DuelEndStepFunc)(void*);
	typedef int(*PVP_DuelGetCardUniqueIDFunc)(int, int, int);
	typedef unsigned int(*DuelGetCardIDByUniqueIDFunc)(int);
	typedef void(*SetPlayerTypeFunc)(int, int);

	typedef void(*EngineCreateFunc)(int, bool);
	typedef void(*EngineDestroyFunc)();

	typedef int(__fastcall* PVP_DuelInfoTimeLeftFunc)();
	typedef int(__fastcall* PVP_DuelInfoTimeTotalFunc)();

	extern uintptr_t baseAddress;
	extern uintptr_t unityPlayerBaseAddress;
	extern bool* bIsInGame;

	extern uintptr_t oMyself;
	extern uintptr_t oRival;

	extern uintptr_t oDuelEndStep;

	extern uintptr_t oGetDuelClientInstanceBase;
	extern std::vector<unsigned int> oGetDuelClientInstanceOffsets;

	extern uintptr_t oIsBusyCheckBoolBase;
	extern std::vector<unsigned int> oIsBusyCheckBoolOffsets;

	extern uintptr_t oTimeDeltaBase;
	extern std::vector<unsigned int> oTimeDeltaOffsets;

	extern uintptr_t oPVP_DuelGetCardUniqueID;
	extern uintptr_t oPVP_DuelGetCardIDByUniqueID;

	extern uintptr_t oPVP_DuelInfoTimeLeft;
	extern uintptr_t oPVP_DuelInfoTimeTotal;

	extern uintptr_t oSetPlayerType;

	extern EngineCreateFunc ohkEngineCreateFunc;
	extern EngineDestroyFunc ohkEngineDestroyFunc;
	extern uintptr_t oEngineCreate;
	extern uintptr_t oEngineDestroy;

	extern uintptr_t oEngineIsBusyCheckBypass;

	typedef int(*EngineApiUtilGetCardUniqueId)(int, int, int);
	extern uintptr_t oEngineApiUtilGetCardUniqueId;

	uintptr_t ResolveAddr(uintptr_t ptr, std::vector<unsigned int> offsets);
	uintptr_t Aobs(PCHAR pattern, PCHAR mask, uintptr_t begin, SIZE_T size);
	void HP(PBYTE destination, PBYTE source, SIZE_T size, BYTE* oldBytes);
	void Nop(PBYTE destination, SIZE_T size, BYTE* oldBytes);

	int Myself();
	int Rival();
	bool DuelEndStep();
	uintptr_t GetDuelClientInstance();
	bool SetGameSpeed(float value);
	bool SetIsBusyCheckBypass(bool value);

	UINT PVP_DuelInfoTimeLeft();
	UINT PVP_DuelInfoTimeTotal();

	void hkEngineCreateFunc(int gamemode, bool isOnline);
	void hkEngineDestroyFunc();

	void SetPlayerType(int player, int type);
}