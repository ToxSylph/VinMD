#include "SDK.h"

namespace SDK
{
	uintptr_t baseAddress = 0;
	uintptr_t unityPlayerBaseAddress = 0;
	bool* bIsInGame = nullptr;

	uintptr_t oMyself = 0x99E220;
	uintptr_t oRival = 0x9B3960;

	uintptr_t oDuelEndStep = 0x976960;

	uintptr_t oGetDuelClientInstanceBase = 0x2CD0068;
	std::vector<unsigned int> oGetDuelClientInstanceOffsets = { 0xb8, 0x0 };

	uintptr_t oTimeDeltaBase = 0x1C907D8;
	std::vector<unsigned int> oTimeDeltaOffsets = { 0xfc };

	uintptr_t oPVP_DuelGetCardUniqueID = 0x9A0DB0;

	uintptr_t oPVP_DuelGetCardIDByUniqueID = 0x9A0810;
	uintptr_t oTHREAD_DuelGetCardIDByUniqueID = 0x9B89C0;

    uintptr_t oPVP_DuelInfoTimeLeft = 0x9A2F20;
	uintptr_t oPVP_DuelInfoTimeTotal = 0x9A2FE0;

	uintptr_t oSetPlayerType = 0x9B5500;



	EngineCreateFunc ohkEngineCreateFunc = nullptr;
	EngineDestroyFunc ohkEngineDestroyFunc = nullptr;
	uintptr_t oEngineCreate = 0x9890F0;
	uintptr_t oEngineDestroy= 0x98E460;


	uintptr_t ResolveAddr(uintptr_t ptr, std::vector<unsigned int> offsets)
	{
		uintptr_t addr = ptr;
		for (unsigned int i = 0; i < offsets.size(); ++i)
		{
			addr = *(uintptr_t*)addr;
			addr += offsets[i];
		}
		return addr;
	}
	uintptr_t Aobs(PCHAR pattern, PCHAR mask, uintptr_t begin, SIZE_T size)
	{
		SIZE_T patternSize = strlen((char*)mask);

		for (int i = 0; i < size; i++)
		{
			bool match = true;
			for (int j = 0; j < patternSize; j++)
			{
				if (*(char*)((uintptr_t)begin + i + j) != pattern[j] && mask[j] != '?')
				{
					match = false;
					break;
				}
			}
			if (match) return (begin + i);
		}
		return 0;
	}


	int Myself()
	{
		uintptr_t address = baseAddress + oMyself;
		GetIntegerNoParamsFunc fPlayer = reinterpret_cast<GetIntegerNoParamsFunc>(address);

		return fPlayer();
	}

	int Rival()
	{
		uintptr_t address = baseAddress + oRival;
		GetIntegerNoParamsFunc fRival = reinterpret_cast<GetIntegerNoParamsFunc>(address);

		return fRival();
	}

	bool DuelEndStep()
	{
		uintptr_t duelClientInstance = GetDuelClientInstance();
		if (duelClientInstance == 0) return false;

		DuelEndStepFunc DuelEndStep = (DuelEndStepFunc)(baseAddress + oDuelEndStep);
		DuelEndStep((void*)duelClientInstance);
		return true;
	}

	uintptr_t GetDuelClientInstance()
	{
		uintptr_t base = baseAddress + oGetDuelClientInstanceBase;

		uintptr_t address = ResolveAddr(base, oGetDuelClientInstanceOffsets);

		return *(uintptr_t*)address;
	}

	bool SetGameSpeed(float value)
	{
		uintptr_t base = unityPlayerBaseAddress + oTimeDeltaBase;

		uintptr_t address = ResolveAddr(base, oTimeDeltaOffsets);

		if (!address) return false;

		*(float*)address = value;

		return true;
	}

	UINT PVP_DuelInfoTimeLeft()
	{
		PVP_DuelInfoTimeLeftFunc TimeLeft = (PVP_DuelInfoTimeLeftFunc)(SDK::baseAddress + oPVP_DuelInfoTimeLeft);

		return TimeLeft();
	}
	UINT PVP_DuelInfoTimeTotal()
	{
		PVP_DuelInfoTimeTotalFunc TimeTotal = (PVP_DuelInfoTimeTotalFunc)(SDK::baseAddress + oPVP_DuelInfoTimeTotal);

		return TimeTotal();
	}

	void hkEngineCreateFunc(int gamemode, bool isOnline)
	{
		*bIsInGame = true;
		return ohkEngineCreateFunc(gamemode, isOnline);
	}
	void hkEngineDestroyFunc()
	{
		*bIsInGame = false;
		return ohkEngineDestroyFunc();
	}


	std::vector<int> GetCardsUniqueIds(bool myself, int index)
	{
		int PID = myself ? Myself() : Rival();
		std::vector<int> uniqueIDs;

		int maxSize = 0;

		switch (index)
		{
		case 13: // Hand
			maxSize = 10;
			break;
		case 14: // Extra Deck
			maxSize = 15;
			break;
		case 15: // Deck
			maxSize = 40;
			break;
		case 16: // GY
			maxSize = 10;
			break;
		default:
			maxSize = 1;
			break;
		}

		PVP_DuelGetCardUniqueIDFunc PVP_DuelGetCardUniqueID = (PVP_DuelGetCardUniqueIDFunc)(baseAddress + oPVP_DuelGetCardUniqueID);

		if (index < 13)
		{
			for (int i = 0; i < maxSize; i++)
			{
				int uniqueID = PVP_DuelGetCardUniqueID(PID, i, 0);
				uniqueIDs.push_back(uniqueID);
			}
		}
		else
		{
			for (int i = 0; i < maxSize; i++)
			{
				int uniqueID = PVP_DuelGetCardUniqueID(PID, index, i);
				uniqueIDs.push_back(uniqueID);
			}
		}
		return uniqueIDs;
	}

	unsigned int GetCardsIDsByUniqueIDs(int uniqueId, bool online)
	{
		DuelGetCardIDByUniqueIDFunc DuelGetCardIDByUniqueID = nullptr;
		if (online)
		{
			DuelGetCardIDByUniqueID = (DuelGetCardIDByUniqueIDFunc)(baseAddress + oPVP_DuelGetCardIDByUniqueID);
		}
		else
		{
			DuelGetCardIDByUniqueID = (DuelGetCardIDByUniqueIDFunc)(baseAddress + oTHREAD_DuelGetCardIDByUniqueID);
		}
		if(!DuelGetCardIDByUniqueID) return 0;

		return DuelGetCardIDByUniqueID(uniqueId);
	}

	void SetPlayerType(int player, int type)
	{
		uintptr_t address = baseAddress + oSetPlayerType;

		((void(*)(int, int))address)(player, type);
	}
}