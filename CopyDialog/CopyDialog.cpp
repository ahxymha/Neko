// MyDialogDLL.cpp
#include "pch.h"
#include "framework.h"
#include "CopyDialog.h"
#include "CopyDlg.h"
#include "afxdllx.h"
#include<afx.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define _AFXDLL
#include <afxdll_.h>

static AFX_EXTENSION_MODULE MyDialogDLLDLL = { NULL, NULL };

CopyDlg* g_pDialog = NULL;
CString g_strDialogResult;

extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        if (!AfxInitExtensionModule(MyDialogDLLDLL, hInstance))
            return 0;

        new CDynLinkLibrary(MyDialogDLLDLL);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        AfxTermExtensionModule(MyDialogDLLDLL);
    }
    return 1;
}

MYDIALOGDLL_API BOOL ShowMyDialog(HWND hParent)
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState());

    try
    {
        if (g_pDialog != NULL)
        {
            if (g_pDialog->GetSafeHwnd())
                g_pDialog->SetForegroundWindow();
            return TRUE;
        }

        g_pDialog = new CopyDlg(CWnd::FromHandle(hParent));

        if (g_pDialog->DoModal() == IDOK)
        {
            delete g_pDialog;
            g_pDialog = NULL;
            return TRUE;
        }

        delete g_pDialog;
        g_pDialog = NULL;
        return FALSE;
    }
    catch (...)
    {
        return FALSE;
    }
}

