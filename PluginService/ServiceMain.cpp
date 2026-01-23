#include <Windows.h>
#include <thread>
#include <TlHelp32.h>
#include <string>
#include <sddl.h>
#include <detours/detours.h>
#include <iostream>
#include <wtsapi32.h>
#include <vector>
#include "../Logger/framework.hpp"
#include <mutex>
#include <sstream>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")

struct CommunicationPipe {
    HANDLE h_IN = nullptr;
    HANDLE h_out = nullptr;
    CommunicationPipe() = default;
    CommunicationPipe(CommunicationPipe&& other) {
        h_IN = other.h_IN;
        h_out = other.h_out;
        other.h_IN = nullptr;
        other.h_out = nullptr;
    }
    CommunicationPipe& operator=(CommunicationPipe&& other) {
        if (this != &other) {
            h_IN = other.h_IN;
            h_out = other.h_out;
            other.h_IN = nullptr;
            other.h_out = nullptr;
        }
        return *this;
    }
    CommunicationPipe(const CommunicationPipe&) = delete;
    CommunicationPipe& operator=(const CommunicationPipe&) = delete;
};

class Host {
private:
    CommunicationPipe cp;
    std::thread Worker;
    std::mutex mtx;
    HANDLE h_stop;
    struct Call {
        enum Provider {
            ConfigMgr,
            PluginMgr,
            Custom
        }pvd;
        std::string callName;
    };
    std::atomic<bool> running;
    std::vector<Call> calllist;

    void WorkerThread() {
        while (WaitForSingleObject(h_stop, 0) == WAIT_TIMEOUT) {
            std::unique_ptr<char> buf(new char[65537]);
            DWORD rd;
            if (!ReadFile(cp.h_IN, buf.get(), 65536, &rd, NULL)) {
                LogClient::error() << "ReadFile Error:" << GetLastError() << std::endl;
                return;
            }
            if (rd == 0) {
                LogClient::error() << "ReadFile Error:" << GetLastError() << std::endl;
                return;
            }
            
        }
    }
public:
    bool SetCommunicationPipe(CommunicationPipe& ocp) {
        cp = std::move(ocp);
        return true;
    }

};

// 获取完整性级别字符串
std::wstring GetIntegrityLevelString(PSID pSid) {
    if (!IsValidSid(pSid)) {
        return L"Invalid SID";
    }

    DWORD dwSubAuthorityCount = *GetSidSubAuthorityCount(pSid);
    if (dwSubAuthorityCount >= 1) {
        PDWORD pSubAuthority = GetSidSubAuthority(pSid, dwSubAuthorityCount - 1);

        switch (*pSubAuthority) {
        case SECURITY_MANDATORY_UNTRUSTED_RID:     return L"Untrusted (0)";
        case SECURITY_MANDATORY_LOW_RID:          return L"Low (4096)";
        case SECURITY_MANDATORY_MEDIUM_RID:       return L"Medium (8192)";
        case SECURITY_MANDATORY_MEDIUM_PLUS_RID:  return L"Medium Plus (8448)";
        case SECURITY_MANDATORY_HIGH_RID:         return L"High (12288)";
        case SECURITY_MANDATORY_SYSTEM_RID:       return L"System (16384)";
        case SECURITY_MANDATORY_PROTECTED_PROCESS_RID: return L"Protected Process (20480)";
        default: {
            wchar_t buffer[64];
            swprintf_s(buffer, L"Unknown (%lu)", *pSubAuthority);
            return buffer;
        }
        }
    }
    return L"Unknown";
}

// 获取当前令牌的完整性级别
std::wstring GetCurrentIntegrityLevel(HANDLE hToken) {
    DWORD dwLengthNeeded = 0;
    GetTokenInformation(hToken, TokenIntegrityLevel, nullptr, 0, &dwLengthNeeded);

    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return L"无法获取";
    }

    std::vector<BYTE> buffer(dwLengthNeeded);
    PTOKEN_MANDATORY_LABEL pTML = (PTOKEN_MANDATORY_LABEL)buffer.data();

    if (GetTokenInformation(hToken, TokenIntegrityLevel, pTML, dwLengthNeeded, &dwLengthNeeded)) {
        return GetIntegrityLevelString(pTML->Label.Sid);
    }

    return L"获取失败";
}

// 将令牌降级为指定的完整性级别
HANDLE DemoteTokenToIntegrityLevel(HANDLE hOriginalToken, DWORD integrityLevelRid) {
    HANDLE hNewToken = nullptr;

    // 1. 复制令牌
    if (!DuplicateTokenEx(hOriginalToken,
        TOKEN_DUPLICATE | TOKEN_ADJUST_DEFAULT | TOKEN_QUERY |
        TOKEN_ADJUST_SESSIONID | TOKEN_ASSIGN_PRIMARY,
        nullptr,
        SecurityImpersonation,
        TokenPrimary,
        &hNewToken)) {
        LogClient::error() << L"令牌复制失败: " << GetLastError() << std::endl;
        return nullptr;
    }

    // 2. 设置完整性级别
    TOKEN_MANDATORY_LABEL tml = { 0 };
    SID_IDENTIFIER_AUTHORITY SIDAuth = SECURITY_MANDATORY_LABEL_AUTHORITY;

    if (AllocateAndInitializeSid(&SIDAuth, 1,
        integrityLevelRid,
        0, 0, 0, 0, 0, 0, 0,
        &tml.Label.Sid)) {

        tml.Label.Attributes = SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED;

        if (!SetTokenInformation(hNewToken,
            TokenIntegrityLevel,
            &tml,
            sizeof(TOKEN_MANDATORY_LABEL))) {
            DWORD error = GetLastError();
            LogClient::error() << L"设置完整性级别失败 (RID=" << integrityLevelRid
                << L"): " << error << std::endl;
            CloseHandle(hNewToken);
            FreeSid(tml.Label.Sid);
            return nullptr;
        }

        FreeSid(tml.Label.Sid);

        // 3. 移除管理员组（如果存在）
        PTOKEN_GROUPS pGroups = nullptr;
        DWORD dwSize = 0;

        // 获取组信息
        GetTokenInformation(hNewToken, TokenGroups, nullptr, 0, &dwSize);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            pGroups = (PTOKEN_GROUPS)malloc(dwSize);
            if (GetTokenInformation(hNewToken, TokenGroups, pGroups, dwSize, &dwSize)) {

                // 构建要删除的管理员组列表
                std::vector<SID_AND_ATTRIBUTES> sidsToDelete;

                for (DWORD i = 0; i < pGroups->GroupCount; i++) {
                    PSID pSid = pGroups->Groups[i].Sid;

                    // 检查是否是管理员组 (S-1-5-32-544)
                    if (IsValidSid(pSid)) {
                        PSID_IDENTIFIER_AUTHORITY pAuthority = GetSidIdentifierAuthority(pSid);

                        // 检查是否是NT Authority (S-1-5)
                        if (pAuthority->Value[0] == 0 &&
                            pAuthority->Value[1] == 0 &&
                            pAuthority->Value[2] == 0 &&
                            pAuthority->Value[3] == 0 &&
                            pAuthority->Value[4] == 0 &&
                            pAuthority->Value[5] == 5) {

                            DWORD dwSubAuthorityCount = *GetSidSubAuthorityCount(pSid);
                            if (dwSubAuthorityCount == 2) {
                                PDWORD pSubAuthority0 = GetSidSubAuthority(pSid, 0);
                                PDWORD pSubAuthority1 = GetSidSubAuthority(pSid, 1);

                                if (*pSubAuthority0 == SECURITY_BUILTIN_DOMAIN_RID &&
                                    *pSubAuthority1 == DOMAIN_ALIAS_RID_ADMINS) {

                                    SID_AND_ATTRIBUTES sidToDelete = { pSid, 0 };
                                    sidsToDelete.push_back(sidToDelete);
                                }
                            }
                        }
                    }
                }

                // 如果找到了管理员组，创建受限令牌
                if (!sidsToDelete.empty()) {
                    HANDLE hRestrictedToken = nullptr;

                    if (CreateRestrictedToken(
                        hNewToken,
                        DISABLE_MAX_PRIVILEGE,          // 禁用所有特权
                        (DWORD)sidsToDelete.size(),     // 要删除的SID数量
                        sidsToDelete.data(),            // 要删除的SID数组
                        0, nullptr,                     // 不删除特权
                        0, nullptr,                     // 不添加受限SID
                        &hRestrictedToken)) {

                        CloseHandle(hNewToken);
                        hNewToken = hRestrictedToken;
                    }
                }
            }
            free(pGroups);
        }

        // 4. 禁用或移除特权
        TOKEN_PRIVILEGES tp = { 0 };
        tp.PrivilegeCount = 0;

        AdjustTokenPrivileges(hNewToken,
            TRUE,        // 禁用所有特权
            &tp,
            0,
            nullptr,
            nullptr);

        return hNewToken;
    }

    CloseHandle(hNewToken);
    return nullptr;
}

// 创建Low令牌的便捷函数
HANDLE CreateLowToken(HANDLE hOriginalToken) {
    return DemoteTokenToIntegrityLevel(hOriginalToken, SECURITY_MANDATORY_LOW_RID);
}

PSID GetAdministratorsSid() {
    PSID pSid = nullptr;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &pSid)) {
        return pSid;
    }
    return nullptr;
}

BOOL ElevateToken() {
    DWORD pid = NULL;
    HANDLE l_processes = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (l_processes != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W processEntry;
        processEntry.dwSize = sizeof(processEntry);

        if (Process32FirstW(l_processes, &processEntry)) {
            do {
                if (_wcsicmp(processEntry.szExeFile, L"lsass.exe") == 0) {
                    pid = processEntry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(l_processes, &processEntry));
        }
        CloseHandle(l_processes);
    }
    if (pid == 0) {
        return FALSE;
    }
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return FALSE;
    }
    LUID luid;
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return FALSE;
    }
    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(hToken);
        return FALSE;
    }
    auto lsassPC = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    HANDLE lsassTK = nullptr;
    if (!OpenProcessToken(lsassPC, TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_QUERY, &lsassTK)) {
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    HANDLE lsassDPTK = nullptr;
    if (!DuplicateTokenEx(lsassTK, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &lsassDPTK)) {
        CloseHandle(lsassTK);
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    LUID luid1;
    if (!LookupPrivilegeValue(NULL, SE_ASSIGNPRIMARYTOKEN_NAME, &luid1)) {
        CloseHandle(lsassDPTK);
        CloseHandle(lsassTK);
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    TOKEN_PRIVILEGES tp1;
    tp1.PrivilegeCount = 1;
    tp1.Privileges[0].Luid = luid1;
    tp1.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(lsassDPTK, FALSE, &tp1, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(lsassDPTK);
        CloseHandle(lsassTK);
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    HANDLE thisThread = GetCurrentThread();
    if (!SetThreadToken(&thisThread, lsassDPTK)) {
        CloseHandle(lsassDPTK);
        CloseHandle(lsassTK);
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    return TRUE;
}

BOOL CreateSandboxEnv(BOOL isMain) {
    HANDLE sandboxToken = nullptr;
    HANDLE thisToken = nullptr;
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    //if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &thisToken)) {
    //    std::wcerr << "OpenProcessToken ERROR" << GetLastError() << std::endl;
    //    return false;
    //}
    if (!WTSQueryUserToken(sessionId, &thisToken)) {
        LogClient::error() << "WTSQueryUserToken ERROR" << GetLastError() << std::endl;
        return false;
    }
    PSID pAdminSid = GetAdministratorsSid();

    if (!pAdminSid) {
        CloseHandle(thisToken);
        return false;
    }

    SID_AND_ATTRIBUTES sidsToDelete = { pAdminSid, 0 };
    if (!CreateRestrictedToken(thisToken, DISABLE_MAX_PRIVILEGE, 1, &sidsToDelete, 0, NULL, 0, NULL, &sandboxToken)) {
        SendlogPP(3, []()->std::string {
            std::stringstream log;
            log << " CreateRestrictedToken ERROR" << GetLastError();
            return log.str();
            }());
        CloseHandle(thisToken);
        return false;
    }
    CloseHandle(thisToken);
    auto LowSandboxToken = CreateLowToken(sandboxToken);
    if (LowSandboxToken == nullptr) {
        return false;
    }
    CommunicationPipe cp_host;
    CommunicationPipe cp_client;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;BU)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL)) {
        LogClient::error() << "ConvertStringSecurityDescriptorToSecurityDescriptor ERROR:" << GetLastError() << std::endl;
    }
    CreatePipe(&cp_host.h_IN, &cp_client.h_out, &sa, 65536);
    CreatePipe(&cp_host.h_out, &cp_client.h_IN, &sa, 65536);
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.hStdInput = cp_client.h_IN;
    si.hStdError = cp_client.h_out;
    si.hStdOutput = cp_client.h_out;
    PROCESS_INFORMATION pi = { 0 };
    wchar_t cmdLine[MAX_PATH];
    GetModuleFileName(NULL, cmdLine, MAX_PATH);
    wcscat_s(cmdLine, MAX_PATH, L" Plgloader");
    if (!CreateProcessAsUserW(LowSandboxToken,
        nullptr,
        cmdLine,
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr,
        nullptr,
        &si,
        &pi)) {
        return false;
    }
    CloseHandle(cp_client.h_IN);
    CloseHandle(cp_client.h_out);
    auto m_hJob = CreateJobObjectW(nullptr, nullptr);
    if (!m_hJob) {
        LogClient::error() << "创建作业对象失败! 错误代码: " << GetLastError() << std::endl;
        return false;
    }

    // 2. 设置基本限制信息
    JOBOBJECT_BASIC_LIMIT_INFORMATION basicLimit = { 0 };
    basicLimit.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    basicLimit.ActiveProcessLimit = 4;  // 限制活动进程数量

    // 3. 设置扩展限制信息（可选，用于更精细的控制）
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION extendedLimit = { 0 };
    extendedLimit.BasicLimitInformation = basicLimit;

    // 4. 将限制应用到作业对象
    if (!SetInformationJobObject(m_hJob,
        JobObjectExtendedLimitInformation,
        &extendedLimit,
        sizeof(extendedLimit))) {
        LogClient::error() << "设置作业对象限制失败! 错误代码: " << GetLastError() << std::endl;
        CloseHandle(m_hJob);
        m_hJob = nullptr;
        return false;
    }

    std::wcout << L"作业对象创建成功，最多允许 " << basicLimit.ActiveProcessLimit << L" 个子进程" << std::endl;
    if (!AssignProcessToJobObject(m_hJob, pi.hProcess)) {
        LogClient::error() << "分配进程到作业对象失败! 错误代码: " << GetLastError() << std::endl;
        TerminateProcess(pi.hProcess, -254);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return false;
    }
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hThread);
    return true;
}

std::vector<std::string> allowDllListA;
std::vector<std::wstring> allowDllListW;

static HMODULE(WINAPI* RealLoadLibraryW)(LPCWSTR) = LoadLibraryW;
static HMODULE(WINAPI* RealLoadLibraryA)(LPCSTR) = LoadLibraryA;

static HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpLibFileName)
{
    if (!lpLibFileName) {
        return RealLoadLibraryW(lpLibFileName);
    }

    std::wstring dllName = lpLibFileName;
    bool allow = false;

    for (const auto& allowed : allowDllListW) {
        if (_wcsicmp(dllName.c_str(), allowed.c_str()) == 0) {
            allow = true;
            break;
        }
    }

    if (!allow) {
        SetLastError(ERROR_ACCESS_DENIED);  // 0x5
        return nullptr;
    }

    return RealLoadLibraryW(lpLibFileName);
}

static HMODULE WINAPI HookLoadLibraryA(LPCSTR lpLibFileName)
{
    if (!lpLibFileName) {
        return RealLoadLibraryA(lpLibFileName);
    }

    std::string dllName = lpLibFileName;
    bool allow = false;

    for (const auto& allowed : allowDllListA) {
        if (_stricmp(dllName.c_str(), allowed.c_str()) == 0) {
            allow = true;
            break;
        }
    }

    if (!allow) {
        SetLastError(ERROR_ACCESS_DENIED);
        return nullptr;
    }

    return RealLoadLibraryA(lpLibFileName);
}

void PluginMain(std::string path) {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)RealLoadLibraryW, HookLoadLibraryW);
    DetourAttach(&(PVOID&)RealLoadLibraryA, HookLoadLibraryA);
    DetourTransactionCommit();



}

int main(int argc, char* argv[]) {
    SetConsoleCP(65001);
    if (argc > 1) {
        std::string flag(argv[1]);
        if (flag == "loader") {
            PluginMain("");
            return 0;
        }
    }
    ElevateToken();
}