
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

BOOL CCopyDialogApp::InitInstance()
{
	if (!isRunningAsAdmin()) {
		Usermain();
	}
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

