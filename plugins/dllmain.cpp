// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <windows.h>
#include <thread>
#include<string>
#include <iostream>
#include <WinTrust.h>
#include <sddl.h>
#include <signal.h>
#include <Cfgmgr32.h>
#include<initguid.h>
#include <softpub.h>
#include <iomanip>
#include <sstream>
#include<Usbiodef.h>
#include "../HookWindowsClose/dllmain.h"
#include "../toast/wintoastlib.h"
#include <functional>
#include<Knownfolders.h>
#include <cwchar>
#include "method.h"
#include <mutex>
#include <queue>

#pragma comment(lib, "Cfgmgr32.lib")


SERVICE_STATUS g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

std::mutex NoticeMutex, FileWatchDogMutex;

MessageQueue<NotificationMode> mq;

WindowsVersionDetector ver;

DirectoryWatcher watchdog, dsdog;

//// 全局变量
//HMODULE g_hHookLibrary = nullptr;
bool isInitialized = false;
using namespace WinToastLib;

void ShowTrayNotification(NotificationMode mode)
{
    // 查找系统托盘窗口
    HWND hTrayWnd = FindWindow(L"Shell_TrayWnd", NULL);
    if (!hTrayWnd) return;

    HWND hNotifyWnd = FindWindowEx(hTrayWnd, NULL, L"TrayNotifyWnd", NULL);
    if (!hNotifyWnd) return;

    // 创建通知消息
    std::wstring message, title;
    switch (mode) {
    case KILL_WINDOW: {
        title = L":(";
        message = L"你杀死了一个可爱的小窗口，呜呜呜";
        break;
    }
    case INIT: {
        title = L"8(:D";
        message = L"(=^･ω･^=)";
        break;
    }
    case USB_IN: {
        title = L"8(:D";
        message = L"哇，这是什么东西";
        break;
    }
    case USB_OUT: {
        title = L":(";
        message = L"不要带走可爱的USB小盆友啊，呜呜呜";
        break;
    }
    case DOWNLOAD_NEW_FILE: {
        title = L"8(:D";
        message = L"哇，这是什么好东西，快来下载文件夹看看吧！";
        break;
    }
    default:
        return;
        break;
    }
    // 发送通知消息到系统托盘
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hNotifyWnd;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcscpy_s(nid.szInfoTitle, title.c_str());
    wcscpy_s(nid.szInfo, message.c_str());
    nid.uTimeout = 3000; // 3秒

    Shell_NotifyIcon(NIM_ADD, &nid);
    Sleep(3000); // 短暂延迟确保通知显示
    Shell_NotifyIcon(NIM_DELETE, &nid);
}


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
bool ShowToastNotification(NotificationMode mode) {
    if (ver.IsWindows11()) {
        NoticeMutex.lock();
        if (mode == KILL_WINDOW) {
            WinToastTemplate _template = WinToastTemplate(WinToastTemplate::Text04);
            _template.setTextField(L":(", WinToastTemplate::FirstLine);
            _template.setTextField(L"你杀死了一个可爱的小窗口 ", WinToastTemplate::SecondLine);
            _template.setTextField(L"呜呜呜", WinToastTemplate::ThirdLine);
            // 设置过期时间（毫秒）
            _template.setDuration(WinToastTemplate::Duration::Short);
            if (WinToast::instance()->showToast(_template, new CustomHandler()) < 0) {
                std::cerr << "显示失败";
                NoticeMutex.unlock();
                return false;
            }
            NoticeMutex.unlock();
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
                NoticeMutex.unlock();
                return false;
            }
            NoticeMutex.unlock();
            return true;
        }
        else if (mode == USB_IN) {
            WinToastTemplate _template = WinToastTemplate(WinToastTemplate::Text04);
            _template.setTextField(L"8(:D", WinToastTemplate::FirstLine);
            _template.setTextField(L"哇，这是什么东西", WinToastTemplate::SecondLine);
            _template.setTextField(L"让我看看，原来是USB小盆友啊", WinToastTemplate::ThirdLine);
            // 设置过期时间（毫秒）
            _template.setDuration(WinToastTemplate::Duration::Short);
            if (WinToast::instance()->showToast(_template, new CustomHandler()) < 0) {
                std::cerr << "显示失败";
                NoticeMutex.unlock();
                return false;
            }
            NoticeMutex.unlock();
            return true;
        }
        else if (mode == USB_OUT) {
            WinToastTemplate _template = WinToastTemplate(WinToastTemplate::Text04);
            _template.setTextField(L":(", WinToastTemplate::FirstLine);
            _template.setTextField(L"不要带走可爱的USB小盆友啊", WinToastTemplate::SecondLine);
            _template.setTextField(L"呜呜呜", WinToastTemplate::ThirdLine);
            // 设置过期时间（毫秒）
            _template.setDuration(WinToastTemplate::Duration::Short);
            if (WinToast::instance()->showToast(_template, new CustomHandler()) < 0) {
                std::cerr << "显示失败";
                NoticeMutex.unlock();
                return false;
            }
            NoticeMutex.unlock();
            return true;
        }
        else if (mode == DOWNLOAD_NEW_FILE) {
            WinToastTemplate _template = WinToastTemplate(WinToastTemplate::Text04);
            _template.setTextField(L"8(:D", WinToastTemplate::FirstLine);
            _template.setTextField(L"哇，这是什么好东西", WinToastTemplate::SecondLine);
            _template.setTextField(L"快来下载文件夹看看吧！", WinToastTemplate::ThirdLine);
            // 设置过期时间（毫秒）
            _template.setDuration(WinToastTemplate::Duration::Short);
            if (WinToast::instance()->showToast(_template, new CustomHandler()) < 0) {
                std::cerr << "显示失败";
                NoticeMutex.unlock();
                return false;
            }
            NoticeMutex.unlock();
            return true;
        }
        else
        {
            NoticeMutex.unlock();
            return false;
        }
    }
    else if (ver.IsWindows10()) {
        // Windows 10 通知实现（可选）
        ShowTrayNotification(mode);
    }
    else {
        // 不支持的系统版本
        return false;
    }
}

void NotifyServer() {
    for (;;) {
        NotificationMode nm;
        mq.wait(nm);
        InitTostNotification();
        ShowToastNotification(nm);
        Sleep(1000);
    }
}

bool SendToast(NotificationMode mode) {
    mq.push(mode);
    return true;
}

bool ToastPiep() {
    std::thread piepThread(SendToast, INIT);
    piepThread.detach();
    PSECURITY_DESCRIPTOR pSD = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
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
            std::thread piepThread(SendToast, KILL_WINDOW);
            piepThread.detach();
            DisconnectNamedPipe(piep);
        }
    }
    LocalFree(pSD);

}

void UnregDeviceNotice();

void StopFileWatchDog();

void unhook(int sig) {
    UninstallHook();
    UnregDeviceNotice();
    StopFileWatchDog();
    exit(0);
}

void UpdateDevice(const wchar_t* devicePath, BOOL arrival)
{
    NotificationMode piep = arrival ? USB_IN : USB_OUT;
    std::thread piepThread(SendToast, piep);
    piepThread.detach();
}

DWORD CALLBACK DeviceNotificationCallback(
    HCMNOTIFICATION hNotify,
    PVOID Context,
    CM_NOTIFY_ACTION Action,
    PCM_NOTIFY_EVENT_DATA EventData,
    DWORD EventDataSize
)
{
    switch (Action)
    {
    case CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL:
        UpdateDevice(EventData->u.DeviceInterface.SymbolicLink, TRUE);
        break;
    case CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL:
        UpdateDevice(EventData->u.DeviceInterface.SymbolicLink, FALSE);
        break;
    default:
        break;
    }
    return ERROR_SUCCESS;
}

HCMNOTIFICATION hNotifications;
void RegDeviceNotice() {
    // 注册设备通知的代码
    CM_NOTIFY_FILTER notifyFilter = { 0 };
    notifyFilter.cbSize = sizeof(CM_NOTIFY_FILTER);
    notifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    notifyFilter.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_USB_DEVICE;

    CM_Register_Notification(
        &notifyFilter,
        nullptr, // No context needed
        DeviceNotificationCallback,
        &hNotifications
    );
}

void UnregDeviceNotice() {
    // 注销设备通知的代码
    if (hNotifications != nullptr) {
        CM_Unregister_Notification(hNotifications);
        hNotifications = nullptr;
    }
}

std::wstring DownloadsFolderPath() {
    PWSTR path = nullptr;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path);
    std::wstring downloadsPath;
    if (SUCCEEDED(result) && path != nullptr) {
        downloadsPath = path;
        CoTaskMemFree(path);
    }
    return downloadsPath;
}

void DirectoryChangeCallback(DWORD action, const std::wstring& fileName,MessageQueue<std::wstring> &msg) {
    if (action == FILE_ACTION_ADDED) {
        FileWatchDogMutex.lock();
        SHELLEXECUTEINFO sei = { 0 };
        sei.cbSize = sizeof(SHELLEXECUTEINFO);
        sei.lpVerb = L"explore";
        auto dpath = DownloadsFolderPath();
        sei.lpFile = dpath.c_str();
        sei.nShow = SW_SHOW;
        ShellExecuteEx(&sei);
        std::thread FileTotasThread(SendToast, DOWNLOAD_NEW_FILE);
        FileTotasThread.detach();
        FileWatchDogMutex.unlock();
    }
}

void FileWatchDog() {
    auto callback = DirectoryChangeCallback;
    PWSTR path = nullptr;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path);
    if (SUCCEEDED(result) && path != nullptr) {
        watchdog.StartWatching(path, callback);
        CoTaskMemFree(path);
    }
}

void StopFileWatchDog() {
    watchdog.StopWatching();
}

HANDLE OpenProcessAndCreatePipe() {
    HANDLE w_hd, r_hd;
    PSECURITY_DESCRIPTOR pSD = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(A;;GA;;;WD)(A;;GA;;;BA)", SDDL_REVISION_1, &pSD, nullptr)) {
        return nullptr;
    }
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = pSD;
    sa.bInheritHandle = true;
    if (!CreatePipe(&r_hd, &w_hd, &sa, 8192)) {
        return nullptr;
    }
    TCHAR szPath[MAX_PATH];
    if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
        return nullptr;
    }
    for (int i = 1; i <= 3; i++) {
        wchar_t args = *(std::to_wstring(i).c_str());

    }
}

int Adminmain()
{
    signal(SIGINT, unhook);
    if (!IsHookInstalled()) {
        InstallHook();
    }
    FreeConsole();
    std::thread toastThread(ToastPiep);
    toastThread.detach();
    std::thread deviceThread(RegDeviceNotice);
    deviceThread.detach();
    std::thread fileWatchDogThread(FileWatchDog);
    fileWatchDogThread.detach();
    std::thread NotifyServerThread(NotifyServer);
    NotifyServerThread.detach();
    //FreeConsole();
    for (;;) {
        if (!IsHookInstalled()) {
            InstallHook();
        }
        Sleep(60000);
    }

    return 0;
}

int Usermain()
{
    Sleep(2000);
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(NULL, szPath, MAX_PATH)) {
        SHELLEXECUTEINFO sei = { 0 };
        sei.cbSize = sizeof(SHELLEXECUTEINFO);
        sei.lpVerb = L"runas";
        sei.lpFile = szPath;
        sei.nShow = SW_SHOW;
        if (!(&sei)) {
            if (GetLastError() == ERROR_CANCELLED) {
                std::wcerr << L"用户取消了管理员权限请求。" << std::endl;
            }
            else {
                std::wcerr << L"提升权限失败，错误代码: " << GetLastError() << std::endl;
                exit(EXIT_FAILURE);
            }
        }
    }
    else {
        std::wcerr << L"无法获取可执行文件路径，错误代码: " << GetLastError() << std::endl;
        exit(EXIT_FAILURE);
    }
}

bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

