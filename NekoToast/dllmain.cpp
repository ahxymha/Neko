// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

HINSTANCE g_hIns = nullptr;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        g_hIns = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    case DLL_PROCESS_DETACH: {
        BOOL UninstallHook();
    }
        break;
    }
    return TRUE;
}


