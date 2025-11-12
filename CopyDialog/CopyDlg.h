#pragma once
#include "afxdialogex.h"
#include <string>


// CopyDlg 对话框

class CopyDlg : public CDialog
{
	DECLARE_DYNAMIC(CopyDlg)

public:
	CopyDlg(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CopyDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG1 };
#endif
	
	


// 实现
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	CEdit fileName;
	CListCtrl Selector;
	CButton done;
	HICON m_hIcon;
	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	std::wstring DesktopPath, FilePath, FileName;
};
