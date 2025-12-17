// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <random>
#include <atomic>
#include <codecvt>
#ifndef INIT_ONCE_STATIC_INIT
#define INIT_ONCE_STATIC_INIT {0}
#endif // !INIT_ONCE_STATIC_INIT
#include <sddl.h>


#pragma pack(push, 1)
struct LogMode {
    DWORD start_pid;                // 启动程序的PID
    char uuid_pipe[37];             // 命名管道UUID字符串
    uint8_t flag;                   // 标志位
    uint64_t checksum;              // 校验和，用于检测损坏
};
#pragma pack(pop)

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

// 共享段定义
#pragma data_seg(".shared")
LogMode g_sharedLogMode = {
    0,
    "",      // uuid_namespace
    0,       // flag
    0        // checksum
};
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")

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

class tQueue{
private:
    std::queue<unsigned short> data;
    std::mutex mtx;
public:
    void push(unsigned short &val){
        mtx.lock();
        data.push(val);
        mtx.unlock();
    }
    unsigned short get() {
        mtx.lock();
        unsigned short res = data.front();
        data.pop();
        mtx.unlock();
        return res;
    }
    unsigned short peek() {
        mtx.lock();
        unsigned short res = data.front();
        mtx.unlock();
        return res;
    }
};

tQueue q_threadAvaliable;
HANDLE h_stop;
Logger& Log = Logger::instance();

struct LogThread {
    unsigned short index=0;
    std::thread worker;
    HANDLE h_piep=0;
    bool avalibale=1;
};

void LogWorker(HANDLE pipe) {
    char* buf = new char[65537];
    DWORD rn = 0;
    if (!ReadFile(pipe, buf, 65536, &rn, NULL)) {
        Log.error()<< "ReadFile ERROR:" << GetLastError() << std::endl;
        return;
    }
    if (rn == 0) {
        Log.error() << "ReadFile ERROR:" << "Data Length is 0" << std::endl;
        return;
    }
    buf[rn] = '\0';
    Logger::LogLevel lev;
    switch (buf[0]) {
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
        default: {
            Log.error() << "Format ERROR,Print as ERROR" << std::endl;
            lev = Logger::LogLevel::ERROR;
        }
    }
    buf[0] = ' ';
    std::string content(buf);
    Log.log(lev) << content << std::endl;
    delete[] buf;
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
    std::vector<LogThread> v_threadPool;
    v_threadPool.reserve(64);
    for (unsigned short i = 0; i < 128; i++) {
        HANDLE pipe;
        pipe = CreateNamedPipeA(m_pipe.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 128, 65536, 65536, 0, &la);
        if (pipe==NULL || pipe==INVALID_HANDLE_VALUE) {
            Log.error() << "Index:" << i << "CreateNamedPipe ERROR:" << GetLastError() << std::endl;
            return;
        }
        LogThread lt;
        lt.index = i;
        lt.avalibale = true;
        lt.h_piep = std::move(pipe);
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
            SetEvent(h_stop);
        }
        return;
        });
    tmpThread.detach();
    while (WaitForSingleObject(h_stop, 0) == WAIT_TIMEOUT){
        BOOL conected = FALSE;
        unsigned int index = q_threadAvaliable.get();
        while (WaitForSingleObject(h_stop, 0) == WAIT_TIMEOUT) {
            conected = ConnectNamedPipe(v_threadPool.at(index).h_piep, NULL);
            if (!conected) {
                if (GetLastError() != ERROR_PIPE_CONNECTED) {
                    Log.error() << "Index:" << index << "ConnectNamedPipe ERROR:" << GetLastError() << std::endl;
                    continue;
                }
            }
            v_threadPool.at(index).avalibale = false;
            std::thread worker(LogWorker, v_threadPool.at(index).h_piep);
            v_threadPool.at(index).worker = std::move(worker);
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

    h_stop = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (h_stop == NULL) {
        return FALSE;
    }

    std::thread mainThread(PipeServer);
    if(mainThread.joinable()){
        mainThread.detach();
    }
    else {
        return FALSE;
    }
    return TRUE;
}

extern"C" LOG_API void __stdcall Sendlog(short level, char* log, int len) {
    while (g_sharedLogMode.flag != 2) {
        Sleep(100);
    }
    std::stringstream pn;
    pn << "\\\\.\\pipe\\" << g_sharedLogMode.uuid_pipe;
    HANDLE pipe = CreateFileA(pn.str().c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (pipe == NULL) {
        Log.error() << "OpenPipe ERROR:" << GetLastError() << std::endl;
        return;
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
            break;
        }
    }
    DWORD processId = GetCurrentProcessId();
    std::stringstream logc;

    logc << flag << "<PID:" << std::to_string(processId) << "> " << log;
    DWORD ct;
    if (!WriteFile(pipe, logc.str().c_str(), logc.str().size() * sizeof(char), &ct, NULL)) {
        Log.error() << "WritePipe ERROR:" << GetLastError() << std::endl;
        return;
    }
    if (ct == 0) {
        Log.error() << "WritePipe ERROR:" << GetLastError() << std::endl;
        return;
    }
    return;
}

extern"C" LOG_API void __stdcall Stop() {
    DWORD processId = GetCurrentProcessId();
    if (g_sharedLogMode.flag != 2) {
        Log.error() << "Log system is not start up" << std::endl;
    }
    if (processId != g_sharedLogMode.start_pid) {
        Log.error() << "Please use first startup process to stop this log system" << std::endl;
        std::stringstream pn;
        pn << "\\\\.\\pipe\\" << g_sharedLogMode.uuid_pipe;
        HANDLE pipe = CreateFileA(pn.str().c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (pipe == NULL) {
            Log.error() << "OpenPipe ERROR:" << GetLastError() << std::endl;
            return;
        }
        std::stringstream logc;

        logc << "E" << "<SYSTEM> " << "Some process want to stop this system. PID:" << processId;
        DWORD ct;
        if (!WriteFile(pipe, logc.str().c_str(), logc.str().size() * sizeof(char), &ct, NULL)) {
            Log.error() << "WritePipe ERROR:" << GetLastError() << std::endl;
            return;
        }
        if (ct == 0) {
            Log.error() << "WritePipe ERROR:" << GetLastError() << std::endl;
            return;
        }
        return;
    }
    SetEvent(h_stop);
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
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:{
        break;
    }
    }
    return TRUE;
}