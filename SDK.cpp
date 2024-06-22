#include "SDK.h"

namespace SDK
{
	uintptr_t baseAddress = 0;

	uintptr_t oMyself = 0x99E220;
	uintptr_t oRival = 0x9B3960;

	uintptr_t oDuelEndStep = 0x976960;

	uintptr_t oGetDuelClientInstanceBase = 0x2CD0068;
	std::vector<unsigned int> oGetDuelClientInstanceOffsets = { 0xb8, 0x0 };

	typedef void(*DuelEndStepFunc)(void*);


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



	bool Myself()
	{
		uintptr_t address = baseAddress + oMyself;

		return *(bool*)address;
	}

	bool Rival()
	{
		uintptr_t address = baseAddress + oRival;

		return *(bool*)address;
	}

	void DuelEndStep()
	{
		uintptr_t duelClientInstance = GetDuelClientInstance();

		DuelEndStepFunc DuelEndStep = (DuelEndStepFunc)(baseAddress + oDuelEndStep);
		DuelEndStep((void*)duelClientInstance);
	}

	uintptr_t GetDuelClientInstance()
	{
		uintptr_t base = baseAddress + oGetDuelClientInstanceBase;

		uintptr_t address = ResolveAddr(base, oGetDuelClientInstanceOffsets);

		return *(uintptr_t*)address;
	}
}