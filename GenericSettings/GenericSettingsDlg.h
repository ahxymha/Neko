
// GenericSettingsDlg.h: 头文件
//

#pragma once


// CGenericSettingsDlg 对话框
class CGenericSettingsDlg : public CDialogEx
{
// 构造
public:
	CGenericSettingsDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_GENERICSETTINGS_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;
	CMenu m_menu;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void ExitClicked();
	afx_msg void OnAbout();
private:
	CTabCtrl m_tab;
};
