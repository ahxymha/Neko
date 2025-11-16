// CConfigDlg.cpp: 实现文件
//

#include "pch.h"
#include "CopyDialog.h"
#include "afxdialogex.h"
#include "CConfigDlg.h"


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
	DDX_Control(pDX, IDC_FPATH, m_fpath);
	DDX_Control(pDX, IDC_RUN, m_br);
	DDX_Control(pDX, IDC_RESTART, m_restart);
	DDX_Control(pDX, IDC_RELOAD, m_reload);
	DDX_Control(pDX, IDC_STOP, m_stop);
	DDX_Control(pDX, IDC_ALIVE, m_alive);
	DDX_Control(pDX, IDC_MEM, m_mem);
	DDX_Control(pDX, IDC_NSTATUS, m_nStatus);
	DDX_Control(pDX, IDC_NCLOSEWINDOW, m_nClose);
	DDX_Control(pDX, IDC_NUSBIN, m_nUsbin);
	DDX_Control(pDX, IDC_NUSBOUT, m_nUsbout);
	DDX_Control(pDX, IDC_NDOWNLOAD, m_nDownload);
	DDX_Control(pDX, IDC_PDESKTOP, m_pDesktop);
	DDX_Control(pDX, IDC_PDOWNLOAD, m_pDownload);
	DDX_Control(pDX, IDC_NOADMIN, m_noAdmin);
	DDX_Control(pDX, IDC_ADMINWAIT, m_aDelay);
}


BEGIN_MESSAGE_MAP(CConfigDlg, CDialog)
	ON_BN_CLICKED(IDC_CHF, &CConfigDlg::OnBnClickedChangeFloder)
	ON_NOTIFY(TCN_SELCHANGE, IDC_NOTIFY_TAB, &CConfigDlg::OnTcnSelchangeNotifyTab)
	ON_BN_CLICKED(IDC_RUN, &CConfigDlg::OnBnClickedRun)
	ON_BN_CLICKED(IDC_RESTART, &CConfigDlg::OnBnClickedRestart)
	ON_BN_CLICKED(IDC_RELOAD, &CConfigDlg::OnBnClickedReload)
	ON_BN_CLICKED(IDC_STOP, &CConfigDlg::OnBnClickedStop)
	ON_BN_CLICKED(IDC_STATUS, &CConfigDlg::OnBnClickedStatus)
	ON_BN_CLICKED(IDC_PROGF, &CConfigDlg::OnBnClickedProgf)
	ON_BN_CLICKED(IDC_SA, &CConfigDlg::OnBnClickedSelectAll)
	ON_BN_CLICKED(IDC_NSA, &CConfigDlg::OnBnClickedNotSelectAll)
	ON_BN_CLICKED(IDC_SAVEME, &CConfigDlg::OnBnClickedSaveProf)
END_MESSAGE_MAP()


// CConfigDlg 消息处理程序

void CConfigDlg::OnBnClickedChangeFloder()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnTcnSelchangeNotifyTab(NMHDR* pNMHDR, LRESULT* pResult)
{
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;
}

void CConfigDlg::OnBnClickedRun()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedRestart()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedReload()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedStop()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedStatus()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedProgf()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedSelectAll()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedNotSelectAll()
{
	// TODO: 在此添加控件通知处理程序代码
}

void CConfigDlg::OnBnClickedSaveProf()
{
	// TODO: 在此添加控件通知处理程序代码
}

BOOL CConfigDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  在此添加额外的初始化

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}
