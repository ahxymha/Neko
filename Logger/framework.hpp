#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
#include <string>
#include <sstream>

#define LOG_API __declspec(dllexport) 

LOG_API void SendlogPP(short level, std::string log);

class LogClient {
public:
    enum  LogLevel {
        DEBUG,
        INFO,
        WARNING,
#ifdef ERROR
#undef ERROR
#endif
        ERROR,
        PANIC
    };
private:
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
            SendlogPP(_level, _buffer.str());
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
public:
    static LogWriter log(LogLevel level) {
        return LogWriter(level);
    }
    inline static LogWriter debug() { return log(LogLevel::DEBUG); }
    inline static LogWriter info() { return log(LogLevel::INFO); }
    inline static LogWriter warning() { return log(LogLevel::WARNING); }
    inline static LogWriter error() { return log(LogLevel::ERROR); }
    inline static LogWriter panic() { return log(LogLevel::PANIC); }
};