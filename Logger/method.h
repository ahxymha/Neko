#pragma once

#include <Windows.h>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <thread>

template <typename T>
class MessageQueue;

struct Log;

class Logger {
private:
    MessageQueue<Log> logs;
    HANDLE stopEvent;
    std::mutex _mtx;
    std::thread *_logd;
    void Logd(MessageQueue<Log> logq, HANDLE stopEvent);
public:
    enum class LogLevel {
        DBBUG,
        INFO,
        WARNING,
#ifdef ERROR
#undef ERROR
#endif
        ERROR
    };
    Logger() {
        stopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        _logd = new std::thread(Logd, logs, stopEvent);
    }
};

struct Log {
    std::string content;
    Logger::LogLevel level;
    time_t time;
};

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

private:
    std::queue<T> _queue;
    std::mutex _mtx;
    std::condition_variable _cv;
};