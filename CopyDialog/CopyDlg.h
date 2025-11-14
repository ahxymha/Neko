
// CopyDlg.h: 头文件
//

#pragma once
#include<string>


// CCopyDlg 对话框
class CCopyDlg : public CDialog
{
// 构造
public:
	CCopyDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_COPYDIALOG_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg void OnOK();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	std::wstring dsp;
	std::wstring TargetPath;
	std::wstring SN;
	CListCtrl SubSelctor;
	CEdit FileName;
	afx_msg void OnNMClickList1(NMHDR* pNMHDR, LRESULT* pResult);
};
