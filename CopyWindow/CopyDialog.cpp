// CopyDialog.cpp: 实现文件
//

#include "pch.h"
#include "CopyWindow.h"
#include "afxdialogex.h"
#include "CopyDialog.h"


// CopyDialog 对话框

IMPLEMENT_DYNAMIC(CopyDialog, CDialogEx)

CopyDialog::CopyDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG1, pParent)
{

}

CopyDialog::~CopyDialog()
{
}

void CopyDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CopyDialog, CDialogEx)
	ON_LBN_SELCHANGE(IDC_LIST3, &CopyDialog::OnLbnSelchangeList3)
END_MESSAGE_MAP()


// CopyDialog 消息处理程序

void CopyDialog::OnLbnSelchangeList3()
{
	// TODO: 在此添加控件通知处理程序代码
}
