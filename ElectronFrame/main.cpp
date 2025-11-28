#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

class PipeTestApp {
private:
    HANDLE htmlPipeRead = INVALID_HANDLE_VALUE;
    HANDLE htmlPipeWrite = INVALID_HANDLE_VALUE;
    HANDLE inputPipeRead = INVALID_HANDLE_VALUE;
    HANDLE inputPipeWrite = INVALID_HANDLE_VALUE;
    HANDLE outputPipeRead = INVALID_HANDLE_VALUE;
    HANDLE outputPipeWrite = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION electronProcessInfo = { 0 };

public:
    bool InitializePipes() {
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

        // 创建 HTML 管道
        if (!CreatePipe(&htmlPipeRead, &htmlPipeWrite, &sa, 0)) {
            std::cerr << "Failed to create HTML pipe" << std::endl;
            return false;
        }

        // 创建输入管道 (主控 -> Electron)
        if (!CreatePipe(&inputPipeRead, &inputPipeWrite, &sa, 0)) {
            std::cerr << "Failed to create input pipe" << std::endl;
            return false;
        }

        // 创建输出管道 (Electron -> 主控)
        if (!CreatePipe(&outputPipeRead, &outputPipeWrite, &sa, 0)) {
            std::cerr << "Failed to create output pipe" << std::endl;
            return false;
        }

        std::cout << "Pipes created successfully" << std::endl;
        return true;
    }

    bool StartElectronApp(const std::string& electronPath) {
        // 设置环境变量传递管道句柄
        SetEnvironmentVariableA("ELECTRON_HTML_PIPE", std::to_string((intptr_t)htmlPipeRead).c_str());
        SetEnvironmentVariableA("ELECTRON_INPUT_PIPE", std::to_string((intptr_t)inputPipeRead).c_str());
        SetEnvironmentVariableA("ELECTRON_OUTPUT_PIPE", std::to_string((intptr_t)outputPipeWrite).c_str());

        std::string commandLine = "\"" + electronPath + "\"";
        std::cout << "Starting Electron with environment variables" << std::endl;

        STARTUPINFOA startupInfo = { sizeof(STARTUPINFOA) };

        if (!CreateProcessA(
            NULL,
            (LPSTR)commandLine.c_str(),
            NULL,
            NULL,
            TRUE,
            0,
            NULL,
            NULL,
            &startupInfo,
            &electronProcessInfo
        )) {
            std::cerr << "Failed to start Electron process. Error: " << GetLastError() << std::endl;
            return false;
        }

        std::cout << "Electron process started successfully" << std::endl;
        return true;
    }

    bool SendHtmlContent(const std::string& htmlContent) {
        DWORD bytesWritten;

        // 发送 HTML 内容
        if (!WriteFile(htmlPipeWrite, htmlContent.c_str(), htmlContent.length(), &bytesWritten, NULL)) {
            std::cerr << "Failed to write HTML content to pipe" << std::endl;
            return false;
        }

        std::cout << "Sent " << bytesWritten << " bytes of HTML content" << std::endl;

        // 关闭写端，表示 HTML 发送完成
        CloseHandle(htmlPipeWrite);
        htmlPipeWrite = INVALID_HANDLE_VALUE;

        return true;
    }

    bool SendMessageToElectron(const std::string& message) {
        DWORD bytesWritten;
        std::string messageWithNewline = message + "\n";

        if (!WriteFile(inputPipeWrite, messageWithNewline.c_str(), messageWithNewline.length(), &bytesWritten, NULL)) {
            std::cerr << "Failed to write message to input pipe" << std::endl;
            return false;
        }

        std::cout << "Sent message to Electron: " << message << std::endl;
        return true;
    }

    void ListenForMessages() {
        std::cout << "Listening for messages from Electron..." << std::endl;

        char buffer[4096];
        DWORD bytesRead;

        while (true) {
            if (ReadFile(outputPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                if (bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    std::string message(buffer);

                    // 移除换行符
                    if (!message.empty() && message.back() == '\n') {
                        message.pop_back();
                    }

                    std::cout << "Received from Electron: " << message << std::endl;

                    // 如果是特定消息，可以做出响应
                    if (message.find("ready") != std::string::npos) {
                        SendMessageToElectron("Hello from C++ app!");
                    }
                }
            }
            else {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE) {
                    std::cout << "Electron closed the pipe" << std::endl;
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::string GetTestHtml() {
        // 使用标准 C++ 字符串，避免 JavaScript 模板字符串语法
        return std::string() +
            "<!DOCTYPE html>\n" +
            "<html>\n" +
            "<head>\n" +
            "    <title>Test Page</title>\n" +
            "    <style>\n" +
            "        body { \n" +
            "            font-family: Arial, sans-serif; \n" +
            "            margin: 40px;\n" +
            "            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);\n" +
            "            color: white;\n" +
            "        }\n" +
            "        .container {\n" +
            "            max-width: 800px;\n" +
            "            margin: 0 auto;\n" +
            "            padding: 20px;\n" +
            "            background: rgba(255,255,255,0.1);\n" +
            "            border-radius: 10px;\n" +
            "            backdrop-filter: blur(10px);\n" +
            "        }\n" +
            "        button {\n" +
            "            background: #4CAF50;\n" +
            "            border: none;\n" +
            "            color: white;\n" +
            "            padding: 15px 32px;\n" +
            "            text-align: center;\n" +
            "            text-decoration: none;\n" +
            "            display: inline-block;\n" +
            "            font-size: 16px;\n" +
            "            margin: 4px 2px;\n" +
            "            cursor: pointer;\n" +
            "            border-radius: 5px;\n" +
            "        }\n" +
            "        #messages {\n" +
            "            background: rgba(0,0,0,0.3);\n" +
            "            padding: 15px;\n" +
            "            border-radius: 5px;\n" +
            "            margin-top: 20px;\n" +
            "            min-height: 100px;\n" +
            "            font-family: monospace;\n" +
            "        }\n" +
            "    </style>\n" +
            "</head>\n" +
            "<body>\n" +
            "    <div class=\"container\">\n" +
            "        <h1>Electron Pipe Test</h1>\n" +
            "        <p>This HTML was sent from the C++ application via anonymous pipes.</p>\n" +
            "        \n" +
            "        <div>\n" +
            "            <button onclick=\"sendTestMessage()\">Send Test Message</button>\n" +
            "            <button onclick=\"sendJsonMessage()\">Send JSON Message</button>\n" +
            "            <button onclick=\"getStatus()\">Get Status</button>\n" +
            "        </div>\n" +
            "        \n" +
            "        <div id=\"messages\"></div>\n" +
            "    </div>\n" +
            "\n" +
            "    <script>\n" +
            "        function logMessage(message) {\n" +
            "            const messagesDiv = document.getElementById('messages');\n" +
            "            const timestamp = new Date().toLocaleTimeString();\n" +
            "            messagesDiv.innerHTML += '[' + timestamp + '] ' + message + '<br>';\n" +
            "            messagesDiv.scrollTop = messagesDiv.scrollHeight;\n" +
            "        }\n" +
            "\n" +
            "        function sendTestMessage() {\n" +
            "            if (window.PipeAPI && window.PipeAPI.isReady()) {\n" +
            "                window.PipeAPI.sendMessage(\"Hello from webpage!\").then(function(success) {\n" +
            "                    logMessage('Message sent: ' + (success ? 'Success' : 'Failed'));\n" +
            "                });\n" +
            "            } else {\n" +
            "                logMessage(\"PipeAPI not ready yet\");\n" +
            "            }\n" +
            "        }\n" +
            "\n" +
            "        function sendJsonMessage() {\n" +
            "            if (window.PipeAPI && window.PipeAPI.isReady()) {\n" +
            "                const message = {\n" +
            "                    type: \"test\",\n" +
            "                    timestamp: new Date().toISOString(),\n" +
            "                    data: {\n" +
            "                        value: Math.random(),\n" +
            "                        items: [\"item1\", \"item2\", \"item3\"]\n" +
            "                    }\n" +
            "                };\n" +
            "                window.PipeAPI.sendMessage(message).then(function(success) {\n" +
            "                    logMessage('JSON message sent: ' + (success ? 'Success' : 'Failed'));\n" +
            "                });\n" +
            "            } else {\n" +
            "                logMessage(\"PipeAPI not ready yet\");\n" +
            "            }\n" +
            "        }\n" +
            "\n" +
            "        function getStatus() {\n" +
            "            if (window.PipeAPI) {\n" +
            "                const status = window.PipeAPI.isReady() ? \"Ready\" : \"Not Ready\";\n" +
            "                logMessage('PipeAPI status: ' + status);\n" +
            "            } else {\n" +
            "                logMessage(\"PipeAPI not available\");\n" +
            "            }\n" +
            "        }\n" +
            "\n" +
            "        // 监听来自 C++ 应用的消息\n" +
            "        if (window.PipeAPI) {\n" +
            "            window.PipeAPI.onMessage(function(data) {\n" +
            "                var displayData = typeof data === 'string' ? data : JSON.stringify(data);\n" +
            "                logMessage('Received: ' + displayData);\n" +
            "            });\n" +
            "        }\n" +
            "\n" +
            "        // 或者使用全局函数\n" +
            "        window.onPipeMessage = function(data) {\n" +
            "            var displayData = typeof data === 'string' ? data : JSON.stringify(data);\n" +
            "            logMessage('Global handler: ' + displayData);\n" +
            "        };\n" +
            "\n" +
            "        logMessage(\"Page loaded and ready\");\n" +
            "    </script>\n" +
            "</body>\n" +
            "</html>\n";
    }

    void RunTest() {
        // 获取测试 HTML 内容
        std::string testHtml = GetTestHtml();

        // 发送 HTML 内容
        if (!SendHtmlContent(testHtml)) {
            return;
        }

        // 等待 Electron 加载
        std::cout << "Waiting for Electron to load..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // 发送一些测试消息
        SendMessageToElectron("{\"type\": \"greeting\", \"message\": \"Hello from C++!\"}");
        std::this_thread::sleep_for(std::chrono::seconds(1));

        SendMessageToElectron("{\"type\": \"command\", \"action\": \"test\", \"value\": 123}");
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 开始监听来自 Electron 的消息
        ListenForMessages();
    }

    ~PipeTestApp() {
        // 清理资源
        if (htmlPipeRead != INVALID_HANDLE_VALUE) CloseHandle(htmlPipeRead);
        if (htmlPipeWrite != INVALID_HANDLE_VALUE) CloseHandle(htmlPipeWrite);
        if (inputPipeRead != INVALID_HANDLE_VALUE) CloseHandle(inputPipeRead);
        if (inputPipeWrite != INVALID_HANDLE_VALUE) CloseHandle(inputPipeWrite);
        if (outputPipeRead != INVALID_HANDLE_VALUE) CloseHandle(outputPipeRead);
        if (outputPipeWrite != INVALID_HANDLE_VALUE) CloseHandle(outputPipeWrite);

        if (electronProcessInfo.hProcess) {
            TerminateProcess(electronProcessInfo.hProcess, 0);
            CloseHandle(electronProcessInfo.hProcess);
            CloseHandle(electronProcessInfo.hThread);
        }
    }
};

int main() {
    std::cout << "Electron Pipe Test Application" << std::endl;
    std::cout << "==============================" << std::endl;

    PipeTestApp app;

    // 初始化管道
    if (!app.InitializePipes()) {
        return 1;
    }

    // 启动 Electron 应用
    // 注意：你需要将路径修改为你的 Electron 应用的实际路径
    std::string electronPath = "electron.exe"; // 或者完整的路径如 "C:\\path\\to\\electron-app.exe"

    if (!app.StartElectronApp(electronPath)) {
        return 1;
    }

    // 运行测试
    app.RunTest();

    std::cout << "Test completed. Press Enter to exit..." << std::endl;
    std::cin.get();

    return 0;
}