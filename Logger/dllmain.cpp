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
    char uuid_boundary[37];         // 边界描述符UUID字符串
    char uuid_namespace[37];        // 命名空间UUID字符串
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
    "",      // uuid_boundary
    "",      // uuid_namespace
    "",      // uuid_pipe
    0,       // flag
    0        // checksum
};
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")

std::string GenerateUUID() {
    // 获取当前时间戳（毫秒）
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    // 版本 7：时间戳 + 随机数
    uint64_t ts_high = (timestamp >> 16) & 0xFFFFFFFFFFFF;
    uint64_t ts_low = timestamp & 0xFFFF;

    // 生成随机部分
    std::mt19937_64 gen;
    std::uniform_int_distribution<uint64_t> dis;
    uint64_t rand_a = dis(gen) & 0xFFF;  // 12 bits
    uint64_t rand_b = dis(gen) & 0x3FFFFFFFFFFFFFFF; // 62 bits

    // 组合 UUID（版本 7：0x70, 变体：0x80）
    uint64_t msb = (ts_high << 16) | (ts_low << 4) | 0x7;
    uint64_t lsb = (rand_b & 0x3FFFFFFFFFFFFFFF) | 0x8000000000000000;

    // 格式化为字符串
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
        << std::setw(8) << ((msb >> 32) & 0xFFFFFFFF) << "-"
        << std::setw(4) << ((msb >> 16) & 0xFFFF) << "-"
        << std::setw(4) << (msb & 0xFFFF) << "-"
        << std::setw(4) << ((lsb >> 48) & 0xFFFF) << "-"
        << std::setw(12) << (lsb & 0xFFFFFFFFFFFF);

    return ss.str();
}

class tQueue{
private:
    std::queue<unsigned short> data;
    std::mutex mtx;
public:
    void push(unsigned short &val){
        
    }
};

std::queue<unsigned short> q_threadAvaliable;

struct LogThread {
    unsigned short index;
    std::thread worker;
    HANDLE h_piep;
    bool avalibale;
};

void LogWorker(std::wstring content) {

}

void PipeServer(HANDLE h_pipe, HANDLE h_stop, std::string m_pipe) {
    SECURITY_ATTRIBUTES la;
    la.nLength = sizeof(SECURITY_ATTRIBUTES);
    la.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;BU)", SDDL_REVISION_1, &la.lpSecurityDescriptor, NULL)) {
        std::cerr << "ConvertStringSecurityDescriptorToSecurityDescriptor ERROR:" << GetLastError() << std::endl;
        return;
    }
    std::vector<LogThread> v_threadPool;
    v_threadPool.reserve(64);
    for (int i = 0; i < 64; i++) {
        HANDLE pipe;
        pipe = CreateNamedPipeA(m_pipe.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 64, 65536, 65536, 0, &la);
        if (pipe==NULL&&pipe==INVALID_HANDLE_VALUE) {
            std::cerr << "Index:" << i << "CreateNamedPipe ERROR:" << GetLastError() << std::endl;
            return;
        }
        LogThread lt;
        lt.index = i;
        lt.avalibale = true;
        lt.h_piep = std::move(pipe);
        v_threadPool.push_back(std::move(lt));
        q_threadAvaliable.push(i);
    }
    std::cout << "Thread Pool Size:" << v_threadPool.size() << std::endl;
    while (WaitForSingleObject(h_stop, 0) == WAIT_TIMEOUT){
        BOOL conected = FALSE;
        unsigned int index
        
    }
}

static BOOL CALLBACK init(PINIT_ONCE InitOnce, PVOID Parameter, PVOID* lpContext) {
    g_sharedLogMode.flag = 1;
    std::string uuid_b = GenerateUUID();
    strncpy_s(g_sharedLogMode.uuid_boundary, sizeof(g_sharedLogMode.uuid_boundary), uuid_b.c_str(), uuid_b.size());
    g_sharedLogMode.checksum = CalculateChecksum(&g_sharedLogMode);
    HANDLE h_boundary = CreateBoundaryDescriptorA(uuid_b.c_str(), 0);
    if (h_boundary == NULL) {
        std::cerr << "CreateBoundaryDescriptor ERROR:" << GetLastError() << std::endl;
        return FALSE;
    }
    std::vector<std::string> ssids = { "S-1-5-18","S-1-5-32-544","S-1-5-32-545" };
    for (auto& ssid : ssids) {
        PSID sid;
        if (!ConvertStringSidToSidA(ssid.c_str(), &sid)) {
            std::cerr <<"ConvertingSID:" << ssid << "ConvertStringSidToSid ERROR:" << GetLastError() << std::endl;
            return FALSE;
        }
        if (!AddSIDToBoundaryDescriptor(&h_boundary, sid)) {
            std::cerr << "AddingSID:" << ssid << "AddSIDToBoundaryDescriptor ERROR:" << GetLastError() << std::endl;
            return FALSE;
        }
    }
    std::string uuid_n = GenerateUUID();
    strncpy_s(g_sharedLogMode.uuid_namespace, sizeof(g_sharedLogMode.uuid_namespace), uuid_n.c_str(), uuid_n.size());
    if (!CreatePrivateNamespaceA(NULL, h_boundary, uuid_n.c_str())) {
        std::cerr << "CreatePrivateNamespace ERROR:" << GetLastError() << std::endl;
        return FALSE;
    }
    HANDLE h_namespace;
    h_namespace = OpenPrivateNamespaceA(h_boundary, uuid_n.c_str());
    if (h_namespace == NULL) {
        std::cerr << "OpenPrivateNamespace ERROR:" << GetLastError() << std::endl;
        return FALSE;
    }
    std::string uuid_m = GenerateUUID();
    strncpy_s(g_sharedLogMode.uuid_pipe, sizeof(g_sharedLogMode.uuid_pipe), uuid_m.c_str(), uuid_m.size());

    return TRUE;
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
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}