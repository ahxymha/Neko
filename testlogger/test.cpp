// LoggerTest.cpp
#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <chrono>

// 定义日志级别枚举
enum LogLevel {
    DEBUG_LEVEL = 0,
    INFO_LEVEL = 1,
    WARNING_LEVEL = 2,
    ERROR_LEVEL = 3,
    PANIC_LEVEL = 4
};

// 定义函数指针类型
typedef void(__stdcall* SendlogFunc)(short level, char* log, int len);

// 全局函数指针
SendlogFunc Sendlog = nullptr;
HMODULE hLoggerDll = nullptr;

// 初始化DLL和函数
bool InitLogger() {
    // 加载DLL
    hLoggerDll = LoadLibrary(TEXT("NekoLogger.dll"));
    if (!hLoggerDll) {
        std::cerr << "加载NekoLogger.dll失败: " << GetLastError() << std::endl;
        return false;
    }
    
    // 获取函数地址 - 注意函数名是"log"，不是"Sendlog"
    Sendlog = (SendlogFunc)GetProcAddress(hLoggerDll, "Sendlog");
    if (!Sendlog) {
        std::cerr << "获取log函数失败: " << GetLastError() << std::endl;
        FreeLibrary(hLoggerDll);
        hLoggerDll = nullptr;
        return false;
    }
    
    std::cout << "Logger DLL加载成功!" << std::endl;
    return true;
}

// 清理资源
void CleanupLogger() {
    if (hLoggerDll) {
        // 注意：日志系统可能需要一些时间来刷新缓冲区
        // 等待一段时间确保所有日志都被处理
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        FreeLibrary(hLoggerDll);
        hLoggerDll = nullptr;
        Sendlog = nullptr;
        std::cout << "Logger DLL已卸载!" << std::endl;
    }
}

// 辅助函数：生成随机字符串
std::string GenerateRandomString(int length) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (int i = 0; i < length; ++i) {
        result += alphanum[dis(gen)];
    }
    
    return result;
}

// 日志写入线程函数
void LogThreadFunc(int threadId, int numLogs) {
    std::cout << "线程 " << threadId << " 开始写入日志..." << std::endl;
    
    for (int i = 0; i < numLogs; i++) {
        // 随机选择日志级别
        int level = rand() % 5;
        
        // 生成日志内容
        std::string logMessage = "[线程 " + std::to_string(threadId) + 
                                "] 日志 #" + std::to_string(i) + 
                                ": " + GenerateRandomString(20);
        
        // 调用Sendlog函数
        if (Sendlog) {
            Sendlog(static_cast<short>(level), 
                   const_cast<char*>(logMessage.c_str()), 
                   static_cast<int>(logMessage.length()));
        } else {
            std::cerr << "Sendlog函数未初始化!" << std::endl;
        }
        
        // 随机延迟，模拟真实场景
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
    }
    
    std::cout << "线程 " << threadId << " 完成" << std::endl;
}

// 简单测试函数
void SimpleTest() {
    std::cout << "\n=== 简单测试开始 ===" << std::endl;
    
    if (!Sendlog) {
        std::cerr << "Sendlog函数未初始化!" << std::endl;
        return;
    }
    
    // 测试不同级别的日志
    std::string testMessages[] = {
        "这是一个DEBUG级别的测试日志",
        "这是一个INFO级别的测试日志",
        "这是一个WARNING级别的测试日志",
        "这是一个ERROR级别的测试日志",
        "这是一个PANIC级别的测试日志"
    };
    
    for (int i = 0; i < 5; i++) {
        std::cout << "发送 " << i << " 级日志: " << testMessages[i] << std::endl;
        
        Sendlog(static_cast<short>(i), 
               const_cast<char*>(testMessages[i].c_str()), 
               static_cast<int>(testMessages[i].length()));
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "=== 简单测试完成 ===" << std::endl;
}

// 多线程压力测试
void StressTest(int numThreads, int logsPerThread) {
    std::cout << "\n=== 多线程压力测试开始 ===" << std::endl;
    std::cout << "线程数: " << numThreads << std::endl;
    std::cout << "每个线程日志数: " << logsPerThread << std::endl;
    std::cout << "总日志数: " << numThreads * logsPerThread << std::endl;
    
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 创建并启动所有线程
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(LogThreadFunc, i + 1, logsPerThread);
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "=== 压力测试完成 ===" << std::endl;
    std::cout << "总耗时: " << duration.count() << " 毫秒" << std::endl;
    if (duration.count() > 0) {
        std::cout << "平均每秒日志数: " 
                  << (numThreads * logsPerThread * 1000.0 / duration.count()) 
                  << std::endl;
    }
}

// 多进程测试函数
void MultiProcessTest() {
    std::cout << "\n=== 多进程测试开始 ===" << std::endl;
    
    // 创建多个进程
    const int numProcesses = 3;
    STARTUPINFOA si[numProcesses];
    PROCESS_INFORMATION pi[numProcesses];
    
    // 获取当前可执行文件路径
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    // 为每个进程创建命令行参数
    for (int i = 0; i < numProcesses; i++) {
        ZeroMemory(&si[i], sizeof(STARTUPINFOA));
        si[i].cb = sizeof(STARTUPINFOA);
        ZeroMemory(&pi[i], sizeof(PROCESS_INFORMATION));
        
        // 构建命令行：传递进程ID作为参数
        std::string cmdLine = std::string("\"") + exePath + "\" child " + std::to_string(i + 1);
        
        std::cout << "创建进程 " << i << ": " << cmdLine << std::endl;
        
        // 创建进程
        if (!CreateProcessA(
            NULL,                   // 可执行文件路径已经在命令行中指定
            const_cast<char*>(cmdLine.c_str()), // 命令行
            NULL,                   // 进程句柄不可继承
            NULL,                   // 线程句柄不可继承
            FALSE,                  // 不继承句柄
            CREATE_NEW_CONSOLE,     // 为新进程创建新控制台
            NULL,                   // 使用父进程环境
            NULL,                   // 使用父进程当前目录
            &si[i],                 // STARTUPINFO
            &pi[i]                  // PROCESS_INFORMATION
        )) {
            std::cerr << "创建进程 " << i << " 失败: " << GetLastError() << std::endl;
        } else {
            std::cout << "创建进程 " << i << " 成功，PID: " << pi[i].dwProcessId << std::endl;
        }
    }
    
    // 等待所有进程完成
    for (int i = 0; i < numProcesses; i++) {
        if (pi[i].hProcess) {
            WaitForSingleObject(pi[i].hProcess, 5000); // 等待5秒
            DWORD exitCode;
            GetExitCodeProcess(pi[i].hProcess, &exitCode);
            std::cout << "进程 " << i << " 退出，代码: " << exitCode << std::endl;
            
            CloseHandle(pi[i].hProcess);
            CloseHandle(pi[i].hThread);
        }
    }
    
    std::cout << "=== 多进程测试完成 ===" << std::endl;
}

// 子进程函数
void ChildProcessFunction(int processId) {
    std::cout << "子进程 " << processId << " 启动" << std::endl;
    
    // 子进程也需要初始化DLL
    if (!InitLogger()) {
        std::cerr << "子进程初始化Logger失败!" << std::endl;
        return;
    }
    
    std::string processName = "进程-" + std::to_string(processId);
    
    for (int i = 0; i < 10; i++) {
        std::string logMessage = "[" + processName + "] 日志 #" + std::to_string(i);
        
        // 使用不同的日志级别
        int level = (i + processId) % 5;
        
        if (Sendlog) {
            Sendlog(static_cast<short>(level), 
                   const_cast<char*>(logMessage.c_str()), 
                   static_cast<int>(logMessage.length()));
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // 子进程结束后清理
    CleanupLogger();
    
    std::cout << "子进程 " << processId << " 完成" << std::endl;
}

int main(int argc, char* argv[]) {
    // 初始化随机种子
    srand(static_cast<unsigned int>(time(nullptr)));
    
    std::cout << "NekoLogger 多进程日志系统测试程序" << std::endl;
    std::cout << "=================================" << std::endl;
    
    // 检查是否是子进程模式
    if (argc > 1 && std::string(argv[1]) == "child") {
        int processId = (argc > 2) ? std::stoi(argv[2]) : 1;
        ChildProcessFunction(processId);
        
        std::cout << "按Enter键退出子进程..." << std::endl;
        std::cin.get();
        
        return 0;
    }
    
    // 主测试程序
    try {
        // 初始化Logger DLL
        if (!InitLogger()) {
            std::cerr << "初始化Logger失败，程序退出!" << std::endl;
            return 1;
        }
        
        // 测试1: 简单测试
        SimpleTest();
        
        // 等待一段时间，让日志线程处理完
        std::cout << "\n等待2秒，让日志系统处理..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // // 测试2: 多线程压力测试
        // StressTest(3, 10);  // 3个线程，每个线程10条日志
        
        // // 等待一段时间
        // std::cout << "\n等待2秒，让日志系统处理..." << std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // // 测试3: 多进程测试
        // MultiProcessTest();
        
        // // 最终等待，确保所有日志都被处理
        // std::cout << "\n等待5秒，确保所有日志都被处理..." << std::endl;
        // std::this_thread::sleep_for(std::chrono::seconds(5));
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        CleanupLogger();
        return 1;
    }
    
    // 清理资源
    CleanupLogger();
    
    std::cout << "\n所有测试完成！" << std::endl;
    std::cout << "按Enter键退出..." << std::endl;
    std::cin.get();
    
    return 0;
}