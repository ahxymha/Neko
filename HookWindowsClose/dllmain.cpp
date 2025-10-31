#include "pch.h"

using namespace WinToastLib;

// 全局变量
HHOOK g_hook = nullptr;
HINSTANCE g_hInstance = nullptr;
bool g_isWinToastInitialized = false;


// 钩子回调函数
LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_DESTROYWND) {
        HWND hwnd = (HWND)wParam;

        // 获取窗口标题
        wchar_t windowTitle[256];
        GetWindowTextW(hwnd, windowTitle, 256);

        // 过滤条件
        bool shouldNotify =
            wcslen(windowTitle) > 0 &&                    // 有标题
            IsWindowVisible(hwnd);                     // 可见窗口

        if (shouldNotify) {
            HANDLE hPipe = CreateFile(L"\\\\.\\pipe\\ToastPipe", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            Sleep(500);
			CloseHandle(hPipe);
        }
    }

    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// 导出函数 - 安装钩子
extern "C" __declspec(dllexport) BOOL InstallHook() {
    if (g_hook) {
        return FALSE; // 钩子已安装
    }

    g_hook = SetWindowsHookEx(WH_CBT, CBTProc, g_hInstance, 0);
    return (g_hook != nullptr);
}

// 导出函数 - 卸载钩子
extern "C" __declspec(dllexport) BOOL UninstallHook() {
    if (g_hook) {
        BOOL result = UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;

        // 清理 WinToast
        if (g_isWinToastInitialized) {
            WinToast::instance()->clear();
            g_isWinToastInitialized = false;
        }

        return result;
    }
    return FALSE;
}

// 导出函数 - 获取钩子状态
extern "C" __declspec(dllexport) BOOL IsHookInstalled() {
    return (g_hook != nullptr);
}

// DLL 入口点
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPARAM lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hInstance = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hook) {
            UnhookWindowsHookEx(g_hook);
        }
        // 清理 WinToast
        if (g_isWinToastInitialized) {
            WinToast::instance()->clear();
        }
        break;
    }
    return TRUE;
}