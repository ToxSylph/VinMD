#pragma once

#include <imgui/imgui.h>

class Config
{
public:
	static inline struct Configuration {
		struct
		{
			bool enable = true;

			float ClientTextSize = 1.f;
			ImVec4 ClientDefaultColor = { 1.f, 1.f, 1.f, 1.f };
			bool bDebugMode = false;
			bool bCustomTimeScale = false;
			float CustomTimeScaleVal1 = 0.1f;
			float CustomTimeScaleVal2 = 1.0f;
			float CustomTimeScaleVal3 = 2.0f;
			float CustomTimeScaleVal4 = 4.0f;
			float CustomTimeScaleVal0 = 1.0f;
			float CustomTimeScaleValCurrent = 1.0f;
		}client;
		struct
		{
			bool enable = false;

			bool bShowDuelTime = false;
		}esp;
		struct
		{
			bool enable = false;

			bool bIsPlayerCPU = false;
			bool bIsRivalCPU = false;
			bool bIsBusyCheckBypass = false;
		}game;
	}cfg;
};