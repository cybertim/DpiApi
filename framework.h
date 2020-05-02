#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif
	//extern __declspec(dllexport) void PrintDpiInfo();
	extern __declspec(dllexport) void SetDPIScaling(INT32 adapterIDHigh, UINT32 adapterIDlow, UINT32 sourceID, UINT32 dpiPercentToSet);
	extern __declspec(dllexport) void RestoreDPIScaling();
	extern __declspec(dllexport) INT32 GetIDHigh(INT32 index);
	extern __declspec(dllexport) INT32 GetIDLow(INT32 index);
	extern __declspec(dllexport) INT32 GetIDSource(INT32 index);
	extern __declspec(dllexport) INT32 GetIndexCount();
#ifdef __cplusplus
}
#endif