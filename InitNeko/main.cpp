#include <windows.h>
#include <taskschd.h>
#include <comdef.h>
#include <comutil.h>
#include<iostream>
#include<vector>
#include"auth.h"
#include <string>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comsuppw.lib")

using namespace std;

HRESULT CreateLogonTask() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) return hr;

    ITaskService* pService = NULL;
    ITaskFolder* pRootFolder = NULL;
    IRegisteredTask* pRegisteredTask = NULL;

    // 创建TaskService对象
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
        IID_ITaskService, (void**)&pService);
    if (FAILED(hr)) {
        if (pRegisteredTask) pRegisteredTask->Release();
        if (pRootFolder) pRootFolder->Release();
        if (pService) pService->Release();
        CoUninitialize();
    }

    // 连接到本地任务计划程序
    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) {
        if (pRegisteredTask) pRegisteredTask->Release();
        if (pRootFolder) pRootFolder->Release();
        if (pService) pService->Release();
        CoUninitialize();
    }

    // 获取根文件夹
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) {
        if (pRegisteredTask) pRegisteredTask->Release();
        if (pRootFolder) pRootFolder->Release();
        if (pService) pService->Release();
        CoUninitialize();
    }

    // 创建任务定义
    ITaskDefinition* pTask = NULL;
    hr = pService->NewTask(0, &pTask);
    if (FAILED(hr)) {
        if (pRegisteredTask) pRegisteredTask->Release();
        if (pRootFolder) pRootFolder->Release();
        if (pService) pService->Release();
        CoUninitialize();
    }
    // 设置任务注册信息
    IRegistrationInfo* pRegInfo = NULL;
    hr = pTask->get_RegistrationInfo(&pRegInfo);
    if (SUCCEEDED(hr)) {
        pRegInfo->put_Author(_bstr_t(L"Current User"));
        pRegInfo->put_Description(_bstr_t(L"Run on user logon"));
        pRegInfo->Release();
    }

    // 设置任务主体
    IPrincipal* pPrincipal = NULL;
    hr = pTask->get_Principal(&pPrincipal);
    if (SUCCEEDED(hr)) {
        // 设置运行级别为最低权限
        pPrincipal->put_RunLevel(TASK_RUNLEVEL_LUA);
        pPrincipal->Release();
    }

    // 设置触发器（用户登录时）
    ITriggerCollection* pTriggerCollection = NULL;
    hr = pTask->get_Triggers(&pTriggerCollection);
    if (SUCCEEDED(hr)) {
        ITrigger* pTrigger = NULL;
        hr = pTriggerCollection->Create(TASK_TRIGGER_LOGON, &pTrigger);
        if (SUCCEEDED(hr)) {
            ILogonTrigger* pLogonTrigger = NULL;
            hr = pTrigger->QueryInterface(IID_ILogonTrigger, (void**)&pLogonTrigger);
            if (SUCCEEDED(hr)) {
                pLogonTrigger->Release();
            }
            pTrigger->Release();
        }
        pTriggerCollection->Release();
    }

    // 设置操作（要执行的程序）
    IActionCollection* pActionCollection = NULL;
    hr = pTask->get_Actions(&pActionCollection);
    if (SUCCEEDED(hr)) {
        IAction* pAction = NULL;
        hr = pActionCollection->Create(TASK_ACTION_EXEC, &pAction);
        if (SUCCEEDED(hr)) {
            IExecAction* pExecAction = NULL;
            hr = pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction);
            if (SUCCEEDED(hr)) {
                // 设置要执行的程序路径
                wchar_t propath[MAX_PATH] = {  };
                GetModuleFileName(NULL, propath, MAX_PATH);
                std::wstring wpath = propath;
                wpath.erase(wpath.size() - 12, 12);
                wpath += L"Neko.exe";
                pExecAction->put_Path(_bstr_t(wpath.c_str()));
                pExecAction->Release();
            }
            pAction->Release();
        }
        pActionCollection->Release();
    }

    // 注册任务
    hr = pRootFolder->RegisterTaskDefinition(
        _bstr_t(L"MyLogonTask"),  // 任务名称
        pTask,
        TASK_CREATE_OR_UPDATE,
        _variant_t(),  // 用户凭据（空表示当前用户）
        _variant_t(),  // 密码
        TASK_LOGON_INTERACTIVE_TOKEN,
        _variant_t(L""),  // 空表示当前用户
        &pRegisteredTask);

    {
        if (pRegisteredTask) pRegisteredTask->Release();
        if (pRootFolder) pRootFolder->Release();
        if (pService) pService->Release();
        CoUninitialize();
    }
    return hr;
}

bool AddCACertificate(const std::vector<BYTE>& certificateData) {
    HCERTSTORE hStore = NULL;
    bool success = false;

    // 打开根证书存储
    hStore = CertOpenSystemStore(NULL, L"ROOT");
    if (!hStore) {
        std::cerr << "无法打开证书存储" << std::endl;
        return false;
    }

    // 添加证书到存储
    PCCERT_CONTEXT pCertContext = CertCreateCertificateContext(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        certificateData.data(),
        certificateData.size()
    );

    if (pCertContext) {
        if (CertAddCertificateContextToStore(
            hStore,
            pCertContext,
            CERT_STORE_ADD_REPLACE_EXISTING,
            NULL
        )) {
            std::cout << "CA证书添加成功" << std::endl;
            success = true;
        }
        else {
            std::cerr << "添加证书失败: " << GetLastError() << std::endl;
        }
        CertFreeCertificateContext(pCertContext);
    }
    else {
        std::cerr << "创建证书上下文失败: " << GetLastError() << std::endl;
    }

    CertCloseStore(hStore, 0);
    return success;
}

std::vector<BYTE> Base64Decode(const std::string& base64Data) {
    DWORD binaryDataSize = 0;

    // 计算解码后数据大小
    if (!CryptStringToBinaryA(
        base64Data.c_str(),
        base64Data.length(),
        CRYPT_STRING_BASE64,
        NULL,
        &binaryDataSize,
        NULL,
        NULL)) {
        throw std::runtime_error("Failed to calculate binary data size");
    }

    // 分配缓冲区并解码
    std::vector<BYTE> binaryData(binaryDataSize);
    if (!CryptStringToBinaryA(
        base64Data.c_str(),
        base64Data.length(),
        CRYPT_STRING_BASE64,
        binaryData.data(),
        &binaryDataSize,
        NULL,
        NULL)) {
        throw std::runtime_error("Failed to decode base64 string");
    }

    binaryData.resize(binaryDataSize);
    return binaryData;
}


int main() {
    CreateLogonTask();
    AddCACertificate(Base64Decode(CERT));
	return 0;
}