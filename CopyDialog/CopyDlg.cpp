
// CopyDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "CopyDialog.h"
#include "CopyDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include <vector>
#include <string>
#include <algorithm>


// CCopyDlg 对话框


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
std::wstring dsp = DsFolderPath();

CCopyDlg::CCopyDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_COPYDIALOG_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CCopyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, SubSelctor);
	DDX_Control(pDX, IDC_EDIT1, FileName);
}

BEGIN_MESSAGE_MAP(CCopyDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(NM_CLICK, IDC_LIST1, &CCopyDlg::OnNMClickList1)
END_MESSAGE_MAP()


// CCopyDlg 消息处理程序

BOOL CCopyDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
	
	this->SetWindowTextW(L"Neko");

	SubSelctor.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	SubSelctor.InsertColumn(0, L"学科", LVCFMT_CENTER, 100);
	SubSelctor.InsertColumn(1, L"路径", LVCFMT_LEFT, 400);
	// TODO: 在此添加额外的初始化代码

	std::vector<std::wstring> subjects = { L"语文",L"数学" ,L"英语" ,L"物理" ,L"历史" ,L"化学" ,L"政治" ,L"生物" ,L"地理" ,L"信息技术" ,L"生涯规划" };
	std::reverse(subjects.begin(), subjects.end());

	for (auto& subject : subjects) {
		int n_item;
		n_item = SubSelctor.InsertItem(0, subject.c_str());
		std::wstring subPath = dsp + L"\\" + subject;
		SubSelctor.SetItemText(n_item, 1, subPath.c_str());
	}
	
	FileName.SetWindowTextW(SN.c_str());

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CCopyDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CCopyDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CCopyDlg::OnNMClickList1(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
}
