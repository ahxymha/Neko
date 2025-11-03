#include <windows.h>
#include <thread>
#include <iostream>
#include <sddl.h>
#include <signal.h>
#include <Cfgmgr32.h>
#include<initguid.h>
#include<Usbiodef.h>
#include "../HookWindowsClose/dllmain.h"
#include "../toast/wintoastlib.h"
#include <functional>
#include<Knownfolders.h>
#include <mutex>
#include <queue>

#pragma comment(lib, "Cfgmgr32.lib")

std::mutex NoticeMutex, FileWatchDogMutex;

enum NotificationMode {
    KILL_WINDOW = 1,
    INIT = 2,
    USB_IN = 3,
    USB_OUT = 4,
    DOWNLOAD_NEW_FILE = 5,
}NotifyMode;

class MessageQueue {
public:
    void push(const NotificationMode& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        _queue.push(msg);
        _cv.notify_one();
    }

    bool poll(NotificationMode& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        if (!_queue.empty()) {
            msg = _queue.front();
            _queue.pop();
            return true;
        }
        return false;
    }

    void wait(NotificationMode& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        while (_queue.empty()) _cv.wait(lck);
        msg = _queue.front();
        _queue.pop();
    }

private:
    std::queue<NotificationMode> _queue;
    std::mutex _mtx;
    std::condition_variable _cv;
}mq;

class DirectoryWatcher {
private:
    HANDLE m_directoryHandle;
    HANDLE m_completionPort;
    OVERLAPPED m_overlapped;
    std::wstring m_directoryPath;
    std::atomic<bool> m_watching;
    std::thread m_watchThread;

    // 回调函数类型
    std::function<void(DWORD, const std::wstring&)> m_callback;

    // 事件用于停止监控
    HANDLE m_stopEvent;

    // 缓冲区大小
    static const DWORD BUFFER_SIZE = 64 * 1024;

public:
    DirectoryWatcher() : m_directoryHandle(INVALID_HANDLE_VALUE),
        m_completionPort(NULL),
        m_watching(false),
        m_stopEvent(NULL) {
        ZeroMemory(&m_overlapped, sizeof(m_overlapped));
    }

    ~DirectoryWatcher() {
        StopWatching();
    }

    // 开始监控目录
    bool StartWatching(const std::wstring& directoryPath,
        std::function<void(DWORD, const std::wstring&)> callback) {
        if (m_watching) {
            StopWatching();
        }

        m_directoryPath = directoryPath;
        m_callback = callback;

        // 创建停止事件
        m_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!m_stopEvent) {
            return false;
        }

        // 打开目录
        m_directoryHandle = CreateFileW(
            directoryPath.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL
        );

        if (m_directoryHandle == INVALID_HANDLE_VALUE) {
            std::wcerr << L"无法打开目录: " << directoryPath << L", 错误: " << GetLastError() << std::endl;
            CloseHandle(m_stopEvent);
            m_stopEvent = NULL;
            return false;
        }

        // 创建完成端口
        m_completionPort = CreateIoCompletionPort(m_directoryHandle, NULL, 0, 0);
        if (!m_completionPort) {
            CloseHandle(m_directoryHandle);
            m_directoryHandle = INVALID_HANDLE_VALUE;
            CloseHandle(m_stopEvent);
            m_stopEvent = NULL;
            return false;
        }

        m_watching = true;

        // 启动监控线程
        m_watchThread = std::thread(&DirectoryWatcher::WatchThreadProc, this);

        return true;
    }

    // 停止监控
    void StopWatching() {
        if (!m_watching) return;

        m_watching = false;

        // 设置停止事件
        if (m_stopEvent) {
            SetEvent(m_stopEvent);
        }

        // 向完成端口发送退出消息
        if (m_completionPort) {
            PostQueuedCompletionStatus(m_completionPort, 0, 0, NULL);
        }

        if (m_watchThread.joinable()) {
            m_watchThread.join();
        }

        if (m_directoryHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_directoryHandle);
            m_directoryHandle = INVALID_HANDLE_VALUE;
        }

        if (m_completionPort) {
            CloseHandle(m_completionPort);
            m_completionPort = NULL;
        }

        if (m_stopEvent) {
            CloseHandle(m_stopEvent);
            m_stopEvent = NULL;
        }
    }

    // 检查是否正在监控
    bool IsWatching() const {
        return m_watching;
    }

private:
    // 监控线程函数
    void WatchThreadProc() {
        std::vector<BYTE> buffer(BUFFER_SIZE);
        DWORD bytesReturned;
        ULONG_PTR completionKey;
        OVERLAPPED* overlapped;

        // 开始第一次读取
        if (!ReadDirectoryChanges(buffer)) {
            return;
        }

        while (m_watching) {
            HANDLE waitHandles[2] = { m_completionPort, m_stopEvent };

            // 等待完成端口或停止事件
            DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

            if (!m_watching) break;

            switch (waitResult) {
            case WAIT_OBJECT_0: // 完成端口有消息
            {
                BOOL result = GetQueuedCompletionStatus(
                    m_completionPort,
                    &bytesReturned,
                    &completionKey,
                    &overlapped,
                    0
                );

                if (!result) {
                    DWORD error = GetLastError();
                    if (error != ERROR_OPERATION_ABORTED && error != WAIT_TIMEOUT) {
                        std::wcerr << L"GetQueuedCompletionStatus 失败, 错误: " << error << std::endl;
                    }
                    break;
                }

                if (bytesReturned > 0) {
                    // 处理目录变化
                    ProcessDirectoryChanges(buffer.data(), bytesReturned);

                    // 重新开始监控
                    if (!ReadDirectoryChanges(buffer)) {
                        m_watching = false;
                    }
                }
            }
            break;

            case WAIT_OBJECT_0 + 1: // 停止事件
                m_watching = false;
                break;

            default:
                break;
            }
        }
    }

    // 开始读取目录变化
    bool ReadDirectoryChanges(std::vector<BYTE>& buffer) {
        DWORD bytesReturned;

        BOOL result = ReadDirectoryChangesW(
            m_directoryHandle,
            buffer.data(),
            BUFFER_SIZE,
            TRUE,  // 监控子目录
            FILE_NOTIFY_CHANGE_FILE_NAME |    // 文件创建、删除、重命名
            FILE_NOTIFY_CHANGE_DIR_NAME |     // 目录创建、删除、重命名
            FILE_NOTIFY_CHANGE_ATTRIBUTES |   // 属性变化
            FILE_NOTIFY_CHANGE_SIZE |         // 文件大小变化
            FILE_NOTIFY_CHANGE_LAST_WRITE |   // 最后写入时间
            FILE_NOTIFY_CHANGE_LAST_ACCESS |  // 最后访问时间
            FILE_NOTIFY_CHANGE_CREATION |     // 创建时间
            FILE_NOTIFY_CHANGE_SECURITY,      // 安全描述符变化
            &bytesReturned,
            &m_overlapped,
            NULL
        );

        if (!result) {
            DWORD error = GetLastError();
            if (error != ERROR_IO_PENDING) {
                std::wcerr << L"ReadDirectoryChangesW 失败, 错误: " << error << std::endl;
                return false;
            }
        }

        return true;
    }

    // 处理目录变化
    void ProcessDirectoryChanges(const BYTE* buffer, DWORD bufferSize) {
        const FILE_NOTIFY_INFORMATION* notifyInfo =
            reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer);

        while (true) {
            // 转换文件名
            std::wstring fileName(notifyInfo->FileName,
                notifyInfo->FileNameLength / sizeof(WCHAR));

            // 调用回调函数
            if (m_callback) {
                m_callback(notifyInfo->Action, fileName);
            }

            // 移动到下一个通知
            if (notifyInfo->NextEntryOffset == 0) {
                break;
            }

            notifyInfo = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<const BYTE*>(notifyInfo) +
                notifyInfo->NextEntryOffset
                );
        }
    }
}watchdog;

class WindowsVersionDetector {
private:
    DWORD m_dwMajorVersion;
    DWORD m_dwMinorVersion;
    DWORD m_dwBuildNumber;
    std::wstring m_versionName;

    void Initialize() {
        HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
        if (hMod) {
            typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
            RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");
            if (RtlGetVersion) {
                RTL_OSVERSIONINFOW osvi = { 0 };
                osvi.dwOSVersionInfoSize = sizeof(osvi);
                if (RtlGetVersion(&osvi) == 0) {
                    m_dwMajorVersion = osvi.dwMajorVersion;
                    m_dwMinorVersion = osvi.dwMinorVersion;
                    m_dwBuildNumber = osvi.dwBuildNumber;
                    DetermineVersionName();
                }
            }
        }
    }

    void DetermineVersionName() {
        if (m_dwBuildNumber >= 22000) {
            m_versionName = L"Windows 11";
        }
        else if (m_dwBuildNumber >= 10240) { // Windows 10 起始版本
            m_versionName = L"Windows 10";
        }
        else {
            m_versionName = L"Unknown Windows Version";
        }
    }

public:
    WindowsVersionDetector() {
        Initialize();
    }

    bool IsWindows11() const {
        return m_dwBuildNumber >= 22000;
    }

    bool IsWindows10() const {
        return m_dwBuildNumber >= 10240 && m_dwBuildNumber < 22000;
    }

    DWORD GetBuildNumber() const {
        return m_dwBuildNumber;
    }

    std::wstring GetVersionName() const {
        return m_versionName;
    }

    void PrintVersionInfo() const {
        std::wcout << L"系统版本: " << m_versionName << std::endl;
        std::wcout << L"构建版本: " << m_dwBuildNumber << std::endl;
        std::wcout << L"主版本: " << m_dwMajorVersion << std::endl;
        std::wcout << L"次版本: " << m_dwMinorVersion << std::endl;
    }
}ver;
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
    switch(mode) {
        case KILL_WINDOW:{
            title = L":(";
            message = L"你杀死了一个可爱的小窗口，呜呜呜";
            break;
        }
        case INIT:{
            title = L"8(:D";
            message = L"(=^･ω･^=)";
            break;
        }
        case USB_IN:{
            title = L"8(:D";
            message = L"哇，这是什么东西";
            break;
        }
        case USB_OUT:{
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
    if(ver.IsWindows11()){
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
    }else if(ver.IsWindows10()){
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
	//StopFileWatchDog();
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

void DirectoryChangeCallback(DWORD action, const std::wstring& fileName) {
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

int Usermain()
{
    Sleep(10000);
    TCHAR szPath[MAX_PATH];
    if (GetModuleFileName(NULL, szPath, MAX_PATH)) {
		SHELLEXECUTEINFO sei = { 0 };
		sei.cbSize = sizeof(SHELLEXECUTEINFO);
		sei.lpVerb = L"runas";
		sei.lpFile = szPath;
        sei.nShow = SW_SHOW;
        if (!ShellExecuteEx(&sei)) {
            if(GetLastError() == ERROR_CANCELLED) {
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

int main() {
	setlocale(LC_ALL, "");
    if (isRunningAsAdmin()) {
		Adminmain();
    }
    else {
		Usermain();
    }
}
