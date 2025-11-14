#include "pch.h"
#include "framework.h"

#ifdef PLUGINS_EXPORTS
#define PLUGINS_API __declspec(dllexport)
#else
#define PLUGINS_API __declspec(dllimport)
#endif


enum NotificationMode {
    KILL_WINDOW = 1,
    INIT = 2,
    USB_IN = 3,
    USB_OUT = 4,
    DOWNLOAD_NEW_FILE = 5,
};
template <typename T>
class MessageQueue {
public:
    void push(const T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        _queue.push(msg);
        _cv.notify_one();
    }

    bool poll(T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        if (!_queue.empty()) {
            msg = _queue.front();
            _queue.pop();
            return true;
        }
        return false;
    }

    void wait(T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        while (_queue.empty()) _cv.wait(lck);
        msg = _queue.front();
        _queue.pop();
    }

private:
    std::queue<T> _queue;
    std::mutex _mtx;
    std::condition_variable _cv;
};

class DirectoryWatcher {
private:
    HANDLE m_directoryHandle;
    HANDLE m_completionPort;
    OVERLAPPED m_overlapped;
    std::wstring m_directoryPath;
    std::atomic<bool> m_watching;
    std::thread m_watchThread;

    // 回调函数类型
    std::function<void(DWORD, const std::wstring&,MessageQueue<std::wstring> &msg)> m_callback;

    // 事件用于停止监控
    HANDLE m_stopEvent;

    // 缓冲区大小
    static const DWORD BUFFER_SIZE = 64 * 1024;

public:
    MessageQueue<std::wstring> filequeue;

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
        std::function<void(DWORD, const std::wstring&,MessageQueue<std::wstring> &msg)> callback) {
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
                m_callback(notifyInfo->Action, fileName,filequeue);
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
};

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
};

extern "C" {
    PLUGINS_API int Adminmain();
}