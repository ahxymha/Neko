// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <random>
#ifndef INIT_ONCE_STATIC_INIT
#define INIT_ONCE_STATIC_INIT {0}
#endif // !INIT_ONCE_STATIC_INIT
#include <sddl.h>
#include <stack>
#include <string.h>
#include <minidumpapiset.h>
#include <Windows.h>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <mutex>
#include <ostream>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "framework.h"
#include "method.h"
#include <vld.h>

#pragma pack(push, 1)
struct LogMode {
    DWORD start_pid;                // 启动程序的PID
    char uuid_pipe[37];             // 命名管道UUID字符串
    char uuid_event[37];            //允许连接事件的uuid字符串
    uint8_t flag;                   // 标志位
    uint64_t checksum;              // 校验和，用于检测损坏
};

class tQueue {
private:
    std::queue<DWORD> data;
    std::mutex mtx;
public:
    void push(DWORD& val) {
        mtx.lock();
        data.push(val);
        mtx.unlock();
    }
    bool empty() {
        mtx.lock();
        bool res = data.empty();
        mtx.unlock();
        return res;
    }
    DWORD get() {
        mtx.lock();
        DWORD res = data.front();
        data.pop();
        mtx.unlock();
        return res;
    }
    DWORD peek() {
        mtx.lock();
        DWORD res = data.front();
        mtx.unlock();
        return res;
    }
    void clear() {
        mtx.lock();
        while (!data.empty()) {
            data.pop();
        }
        mtx.unlock();
        return;
    }
};

struct LogThread {
    std::mutex t_mtx;
    unsigned short index = 0;
    std::thread worker;
    HANDLE h_thread = nullptr;
    HANDLE h_piep = 0;
    bool avalibale = 1;
    LogThread() = default;
    LogThread(LogThread&& other) 
        : t_mtx(),
        index(other.index),
        worker(std::move(other.worker)),
        h_thread(other.h_thread),
        h_piep(other.h_piep),
        avalibale(other.avalibale) {
        if (!other.t_mtx.try_lock()) {
            throw std::runtime_error("Other thread is running");
        }
        other.h_thread = nullptr;
        other.h_piep = nullptr;
        other.avalibale = false;
        other.index = 0;
    }
    LogThread& operator=(LogThread&& other) {
        if (this != &other) {
            if (!other.t_mtx.try_lock()) {
                throw std::runtime_error("Other thread is running");
            }
            index = other.index;
            worker = std::move(other.worker);
            h_thread = other.h_thread;
            h_piep = other.h_piep;
            avalibale = other.avalibale;

            other.h_thread = nullptr;
            other.h_piep = nullptr;
            other.avalibale = false;
            other.index = 0;
        }
        return *this;
    }
    LogThread(const LogThread&) = delete;
    LogThread& operator=(const LogThread&) = delete;
};

#pragma pack(pop)

std::vector<LogThread> v_threadPool;
tQueue q_threadAvaliable;
bool isProcessAvalibale = false;
HANDLE g_stopflag = nullptr;
MessageQueue<std::string> thisProcessLogQueue;
Logger& Log = Logger::instance();
std::stack<HANDLE> h_threads;

// 共享段定义
#pragma data_seg(".shared")
LogMode g_sharedLogMode = {
    0,
    "",      // uuid_namespace
    "",
    0,       // flag
    0        // checksum
};
uint8_t isAvalibale = 0;
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")

// 计算简单校验和
static uint64_t CalculateChecksum(const struct LogMode* data) {
    struct LogMode temp = *data;
    temp.checksum = 0;

    const uint8_t* bytes = (const uint8_t*)&temp;
    size_t size = sizeof(struct LogMode);

    uint32_t sum1 = 0;
    uint32_t sum2 = 0;

    for (size_t i = 0; i < size; i++) {
        sum1 = (sum1 + bytes[i]) & 0xFFFFFFFF;
        sum2 = (sum2 + sum1) & 0xFFFFFFFF;
    }

    // 合并两个32位校验和
    return ((uint64_t)sum2 << 32) | sum1;
}

std::string GenerateUUID() {
    // 使用随机设备作为种子源
    std::random_device rd;
    // 使用Mersenne Twister算法
    std::mt19937 gen(rd());
    // 定义随机数范围：0 到 2^32-1 (32位无符号整数)
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

    // 生成随机数并转换为16进制字符串
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << dis(gen);
    return ss.str();
}

void LogWorker(HANDLE pipe,DWORD index) {
    std::lock_guard<std::mutex> lck(v_threadPool.at(index).t_mtx);
    while(WaitForSingleObject(g_stopflag, 0) == WAIT_TIMEOUT){
        std::unique_ptr<char> buf(new char[65537]);
        DWORD rn = 0;
        if (!ReadFile(pipe, buf.get(), 65536, &rn, NULL)) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                //if (DisconnectNamedPipe(pipe)) {
                //    CloseHandle(pipe);
                //    v_threadPool.at(index).h_piep = nullptr;
                //    std::stringstream s_pipe;
                //    s_pipe << "\\\\.\\pipe\\" << g_sharedLogMode.uuid_pipe;
                //    std::string m_pipe = std::move(s_pipe.str());
                //    SECURITY_ATTRIBUTES la;
                //    la.nLength = sizeof(SECURITY_ATTRIBUTES);
                //    la.bInheritHandle = FALSE;
                //    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;BU)", SDDL_REVISION_1, &la.lpSecurityDescriptor, NULL)) {
                //        Log.error() << "ConvertStringSecurityDescriptorToSecurityDescriptor ERROR:" << GetLastError() << std::endl;
                //        delete[] buf;
                //        return;
                //    }
                //    v_threadPool.at(index).h_piep = CreateNamedPipeA(m_pipe.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 128, 65536, 65536, 0, &la);
                //    q_threadAvaliable.push(index);
                //    delete[] buf;
                //    return;
                //}
                DisconnectNamedPipe(pipe);
                CloseHandle(pipe);
                v_threadPool.at(index).h_piep = nullptr;
                v_threadPool.at(index).avalibale = true;
                v_threadPool.at(index).h_thread = nullptr;
                q_threadAvaliable.push(index);
                return;
            }
            if (WaitForSingleObject(g_stopflag, 0) != WAIT_TIMEOUT) {
                return;
            }
            Log.error() << "ReadFile ERROR:" << GetLastError() << std::endl;
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
            v_threadPool.at(index).h_piep = nullptr;
            v_threadPool.at(index).avalibale = true;
            v_threadPool.at(index).h_thread = nullptr;
            q_threadAvaliable.push(index);
            return;
        }
        if (rn == 0) {
            Log.error() << "ReadFile ERROR:" << "Data Length is 0" << std::endl;
            return;
        }
        buf.get()[rn] = '\0';
        Logger::LogLevel lev;
        switch (buf.get()[0]) {
        case 'D': {
            lev = Logger::LogLevel::DEBUG;
            break;
        }
        case 'I': {
            lev = Logger::LogLevel::INFO;
            break;
        }
        case 'W': {
            lev = Logger::LogLevel::WARNING;
            break;
        }
        case 'E': {
            lev = Logger::LogLevel::ERROR;
            break;
        }
        case 'P': {
            lev = Logger::LogLevel::PANIC;
            break;
        }
        case 'X': {
            //if (DisconnectNamedPipe(pipe)) {
            //    CloseHandle(pipe);
            //    v_threadPool.at(index).h_piep = nullptr;
            //    std::stringstream s_pipe;
            //    s_pipe << "\\\\.\\pipe\\" << g_sharedLogMode.uuid_pipe;
            //    std::string m_pipe = std::move(s_pipe.str());
            //    SECURITY_ATTRIBUTES la;
            //    la.nLength = sizeof(SECURITY_ATTRIBUTES);
            //    la.bInheritHandle = FALSE;
            //    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;BU)", SDDL_REVISION_1, &la.lpSecurityDescriptor, NULL)) {
            //        Log.error() << "ConvertStringSecurityDescriptorToSecurityDescriptor ERROR:" << GetLastError() << std::endl;
            //        return;
            //    }
            //    v_threadPool.at(index).h_piep = CreateNamedPipeA(m_pipe.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, &la);
            //    q_threadAvaliable.push(index);
            //    delete[] buf;
            //    return;
            //}
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
            v_threadPool.at(index).h_piep = nullptr;
            v_threadPool.at(index).avalibale = true;
            v_threadPool.at(index).h_thread = nullptr;
            q_threadAvaliable.push(index);
            return;
        }
        default: {
            Log.error() << "Format ERROR,Print as ERROR" << std::endl;
            lev = Logger::LogLevel::ERROR;
        }
        }
        buf.get()[0] = ' ';
        std::string content(buf.get());
        Log.log(lev) << content << std::endl;
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return;
}

void PipeServer() {
    std::stringstream s_pipe;
    s_pipe << "\\\\.\\pipe\\" << g_sharedLogMode.uuid_pipe;
    std::string m_pipe = std::move(s_pipe.str());
    SECURITY_ATTRIBUTES la;
    la.nLength = sizeof(SECURITY_ATTRIBUTES);
    la.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;BU)", SDDL_REVISION_1, &la.lpSecurityDescriptor, NULL)) {
        Log.error() << "ConvertStringSecurityDescriptorToSecurityDescriptor ERROR:" << GetLastError() << std::endl;
        return;
    }
    SECURITY_ATTRIBUTES ela;
    ela.nLength = sizeof(SECURITY_ATTRIBUTES);
    ela.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;BU)", SDDL_REVISION_1, &ela.lpSecurityDescriptor, NULL)) {
        Log.error() << "ConvertStringSecurityDescriptorToSecurityDescriptor ERROR:" << GetLastError() << std::endl;
        return;
    }
    std::stringstream en;
    en << "Global\\" << g_sharedLogMode.uuid_event;
    auto ableConn = CreateEventA(&ela, FALSE, FALSE, en.str().c_str());
    if (ableConn == NULL) {
        Log.error() << "CreateEvent ERROR:" << GetLastError() << std::endl;
        return;
    }
    v_threadPool.reserve(512);
    for (DWORD i = 0; i < 512; i++) {
        LogThread lt;
        lt.index = i;
        lt.avalibale = true;
        v_threadPool.push_back(std::move(lt));
        q_threadAvaliable.push(i);
    }
    Log.info() << "Thread Pool Size:" << v_threadPool.size() << std::endl;
    std::thread tmpThread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (g_sharedLogMode.flag == 1) {
            g_sharedLogMode.flag = 2;
        }
        else {
            g_sharedLogMode.flag = 3;
            SetEvent(g_stopflag);
        }
        return;
        });
    tmpThread.detach();
    while (WaitForSingleObject(g_stopflag, 0) == WAIT_TIMEOUT){
        BOOL conected = FALSE;
        while (q_threadAvaliable.empty()) {
            isAvalibale = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        isAvalibale = 1;
        DWORD index = q_threadAvaliable.get();
        HANDLE pipe;
        pipe = CreateNamedPipeA(m_pipe.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, &la);
        if (pipe == NULL || pipe == INVALID_HANDLE_VALUE) {
            Log.error() << "Index:" << index << "CreateNamedPipe ERROR:" << GetLastError() << std::endl;
            return;
        }
        v_threadPool.at(index).h_piep = std::move(pipe);
        while (WaitForSingleObject(g_stopflag, 0) == WAIT_TIMEOUT) {
            SetEvent(ableConn);
            conected = ConnectNamedPipe(v_threadPool.at(index).h_piep, NULL);
            isAvalibale = 0;
            if (WaitForSingleObject(g_stopflag, 0) != WAIT_TIMEOUT) {
                CloseHandle(ableConn);
                return;
            }
            if (!conected) {
                if (GetLastError() != ERROR_PIPE_CONNECTED) {
                    Log.error() << "Index:" << index << "ConnectNamedPipe ERROR:" << GetLastError() << std::endl;
                    isAvalibale = 1;
                    continue;
                }
            }
            v_threadPool.at(index).avalibale = false;
            std::thread worker(LogWorker, v_threadPool.at(index).h_piep, index);
            v_threadPool.at(index).worker = std::move(worker);
            v_threadPool.at(index).h_thread = reinterpret_cast<HANDLE>(v_threadPool.at(index).worker.native_handle());
            if (WaitForSingleObject(g_stopflag, 0) != WAIT_TIMEOUT) {
                CloseHandle(ableConn);
                return;
            }
            v_threadPool.at(index).worker.detach();
            break;
        }
    }
}

static BOOL CALLBACK init(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* lpContext) {
    g_sharedLogMode.flag = 1;
	Log.info() << "Logger DLL Injected Successfully." << std::endl;

    g_sharedLogMode.start_pid = GetCurrentProcessId();

    std::string uuid_m = GenerateUUID();
    strncpy_s(g_sharedLogMode.uuid_pipe, sizeof(g_sharedLogMode.uuid_pipe), uuid_m.c_str(), uuid_m.size());

    std::string uuid_e = GenerateUUID();
    strncpy_s(g_sharedLogMode.uuid_event, sizeof(g_sharedLogMode.uuid_event), uuid_e.c_str(), uuid_e.size());

    std::thread mainThread(PipeServer);
    if(mainThread.joinable()){
        h_threads.push(reinterpret_cast<HANDLE>(mainThread.native_handle()));
        mainThread.detach();
    }
    else {
        return FALSE;
    }
    return TRUE;
}

// 生成转储文件（Windows）
static void generateDump() {
    return;
    HANDLE hDumpFile = CreateFileW(
        L"logger_coredump.dmp",
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hDumpFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ExceptionPointers = nullptr;
        dumpInfo.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hDumpFile,
            MiniDumpWithFullMemory,
            &dumpInfo,
            nullptr,
            nullptr
        );

        CloseHandle(hDumpFile);
    }
}

extern"C" LOG_API void __stdcall Sendlog(short level, char* log, int len) {
    while (g_sharedLogMode.flag != 2) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    char flag = '\0';
    switch (level) {
        case 0:{
            flag = 'D';
            break;
        }
        case 1: {
            flag = 'I';
            break;
        }
        case 2: {
            flag = 'W';
            break;
        }
        case 3: {
            flag = 'E';
            break;
        }
        case 4: {
            flag = 'P';
            generateDump();
            break;
        }
    }
    DWORD processId = GetCurrentProcessId();
    std::stringstream logc;

    logc << flag << "<PID:" << std::to_string(processId) << "> " << log;
    thisProcessLogQueue.push(logc.str());
    return;
}

extern"C" LOG_API void __stdcall Stop() {
    DWORD processId = GetCurrentProcessId();
    if (g_sharedLogMode.flag != 2) {
        Log.error() << "Log system is not start up" << std::endl;
    }
    SetEvent(g_stopflag);
    Log.info() << "This system will be closed" << std::endl;
    if (processId != g_sharedLogMode.start_pid) {
        for (int i = 0; i < h_threads.size(); i++) {
            WaitForSingleObject(h_threads.top(), INFINITE);
            h_threads.pop();
        }
        Log.stop();
        return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (auto& thread : v_threadPool) {
        if (!thread.t_mtx.try_lock()) {
            if(thread.h_piep != nullptr){
                CancelIoEx(thread.h_piep, NULL);
                WaitForSingleObject(thread.h_thread, INFINITE);
                CloseHandle(thread.h_piep);
                thread.h_thread = nullptr;
                thread.h_piep = nullptr;
            }
        }
        else {
            thread.t_mtx.unlock();
        }
    }
    std::stringstream pn;
    pn << "\\\\.\\pipe\\" << g_sharedLogMode.uuid_pipe;
    auto tmpp = CreateFileA(pn.str().c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CloseHandle(tmpp);
    for (int i = 0; i < h_threads.size(); i++) {
        WaitForSingleObject(h_threads.top(), INFINITE);
        h_threads.pop();
    }
    Log.stop();
    v_threadPool.clear();
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        if (g_sharedLogMode.flag == 0) {
            static INIT_ONCE initOnce = INIT_ONCE_STATIC_INIT;
            InitOnceExecuteOnce(&initOnce, init, NULL, NULL);
        }
        g_stopflag = CreateEvent(NULL, TRUE, FALSE, NULL);
        std::thread scss([]() {
            while (g_sharedLogMode.flag != 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::stringstream pn;
            pn << "\\\\.\\pipe\\" << g_sharedLogMode.uuid_pipe;
            while (isAvalibale != 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::stringstream en;
            en << "Global\\" << g_sharedLogMode.uuid_event;
            auto es = OpenEventA(SYNCHRONIZE, FALSE, en.str().c_str());
            WaitForSingleObject(es, INFINITE);
            HANDLE pipe = nullptr;
            do {
                pipe = CreateFileA(pn.str().c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            } while (GetLastError() == ERROR_PIPE_BUSY);
            if (pipe == NULL || pipe == INVALID_HANDLE_VALUE) {
                Log.error() << "OpenPipe ERROR:" << GetLastError() << std::endl;
                return;
            }
            isProcessAvalibale = true;
            while (WaitForSingleObject(g_stopflag, 0) == WAIT_TIMEOUT) {
                std::string logc;
                try{
                    thisProcessLogQueue.wait(logc, g_stopflag);
                }
                catch (SystemClose e) {
                    DWORD ct;
                    WriteFile(pipe, "X", sizeof(char) * 2, &ct, NULL);
                    return;
                }
                DWORD ct;
                if (!WriteFile(pipe, logc.c_str(), logc.size() * sizeof(char), &ct, NULL)) {
                    Log.error() << "WritePipe ERROR:" << GetLastError() << ' ' << "PID:" << GetCurrentProcessId() << std::endl;
                    continue;
                }
                if (ct == 0) {
                    Log.error() << "WritePipe ERROR:" << GetLastError() << ' ' << "PID:" << GetCurrentProcessId() << std::endl;
                    continue;
                }
            }
            DWORD ct;
            WriteFile(pipe, "X", sizeof(char) * 2, &ct, NULL);
            });
        h_threads.push(reinterpret_cast<HANDLE>(scss.native_handle()));
        scss.detach();
        break;
    }
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:{
        Stop();
        break;
    }
    }
    return TRUE;
}