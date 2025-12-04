// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
HANDLE HexStringToHandle(const std::string& hexStr) {
    // 验证输入只包含有效的十六进制字符
    for (char c : hexStr) {
        if (!((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'))) {
            std::cerr << "Invalid hex character in handle string: " << c << std::endl;
            return INVALID_HANDLE_VALUE;
        }
    }

    std::stringstream ss;
    uintptr_t handleValue;

    // 将十六进制字符串转换为整数
    ss << std::hex << hexStr;
    ss >> handleValue;

    // 检查转换是否成功
    if (ss.fail()) {
        std::cerr << "Failed to convert hex string to handle" << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    return reinterpret_cast<HANDLE>(handleValue);
}
HANDLE GetHandleFromEnvironment(const char* varName) {
    char buffer[256];
    DWORD length = GetEnvironmentVariableA(varName, buffer, sizeof(buffer));

    if (length == 0) {
        std::cerr << "Environment variable " << varName << " not found or empty" << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    if (length >= sizeof(buffer)) {
        std::cerr << "Environment variable " << varName << " too long" << std::endl;
        return INVALID_HANDLE_VALUE;
    }

    std::string hexStr(buffer);
    std::cout << "Got " << varName << " = " << hexStr << std::endl;

    return HexStringToHandle(hexStr);
}
bool CheckHandleValidity(HANDLE h, const char* name) {
    if (h == INVALID_HANDLE_VALUE || h == NULL) {
        std::cerr << name << " handle is invalid or NULL" << std::endl;
        return false;
    }

    DWORD flags;
    if (GetHandleInformation(h, &flags)) {
        std::cout << name << " handle is valid. Flags: 0x" << std::hex << flags << std::dec << std::endl;
        return true;
    }
    else {
        DWORD error = GetLastError();
        std::cerr << name << " handle is invalid. Error: " << error << std::endl;
        return false;
    }
}

extern"C" WEBHELPER_API bool __stdcall GetHtmlContent(char* res, const unsigned int len) {
    HANDLE htmlPipe = GetHandleFromEnvironment("ELECTRON_HTML_PIPE");
    if (!CheckHandleValidity(htmlPipe, "HTML pipe")) {
        return false;
    }

    std::cout << "Reading HTML content from pipe..." << std::endl;

    // 使用PeekNamedPipe先检查是否有数据
    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(htmlPipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
        DWORD error = GetLastError();
        std::cerr << "PeekNamedPipe failed. Error: " << error << std::endl;
        return false;
    }

    std::cout << "Bytes available in pipe: " << bytesAvailable << std::endl;

    if (bytesAvailable == 0) {
        std::cout << "No data available in pipe yet. Waiting 2 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 再次检查
        if (!PeekNamedPipe(htmlPipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
            DWORD error = GetLastError();
            std::cerr << "Second PeekNamedPipe failed. Error: " << error << std::endl;
            return false;
        }

        std::cout << "Bytes available after wait: " << bytesAvailable << std::endl;
    }

    if (bytesAvailable == 0) {
        std::cerr << "Still no data in pipe after waiting. Something is wrong." << std::endl;
        return false;
    }

    // 读取HTML内容
    std::string htmlContent;
    char buffer[4096]{};
    DWORD bytesRead;
    DWORD totalBytesRead = 0;

    // 确保不会读取超过缓冲区大小的内容
    const size_t maxReadSize = len - 1; // 为null终止符留出空间

    while (totalBytesRead < maxReadSize) {
        DWORD bytesToRead = sizeof(buffer) - 1;
        if (bytesToRead > maxReadSize - totalBytesRead) {
            bytesToRead = static_cast<DWORD>(maxReadSize - totalBytesRead);
        }

        if (!ReadFile(htmlPipe, buffer, bytesToRead, &bytesRead, NULL)) {
            DWORD error = GetLastError();

            if (error == ERROR_BROKEN_PIPE) {
                std::cout << "HTML pipe closed by parent (normal)" << std::endl;
                break;
            }
            else if (error == ERROR_NO_DATA) {
                std::cout << "No more data in pipe" << std::endl;
                break;
            }
            else {
                std::cerr << "ReadFile failed. Error: " << error << std::endl;
                break;
            }
        }

        if (bytesRead == 0) {
            std::cout << "End of HTML content (0 bytes read)" << std::endl;
            break;
        }

        totalBytesRead += bytesRead;
        buffer[bytesRead] = '\0';
        htmlContent.append(buffer, bytesRead);

        std::cout << "Read " << bytesRead << " bytes. Total: " << totalBytesRead
            << " (max: " << maxReadSize << ")" << std::endl;

        // 如果读取的字节数少于请求的字节数，可能已经读取完所有数据
        if (bytesRead < bytesToRead) {
            std::cout << "Read less than requested, assuming end of data" << std::endl;
            break;
        }
    }

    if (!htmlContent.empty()) {
        std::cout << "\n=== Successfully Read HTML Content ===" << std::endl;
        std::cout << "Total size: " << htmlContent.size() << " bytes" << std::endl;
        std::cout << "Buffer size: " << len << " bytes" << std::endl;

        // 修复：确保不会复制超过缓冲区大小的内容
        if (htmlContent.size() >= len) {
            std::cerr << "WARNING: HTML content too large for buffer ("
                << htmlContent.size() << " >= " << len << ")" << std::endl;
            // 安全复制，为null终止符留出空间
            strncpy_s(res, len, htmlContent.c_str(), len - 1);
            res[len - 1] = '\0';
            std::cout << "Content truncated to " << (len - 1) << " bytes" << std::endl;
        }
        else {
            // 安全复制，确保有null终止符
            strcpy_s(res, len, htmlContent.c_str());
        }

        std::cout << "=======================================\n" << std::endl;
        return true;
    }
    else {
        std::cout << "No HTML content was read!" << std::endl;
        if (len > 0) {
            res[0] = '\0';
        }
        return false;
    }
}

bool InputContentCallbackd(bool (*callback)(char*, int)) {
    HANDLE inputPipe = GetHandleFromEnvironment("ELECTRON_INPUT_PIPE");
    while(true){
        if (!CheckHandleValidity(inputPipe, "Input pipe")) {
            return false;
        }

        std::cout << "Waiting for input content from pipe..." << std::endl;

        while (true) {
            DWORD bytesAvailable;
            if (!PeekNamedPipe(inputPipe, NULL, 0, NULL, &bytesAvailable, NULL)) {
                DWORD error = GetLastError();
                std::cerr << "PeekNamedPipe failed. Error: " << error << std::endl;
                return false;
            }
            if (bytesAvailable != 0)
                break;
        }

        // 读取输入内容
        std::string inputContent;
        char buffer[4096]{};  // 固定大小的缓冲区
        DWORD bytesRead;
        DWORD totalBytesRead = 0;

        while (true) {
            if (!ReadFile(inputPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                DWORD error = GetLastError();

                if (error == ERROR_BROKEN_PIPE) {
                    std::cout << "Input pipe closed by parent (normal)" << std::endl;
                    break;
                }
                else if (error == ERROR_NO_DATA) {
                    std::cout << "No more data in input pipe" << std::endl;
                    break;
                }
                else {
                    std::cerr << "ReadFile from input pipe failed. Error: " << error << std::endl;
                    break;
                }
            }

            if (bytesRead == 0) {
                std::cout << "End of input content (0 bytes read)" << std::endl;
                break;
            }

            totalBytesRead += bytesRead;
            buffer[bytesRead] = '\0';

            // 检查是否会超出总内容大小限制（可选）
            if (inputContent.size() + bytesRead > 50 * 1024 * 1024) { // 限制为50MB
                std::cerr << "Input content too large, exceeding 10MB limit" << std::endl;
                return false;
            }

            inputContent.append(buffer, bytesRead);

            std::cout << "Read " << bytesRead << " bytes from input pipe. Total: " << totalBytesRead << std::endl;

            // 如果读取的字节数少于缓冲区大小，可能已经读取完所有数据
            if (bytesRead < sizeof(buffer) - 1) {
                std::cout << "Read less than buffer size, assuming end of input data" << std::endl;
                break;
            }
        }

        if (!inputContent.empty()) {
            std::cout << "\n=== Successfully Read Input Content ===" << std::endl;
            std::cout << "Total size: " << inputContent.size() << " bytes" << std::endl;

            char* res = new char[inputContent.size() + 2];
            strcpy_s(res, inputContent.size() + 2, inputContent.c_str());
            if (!callback(res, inputContent.size() + 1)) {
                std::cerr << "Callback fuction returned error\n";
            }
            delete[] res;
            std::cout << "========================================\n" << std::endl;
        }
        else {
            std::cout << "No input content was read!" << std::endl;
        }
    }
}

extern "C" WEBHELPER_API int __stdcall SetInputContentCallback(bool (*callback)(char*, int)) {
    std::thread ic(InputContentCallbackd,callback);
    ic.detach();
    return (uintptr_t)ic.native_handle();
}

extern "C" WEBHELPER_API bool __stdcall StopInputListener(uintptr_t hl) {
    HANDLE threadHl = reinterpret_cast<HANDLE>(hl);
    return TerminateThread(threadHl, 0);
}

extern "C" WEBHELPER_API bool __stdcall SetOutputContent(char* str, const unsigned int len) {
    HANDLE outputPipe = GetHandleFromEnvironment("ELECTRON_OUTPUT_PIPE");
    if (!CheckHandleValidity(outputPipe, "Output pipe")) {
        return false;
    }

    std::cout << "Writing output content to pipe..." << std::endl;

    if (str == nullptr || len == 0) {
        std::cerr << "Invalid output content: null pointer or zero length" << std::endl;
        return false;
    }

    // 安全地获取字符串长度，避免缓冲区溢出
    size_t actualLen = 0;

    // 如果调用者提供了长度参数，使用它；否则计算字符串长度
    if (len > 0) {
        // 查找空终止符，但不超过len
        for (actualLen = 0; actualLen < len && str[actualLen] != '\0'; ++actualLen) {}

        // 如果没有找到空终止符，则使用len作为长度
        if (actualLen == len) {
            // 字符串可能不是以空字符结尾，或者缓冲区已满
            std::cout << "Output string may not be null-terminated or buffer is full, using provided length: " << len << std::endl;
            actualLen = len;
        }
    }
    else {
        std::cerr << "Invalid length parameter" << std::endl;
        return false;
    }

    if (actualLen == 0) {
        std::cout << "Empty output content, nothing to write" << std::endl;
        return true; // 空字符串也认为是成功的
    }

    std::cout << "Writing " << actualLen << " bytes to output pipe" << std::endl;
    std::cout << "Content preview: " << std::string(str, (((actualLen) < ((size_t)100)) ? (actualLen) : ((size_t)100)))
        << (actualLen > 100 ? "..." : "") << std::endl;

    DWORD bytesWritten;
    if (!WriteFile(outputPipe, str, (DWORD)actualLen, &bytesWritten, NULL)) {
        DWORD error = GetLastError();
        std::cerr << "WriteFile to output pipe failed. Error: " << error << std::endl;
        return false;
    }

    std::cout << "Successfully wrote " << bytesWritten << " bytes to output pipe" << std::endl;

    if (bytesWritten != actualLen) {
        std::cerr << "Warning: Only wrote " << bytesWritten << " of " << actualLen << " bytes" << std::endl;
        // 部分写入也可能被认为是成功的，取决于具体需求
    }

    return true;
}


