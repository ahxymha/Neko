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
#include <exception>

#pragma comment(lib, "dbghelp.lib")

struct SystemClose : public std::exception {
    const char* what() const throw () {
        return "SystemClosing";
    }
};

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

    void wait(T& msg, HANDLE stop) {
        std::unique_lock<std::mutex> lck(_mtx);
        while (_queue.empty()) {
            _cv.wait_for(lck, std::chrono::milliseconds(100));
            if (WaitForSingleObject(stop, 0) != WAIT_TIMEOUT) {
                throw SystemClose();
            }
        }
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
        SetConsoleCP(65001);
        _running = true;
        _logThread = std::thread(&Logger::logWorker, this);
    }

    // 禁用拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

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

            char sig;
            switch (logEntry.level) {
            case LogLevel::DEBUG: sig = '%'; break;
            case LogLevel::INFO: sig = '*'; break;
            case LogLevel::WARNING:sig = '&'; break;
            case LogLevel::ERROR: sig = '-'; break;
            case LogLevel::PANIC: sig = '#'; break;
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
                std::cout << "[" << sig << "] <" << timeStr << "> [" << levelStr << "] " << logEntry.content << std::endl;
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // 恢复默认颜色
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
                if (logEntry.content.empty())continue;
                std::lock_guard<std::mutex> lock(_consoleMutex);
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                std::string levelStr;
                switch (logEntry.level) {
                case LogLevel::DEBUG: levelStr = "DEBUG"; break;
                case LogLevel::INFO: levelStr = "INFO"; break;
                case LogLevel::WARNING: levelStr = "WARNING"; break;
                case LogLevel::ERROR: levelStr = "ERROR"; break;
                case LogLevel::PANIC: levelStr = "PANIC"; break;
                }
                char sig{};
                switch (logEntry.level) {
                case LogLevel::DEBUG: sig = '%'; break;
                case LogLevel::INFO: sig = '*'; break;
                case LogLevel::WARNING:sig = '&'; break;
                case LogLevel::ERROR: sig = '-'; break;
                case LogLevel::PANIC: sig = '#'; break;
                }
                WORD color;
                switch (logEntry.level) {
                case LogLevel::DEBUG: color = FOREGROUND_BLUE | FOREGROUND_GREEN; break;  // 青色
                case LogLevel::INFO: color = FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;  // 亮绿色
                case LogLevel::WARNING: color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; break;  // 黄色
                case LogLevel::ERROR: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;  // 亮红色
                case LogLevel::PANIC: color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY; break;  // 亮紫色
                default: color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // 白色
                }
                auto now = std::chrono::system_clock::now();
                auto now_c = std::chrono::system_clock::to_time_t(now);
                char timestr[64];
                ctime_s(timestr, sizeof(timestr), &now_c);
                SetConsoleTextAttribute(hConsole, color);
                std::cout << "[" << sig << "] <" << timestr << "> [" << levelStr << "] " << logEntry.content << std::endl;
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // 恢复默认颜色
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