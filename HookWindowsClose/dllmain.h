#pragma once
#pragma once

#ifdef HOOKWINDOWSCLOSE_EXPORTS
#define HOOKLIBRARY_API __declspec(dllexport)
#else
#define HOOKLIBRARY_API __declspec(dllimport)
#endif



extern "C" {
    HOOKLIBRARY_API BOOL InstallHook();
    HOOKLIBRARY_API BOOL UninstallHook();
    HOOKLIBRARY_API BOOL IsHookInstalled();
}