#pragma once
#include "afxdialogex.h"


// CConfigDlg 对话框

class CConfigDlg : public CDialog
{
	DECLARE_DYNAMIC(CConfigDlg)

public:
	CConfigDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CConfigDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CONFIG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
	CTabCtrl m_tab;
	CEdit m_fpath;
public:
	afx_msg void OnBnClickedChangeFloder();
	afx_msg void OnTcnSelchangeNotifyTab(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBnClickedRun();
	CButton m_br;
	afx_msg void OnBnClickedRestart();
	CButton m_restart;
	afx_msg void OnBnClickedReload();
	CButton m_reload;
	afx_msg void OnBnClickedStop();
	CButton m_stop;
	afx_msg void OnBnClickedStatus();
	afx_msg void OnBnClickedProgf();
	CEdit m_alive;
	CEdit m_mem;
	CButton m_nStatus;
	CButton m_nClose;
	CButton m_nUsbin;
	CButton m_nUsbout;
	CButton m_nDownload;
	CButton m_pDesktop;
	CButton m_pDownload;
	CButton m_noAdmin;
	CEdit m_aDelay;
	afx_msg void OnBnClickedSelectAll();
	afx_msg void OnBnClickedNotSelectAll();
	afx_msg void OnBnClickedSaveProf();
//	afx_msg void OnEnChangeNl1();
	virtual BOOL OnInitDialog();
};
