#pragma once
#include "afxdialogex.h"
#include "CNotifySetting.h"


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
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	CNotifySetting* m_dt = nullptr;
	CTabCtrl m_tab;
	CEdit m_fpath;
	CButton m_changef;
	CButton m_mdf;
	CButton m_nwc;
	CButton m_nui;
	CButton m_nuo;
	CButton m_ns;
	CButton m_ndf;
	afx_msg void OnBnClickedSave();
	afx_msg void OnBnClickedStop();
	afx_msg void OnBnClickedLoadConfig();
	afx_msg void OnBnClickedRestart();
	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedChangePath();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
};
