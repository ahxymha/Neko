// CConfigDlg.cpp: 实现文件
//

#include "pch.h"
#include "CopyDialog.h"
#include "CNotifySetting.h"
#include "afxdialogex.h"
#include "CConfigDlg.h"
#include <algorithm>
#include <vector>
#include <string>


// CConfigDlg 对话框

IMPLEMENT_DYNAMIC(CConfigDlg, CDialog)

CConfigDlg::CConfigDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_CONFIG, pParent)
{

}

CConfigDlg::~CConfigDlg()
{
}

void CConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NOTIFY_TAB, m_tab);
	DDX_Control(pDX, IDC_FLOSERPATH, m_fpath);
	DDX_Control(pDX, IDC_CHANGEPATH, m_changef);
	DDX_Control(pDX, IDC_MOVE_FILE, m_mdf);
	DDX_Control(pDX, IDC_NOTIFY_WINDOW_CLOSE, m_nwc);
	DDX_Control(pDX, IDC_NOTIFY_USB_IN, m_nui);
	DDX_Control(pDX, IDC_NOTIFY_USB_OUT, m_nuo);
	DDX_Control(pDX, IDC_NOTIFY_START, m_ns);
	DDX_Control(pDX, IDC_NOTIFY_DOWNLOAD, m_ndf);
}

BOOL CConfigDlg::OnInitDialog() {
	CDialog::OnInitDialog();

	std::vector<std::wstring> notifies = { L"Neko启动",L"窗口关闭",L"USB插入",L"USB拔出",L"下载新增" };
	std::reverse(notifies.begin(), notifies.end());
	int i = 0;
	for (auto &notify : notifies) {
		m_tab.InsertItem(i, notify.c_str());
		i++;
	}
	this->m_dt = new CNotifySetting;
	this->m_dt->Create(IDD_NoticeSettings, &this->m_tab);
	CRect winRect;
	this->m_tab.AdjustRect(FALSE, winRect);
	this->m_dt->MoveWindow(winRect);
	this->m_tab.SetCurSel(0);
	this->m_dt->ShowWindow(SW_SHOW);
}

BEGIN_MESSAGE_MAP(CConfigDlg, CDialog)
	ON_BN_CLICKED(IDC_SAVE_SETTING, &CConfigDlg::OnBnClickedSave)
	ON_BN_CLICKED(IDC_STOP, &CConfigDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_LOADCONFIG, &CConfigDlg::OnBnClickedLoadConfig)
	ON_BN_CLICKED(IDC_RESTART, &CConfigDlg::OnBnClickedRestart)
	ON_BN_CLICKED(IDC_START, &CConfigDlg::OnBnClickedStart)
	ON_BN_CLICKED(IDC_CHANGEPATH, &CConfigDlg::OnBnClickedChangePath)
END_MESSAGE_MAP()


// CConfigDlg 消息处理程序

void CConfigDlg::OnBnClickedSave()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedStop()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedLoadConfig()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedRestart()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedStart()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedChangePath()
{
	// TODO: 在此添加控件通知处理程序代码
}
