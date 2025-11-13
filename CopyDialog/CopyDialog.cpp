
// CopyDialog.cpp: 定义应用程序的类行为。
//

#include "pch.h"
#include "framework.h"

#include<iostream>
#include "CopyDialog.h"
#include "CopyDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include <functional>

std::mutex dsMutex;
// CCopyDialogApp

BEGIN_MESSAGE_MAP(CCopyDialogApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CCopyDialogApp 构造

CCopyDialogApp::CCopyDialogApp()
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的 CCopyDialogApp 对象

CCopyDialogApp theApp;
MessageQueue<std::wstring> mq;

// CCopyDialogApp 初始化
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
            std::wstring relativeFileName(notifyInfo->FileName,
                notifyInfo->FileNameLength / sizeof(WCHAR));

            // 构建完整路径
            std::wstring fullPath = m_directoryPath;
            if (fullPath.back() != L'\\') {
                fullPath += L'\\';
            }
            fullPath += relativeFileName;

            // 调用回调函数
            if (m_callback) {
                m_callback(notifyInfo->Action, fullPath);
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
}dswatch;

std::wstring DesktopFolderPath() {
	PWSTR path = nullptr;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path);
	std::wstring downloadsPath;
	if (SUCCEEDED(result) && path != nullptr) {
		downloadsPath = path;
		CoTaskMemFree(path);
	}
	return downloadsPath;
}

void DesktopWatchDog() {
    auto callback = DesktopChangeCallback;
    PWSTR path = nullptr;
    HRESULT result = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path);
    if (SUCCEEDED(result) && path != nullptr) {
        dswatch.StartWatching(path, callback);
        CoTaskMemFree(path);
    }
}

void DesktopChangeCallback(DWORD action, const std::wstring& fileName) {
	if (action == FILE_ACTION_ADDED) {
		dsMutex.lock();
        mq.push(fileName);
		dsMutex.unlock();
	}
}

BOOL CCopyDialogApp::InitInstance()
{
	// 如果应用程序存在以下情况，Windows XP 上需要 InitCommonControlsEx()
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();


	// 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("应用程序向导生成的本地应用程序"));



	for(;;){
		std::wstring fPath;
		mq.wait(fPath);
		CCopyDlg dlg;
        dlg.SN = fPath;
		m_pMainWnd = &dlg;
		INT_PTR nResponse = dlg.DoModal();
		if (nResponse == IDOK)
		{
			// TODO: 在此放置处理何时用
			//  “确定”来关闭对话框的代码
		}
		else if (nResponse == IDCANCEL)
		{
			// TODO: 在此放置处理何时用
			//  “取消”来关闭对话框的代码
		}
		else if (nResponse == -1)
		{
			TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
			TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
		}
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

