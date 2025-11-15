// CNotifySetting.cpp: 实现文件
//

#include "pch.h"
#include "CopyDialog.h"
#include "afxdialogex.h"
#include "CNotifySetting.h"


// CNotifySetting 对话框

IMPLEMENT_DYNAMIC(CNotifySetting, CDialog)

CNotifySetting::CNotifySetting(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_NoticeSettings, pParent)
{

}

CNotifySetting::~CNotifySetting()
{
}

void CNotifySetting::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CNotifySetting, CDialog)
	ON_BN_CLICKED(IDC_SAVE, &CNotifySetting::OnBnClickedSave)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CNotifySetting 消息处理程序

void CNotifySetting::OnBnClickedSave()
{
	// TODO: 在此添加控件通知处理程序代码
}
void CNotifySetting::OnShowWindow(BOOL bShow, UINT nStatus) {
	// TODO: 在此添加控件通知处理程序代码
}
