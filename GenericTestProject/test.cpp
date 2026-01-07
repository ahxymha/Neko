#include <windows.h>
#include <sddl.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "advapi32.lib")

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
        std::wcerr << L"令牌复制失败: " << GetLastError() << std::endl;
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
            std::wcerr << L"设置完整性级别失败 (RID=" << integrityLevelRid
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

// 创建Untrusted令牌的便捷函数
HANDLE CreateUntrustedTokenEx(HANDLE hOriginalToken) {
    return DemoteTokenToIntegrityLevel(hOriginalToken, SECURITY_MANDATORY_UNTRUSTED_RID);
}

// 创建Low令牌的便捷函数
HANDLE CreateLowToken(HANDLE hOriginalToken) {
    return DemoteTokenToIntegrityLevel(hOriginalToken, SECURITY_MANDATORY_LOW_RID);
}

int main() {

    std::wcout.imbue(std::locale("zh_CN.UTF-8"));
    std::wcerr.imbue(std::locale("zh_CN.UTF-8"));
    std::wcin.imbue(std::locale("zh_CN.UTF-8"));

    HANDLE hProcessToken = nullptr;
    HANDLE hUntrustedToken = nullptr;

    // 获取当前进程令牌
    if (!OpenProcessToken(GetCurrentProcess(),
        TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES,
        &hProcessToken)) {
        std::wcerr << L"打开进程令牌失败: " << GetLastError() << std::endl;
        return 1;
    }

    // 显示当前完整性级别
    std::wcout << L"当前进程完整性级别: "
        << GetCurrentIntegrityLevel(hProcessToken) << std::endl;

    // 创建Untrusted令牌
    hUntrustedToken = CreateUntrustedTokenEx(hProcessToken);

    if (hUntrustedToken) {
        std::wcout << L"\n成功创建Untrusted令牌!" << std::endl;
        std::wcout << L"新令牌完整性级别: "
            << GetCurrentIntegrityLevel(hUntrustedToken) << std::endl;

        // 使用Untrusted令牌创建进程（示例）
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = { 0 };

        // 注意：notepad.exe可能无法以Untrusted级别运行
        // 这里使用calc.exe作为示例
        wchar_t cmdLine[] = L"cmd.exe";

        std::wcout << L"\n尝试以Untrusted完整性级别创建进程..." << std::endl;

        if (CreateProcessAsUserW(hUntrustedToken,
            nullptr,
            cmdLine,
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi)) {
            std::wcout << L"进程创建成功! PID: " << pi.dwProcessId << std::endl;

            // 等待进程结束
            WaitForSingleObject(pi.hProcess, INFINITE);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else {
            DWORD error = GetLastError();
            std::wcerr << L"创建进程失败: " << error << std::endl;

            // 如果Untrusted失败，尝试Low级别
            if (error == ERROR_ELEVATION_REQUIRED || error == ERROR_ACCESS_DENIED) {
                std::wcout << L"\n尝试使用Low完整性级别..." << std::endl;

                CloseHandle(hUntrustedToken);
                hUntrustedToken = CreateLowToken(hProcessToken);

                if (hUntrustedToken) {
                    if (CreateProcessAsUserW(hUntrustedToken,
                        nullptr,
                        cmdLine,
                        nullptr,
                        nullptr,
                        FALSE,
                        0,
                        nullptr,
                        nullptr,
                        &si,
                        &pi)) {
                        std::wcout << L"进程以Low完整性级别创建成功!" << std::endl;

                        WaitForSingleObject(pi.hProcess, 2000);

                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                }
            }
        }

        CloseHandle(hUntrustedToken);
    }
    else {
        std::wcerr << L"创建Untrusted令牌失败" << std::endl;
    }

    CloseHandle(hProcessToken);

    return 0;
}
