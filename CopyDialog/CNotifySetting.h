#pragma once
#include "afxdialogex.h"


// CNotifySetting 对话框

class CNotifySetting : public CDialog
{
	DECLARE_DYNAMIC(CNotifySetting)

public:
	CNotifySetting(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CNotifySetting();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_NoticeSettings };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedSave();
	int selector = 0;
};
