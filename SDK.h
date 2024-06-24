#pragma once
#include <windows.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <vector>

namespace SDK
{
	static struct DuelClient
	{
		char pad_0x0000[0xE4]; //0x0000
		int pvpErrorCounter; //0x00E4
		bool pvpError; // 0xE8
		bool pvpTimeout; // 0xE9
		char pad_00EA[0x2]; //0xEA
		float replayTimeMargin; // 0xEC
		bool replayRealTime; //0xF0

		char pad_00F1[0xE7]; // 0xF1
		bool resultSending; //0x1D8
		bool resultSended; // 0x1D9
	};

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

	uintptr_t ResolveAddr(uintptr_t ptr, std::vector<unsigned int> offsets);
	uintptr_t Aobs(PCHAR pattern, PCHAR mask, uintptr_t begin, SIZE_T size);


	int Myself();
	int Rival();
	bool DuelEndStep();
	uintptr_t GetDuelClientInstance();
	bool SetGameSpeed(float value);

	UINT PVP_DuelInfoTimeLeft();
	UINT PVP_DuelInfoTimeTotal();

	void hkEngineCreateFunc(int gamemode, bool isOnline);
	void hkEngineDestroyFunc();

	std::vector<int> GetCardsUniqueIds(bool myself, int index);
	unsigned int GetCardsIDsByUniqueIDs(int uniqueId, bool online);

	void SetPlayerType(int player, int type);
}