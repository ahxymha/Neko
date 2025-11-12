// CopyDlg.cpp: 实现文件
//

#include "pch.h"
#include "CopyDialog.h"
#include "afxdialogex.h"
#include "CopyDlg.h"
#include <vector>
#include <string>


// CopyDlg 对话框

IMPLEMENT_DYNAMIC(CopyDlg, CDialog)

CopyDlg::CopyDlg(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_DIALOG1, pParent)
{

}

CopyDlg::~CopyDlg()
{
}

void CopyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, fileName);
	DDX_Control(pDX, IDC_LIST1, Selector);
	DDX_Control(pDX, IDOK, done);
}

BEGIN_MESSAGE_MAP(CopyDlg, CDialog)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_CLOSE()
	ON_BN_CLICKED(IDOK, OnClose)
END_MESSAGE_MAP()

BOOL CopyDlg::OnInitDialog() {
	CDialog::OnInitDialog();
	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	Selector.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	Selector.InsertColumn(0, L"学科", LVCFMT_CENTER, 100);
	Selector.InsertColumn(1, L"路径", LVCFMT_LEFT, 400);
	// TODO: 在此添加额外的初始化代码

	std::vector<std::wstring> subjects = { L"语文",L"数学" ,L"英语" ,L"物理" ,L"历史" ,L"化学" ,L"政治" ,L"生物" ,L"地理" ,L"信息技术" ,L"生涯规划" };
	std::reverse(subjects.begin(), subjects.end());

	for (auto& subject : subjects) {
		int n_item;
		n_item = Selector.InsertItem(0, subject.c_str());
		std::wstring subPath = L"D:\\老师文件\\" + subject;
		Selector.SetItemText(n_item, 1, subPath.c_str());
	}



	return TRUE;
}


void CopyDlg::OnPaint()
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

HCURSOR CopyDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// CopyDlg 消息处理程序
