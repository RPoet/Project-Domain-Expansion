#include "Engine/Platform/PlatformDefine.h"

// These exports are required by the D3D12 Agility SDK loader.
extern "C"
{
	__declspec(dllexport) extern const uint32 D3D12SDKVersion = 619;
	__declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}
