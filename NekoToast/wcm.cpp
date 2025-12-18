#include "pch.h"

using namespace WinToastLib;

// 全局变量
HHOOK g_hook = nullptr;


// 钩子回调函数
LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HCBT_DESTROYWND) {
        HWND hwnd = (HWND)wParam;

        // 获取窗口标题
        wchar_t windowTitle[256];
        GetWindowTextW(hwnd, windowTitle, 256);

        // 过滤条件
        bool shouldNotify =
			wcslen(windowTitle) > 0 &&                  // 有标题
			!(GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD) &&                    // 非子窗口
            IsWindowVisible(hwnd);                     // 可见窗口

        if (shouldNotify) {
            HANDLE hPipe = CreateFile(L"\\\\.\\pipe\\ToastPipe", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            Sleep(100);
			CloseHandle(hPipe);
        }
    }

    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// 导出函数 - 安装钩子
BOOL InstallHook(HINSTANCE hIns) {
    if (g_hook) {
        return FALSE; // 钩子已安装
    }

    g_hook = SetWindowsHookEx(WH_CBT, CBTProc, hIns, 0);
    return (g_hook != nullptr);
}

// 导出函数 - 卸载钩子
BOOL UninstallHook() {
    if (g_hook) {
        BOOL result = UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
        return result;
    }
    return FALSE;
}

// 导出函数 - 获取钩子状态
BOOL IsHookInstalled() {
    return (g_hook != nullptr);
}
