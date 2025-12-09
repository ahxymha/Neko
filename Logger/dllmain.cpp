// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <random>
#include <atomic>

#pragma pack(push, 1)
struct LogMode {
    char uuid[37];      // UUID字符串
    uint8_t flag;       // 标志位
    uint32_t checksum;  // 校验和，用于检测损坏
};
#pragma pack(pop)

// 计算简单校验和
static uint32_t CalculateChecksum(const LogMode* mode) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(mode);
    uint32_t sum = 0;

    for (size_t i = 0; i < sizeof(LogMode) - sizeof(uint32_t); ++i) {
        sum += data[i];
    }

    return sum;
}

// 共享段定义
#pragma data_seg(".shared")
LogMode g_sharedLogMode = {
    "",      // uuid
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

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        if (g_sharedLogMode.flag == 0) {
            g_sharedLogMode.flag = 1;
            std::string uuid = GenerateUUID();
            strncpy_s(g_sharedLogMode.uuid, sizeof(g_sharedLogMode.uuid), uuid.c_str(), uuid.size());
            g_sharedLogMode.checksum = CalculateChecksum(&g_sharedLogMode);
        }
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}