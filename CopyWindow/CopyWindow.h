// CopyWindow.h: CopyWindow DLL 的主标头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"		// 主符号


class CMyMFCWindow : public CFrameWnd
{
public:
    CMyMFCWindow();

protected:
    DECLARE_MESSAGE_MAP()

public:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnClose();

private:
    CString m_strDisplayText;
    CButton m_btnClose;
};

// CCopyWindowApp
// 有关此类实现的信息，请参阅 CopyWindow.cpp
//

class CCopyWindowApp : public CWinApp
{
public:
	CCopyWindowApp();

// 重写
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
