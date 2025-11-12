// CopyDialog.h: CopyDialog DLL 的主标头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"		// 主符号

#pragma once


#define MYDIALOGDLL_API __declspec(dllexport)


// 导出函数声明
extern "C" {
	MYDIALOGDLL_API BOOL ShowMyDialog(HWND hParent = NULL);
}

// CCopyDialogApp
// 有关此类实现的信息，请参阅 CopyDialog.cpp
//



class CCopyDialogApp : public CWinApp
{
public:
	CCopyDialogApp();

// 重写
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
