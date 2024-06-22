#pragma once
#include "windows.h"
#include <vector>
#include "iostream"

namespace SDK
{
	extern uintptr_t baseAddress;

	extern uintptr_t oMyself;
	extern uintptr_t oRival;

	extern uintptr_t oDuelEndStep;

	uintptr_t ResolveAddr(uintptr_t ptr, std::vector<unsigned int> offsets);

	bool Myself();
	bool Rival();
	void DuelEndStep();
	uintptr_t GetDuelClientInstance();
}