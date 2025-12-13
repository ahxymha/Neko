#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <vector>
#include <Windows.h>
#include <dbghelp.h>
#include <chrono>
#include <iomanip>
#include <memory>
#include <functional>

#pragma comment(lib, "dbghelp.lib")

// 消息队列模板类
template <typename T>
class MessageQueue {
public:
    void push(const T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        _queue.push(msg);
        _cv.notify_one();
    }

    bool poll(T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        if (!_queue.empty()) {
            msg = _queue.front();
            _queue.pop();
            return true;
        }
        return false;
    }

    void wait(T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        while (_queue.empty()) _cv.wait(lck);
        msg = _queue.front();
        _queue.pop();
    }

    size_t size() {
        std::unique_lock<std::mutex> lck(_mtx);
        return _queue.size();
    }

private:
    std::queue<T> _queue;
    std::mutex _mtx;
    std::condition_variable _cv;
};

class Logger {
public:
    // 日志级别枚举
    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
#ifdef ERROR
#undef ERROR
#endif
        ERROR,
        PANIC
    };

    // 日志结构体
    struct Log {
        std::string content;
        Logger::LogLevel level;
        time_t time;
    };

private:
    // 日志写入器类
    class LogWriter {
    private:
        std::stringstream _buffer;
        LogLevel _level;
        
        LogWriter(const LogWriter&) = delete;
        LogWriter& operator=(const LogWriter&) = delete;
    public:
        LogWriter(LogLevel level) {
            _level = level;
        }

        LogWriter(LogWriter&& other) noexcept : _level(other._level) {
            _buffer << other._buffer.rdbuf();
            _level = other._level;
        }

        LogWriter& operator=(LogWriter&& other) noexcept {
            _level = other._level;
            _buffer << other._buffer.rdbuf();
            return *this;
        }

        ~LogWriter() {
            if (Logger::getInstance()._running) {
                Logger::getInstance().writeLog(_buffer.str(), _level);
            }
        }

        template<typename T>
        LogWriter& operator<<(const T& value) {
            _buffer << value;
            return *this;
        }

        // 处理 std::endl 等特殊操作符
        LogWriter& operator<<(std::ostream& (*manip)(std::ostream&)) {
            _buffer << manip;
            return *this;
        }
    };

    MessageQueue<Log> _logQueue;
    std::thread _logThread;
    std::atomic<bool> _running;
    std::mutex _consoleMutex;

    // 单例实例
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // 私有构造函数
    Logger() : _running(false) {
        _running = true;
        _logThread = std::thread(&Logger::logWorker, this);
    }

    // 禁用拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 生成转储文件（Windows）
    static void generateDump() {
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

    // 日志处理线程函数
    void logWorker() {
        while (_running) {
            Log logEntry;
            _logQueue.wait(logEntry);

            if (!_running) break;

            // 格式化时间
            struct tm timeinfo;
            localtime_s(&timeinfo, &logEntry.time);
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

            // 获取级别字符串
            std::string levelStr;
            switch (logEntry.level) {
            case LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LogLevel::INFO: levelStr = "INFO"; break;
            case LogLevel::WARNING: levelStr = "WARNING"; break;
            case LogLevel::ERROR: levelStr = "ERROR"; break;
            case LogLevel::PANIC: levelStr = "PANIC"; break;
            }

            // 输出到控制台
            {
                std::lock_guard<std::mutex> lock(_consoleMutex);
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

                // 根据日志级别设置颜色
                WORD color;
                switch (logEntry.level) {
                case LogLevel::DEBUG: color = FOREGROUND_BLUE | FOREGROUND_GREEN; break;  // 青色
                case LogLevel::INFO: color = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;  // 亮绿色
                case LogLevel::WARNING: color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;  // 黄色
                case LogLevel::ERROR: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;  // 亮红色
                case LogLevel::PANIC: color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;  // 亮紫色
                default: color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // 白色
                }

                SetConsoleTextAttribute(hConsole, color);
                std::cout << timeStr << " [" << levelStr << "] " << logEntry.content << std::endl;
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // 恢复默认颜色
            }

            // 如果是 PANIC 级别，生成转储文件并终止程序
            if (logEntry.level == LogLevel::PANIC) {
                std::cerr << "PANIC level log detected. Generating core dump..." << std::endl;
                generateDump();
                TerminateProcess(GetCurrentProcess(), 1);
            }
        }
    }

    // 写入日志到队列
    void writeLog(const std::string& content, LogLevel level) {
        Log logEntry;
        logEntry.content = content;
        logEntry.level = level;
        time(&logEntry.time);
        _logQueue.push(logEntry);
    }

public:
    ~Logger() {
        stop();
    }

    // 停止日志系统
    void stop() {
        if (_running) {
            _running = false;
            // 推送一个空日志来唤醒等待的线程
            Log dummyLog;
            _logQueue.push(dummyLog);
            if (_logThread.joinable()) {
                _logThread.join();
            }

            // 输出剩余的日志
            Log logEntry;
            while (_logQueue.poll(logEntry)) {
                struct tm timeinfo;
                localtime_s(&timeinfo, &logEntry.time);
                char timeStr[64];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

                std::string levelStr;
                switch (logEntry.level) {
                case LogLevel::DEBUG: levelStr = "DEBUG"; break;
                case LogLevel::INFO: levelStr = "INFO"; break;
                case LogLevel::WARNING: levelStr = "WARNING"; break;
                case LogLevel::ERROR: levelStr = "ERROR"; break;
                case LogLevel::PANIC: levelStr = "PANIC"; break;
                }

                std::cout << timeStr << " [" << levelStr << "] " << logEntry.content << std::endl;
            }
        }
    }

    // 获取单例实例的静态方法
    static Logger& instance() {
        return getInstance();
    }

    // 静态便捷方法，用于获取日志写入器
    static LogWriter log(LogLevel level) {
        return LogWriter(level);
    }

    // 静态便捷方法
    static LogWriter debug() { return log(LogLevel::DEBUG); }
    static LogWriter info() { return log(LogLevel::INFO); }
    static LogWriter warning() { return log(LogLevel::WARNING); }
    static LogWriter error() { return log(LogLevel::ERROR); }
    static LogWriter panic() { return log(LogLevel::PANIC); }

    // 获取队列大小（用于测试）
    size_t queueSize() {
        return _logQueue.size();
    }
};