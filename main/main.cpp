#include <windows.h>
#include <thread>
#include <iostream>
#include <sddl.h>
#include <signal.h>
#include "../HookWindowsClose/dllmain.h"
#include "../toast/wintoastlib.h"


//// 全局变量
//HMODULE g_hHookLibrary = nullptr;
bool isInitialized = false;
enum NotificationMode {
    KILL_WINDOW = 1,
    INIT = 2,
};
using namespace WinToastLib;

class CustomHandler : public IWinToastHandler {
public:
    void toastActivated() const override {
        // 通知被点击
        std::cout << "通知被点击";
    }

    void toastActivated(int actionIndex) const override {
        // 带操作的通知被点击
    }

    void toastActivated(std::wstring response) const override {
        // 带输入的通知被点击（处理用户输入）
    }

    void toastDismissed(WinToastDismissalReason state) const override {
        // 通知被关闭
        switch (state) {
        case UserCanceled:
            break;
        case ApplicationHidden:
            break;
        case TimedOut:
            break;
        }
    }

    void toastFailed() const override {
        std::cerr << "通知失败";
    }
};
bool InitTostNotification() {
    WinToast::instance()->setAppName(L"Neko");
    WinToast::instance()->setAppUserModelId(WinToast::configureAUMI(L"xymh", L"Neko", L"App", L"1.0.0"));
    if (!WinToast::instance()->initialize()) {
        return false;
    }
    return true;
}
bool ShowToastNotification(int mode) {
    InitTostNotification();
    if (mode == KILL_WINDOW) {
        WinToastTemplate _template = WinToastTemplate(WinToastTemplate::Text04);
        _template.setTextField(L":(", WinToastTemplate::FirstLine);
        _template.setTextField(L"你杀死了一个可爱的小窗口 ", WinToastTemplate::SecondLine);
        _template.setTextField(L"呜呜呜", WinToastTemplate::ThirdLine);
        // 设置过期时间（毫秒）
        _template.setDuration(WinToastTemplate::Duration::Short);
        if (WinToast::instance()->showToast(_template, new CustomHandler()) < 0) {
            std::cerr << "显示失败";
            return false;
        }
        return true;
    }
    else if (mode == INIT) {
        WinToastTemplate _template = WinToastTemplate(WinToastTemplate::Text04);
        _template.setTextField(L"8(:D", WinToastTemplate::FirstLine);
        _template.setTextField(L"(=^･ω･^=)", WinToastTemplate::SecondLine);
        _template.setTextField(L"", WinToastTemplate::ThirdLine);
        // 设置过期时间（毫秒）
        _template.setDuration(WinToastTemplate::Duration::Short);
        if (WinToast::instance()->showToast(_template, new CustomHandler()) < 0) {
            std::cerr << "显示失败";
            return false;
        }
        return true;
    }
    else
    {
        return false;
    }
}
bool PiepSend(handle_t piep) {
    if (piep == nullptr) {
        ShowToastNotification(INIT);
    }
    else {
        ShowToastNotification(KILL_WINDOW);
    }
    return true;
}
bool ToastPiep() {
    std::thread piepThread(PiepSend, nullptr);
    piepThread.detach();
	PSECURITY_DESCRIPTOR pSD = nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(A;;GA;;;WD)(A;;GA;;;BA)", SDDL_REVISION_1, &pSD, nullptr)) {
        return false;
	}
	SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = pSD;
    sa.bInheritHandle = FALSE;
    auto piep = CreateNamedPipeW(
        L"\\\\.\\pipe\\ToastPipe",
        PIPE_ACCESS_INBOUND,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        255,
        512,
        512,
        0,
        &sa
    );
	if (piep == INVALID_HANDLE_VALUE) {
        LocalFree(pSD);
        return false;
    }
    for (;;) {
        BOOL connected = ConnectNamedPipe(piep, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (connected) {
			std::thread piepThread(PiepSend, piep);
			piepThread.detach();
			DisconnectNamedPipe(piep);
        }
    }
	LocalFree(pSD);

}
void unhook(int sig) {
    UninstallHook();
	exit(0);
}
int main()
{
	signal(SIGINT, unhook);
    if (!IsHookInstalled()) {
         InstallHook();
     }
	FreeConsole();
	std::thread toastThread(ToastPiep);
	toastThread.detach();
	SetConsoleTitle(L"HookWindowsClose Service");
    //FreeConsole();
    for (;;) {
        if (!IsHookInstalled()) {
            InstallHook();
        }
        Sleep(60000);
    }

    return 0;
}
