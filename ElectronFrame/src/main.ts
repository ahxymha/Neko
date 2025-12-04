// file: main.ts
import { app, BrowserWindow, ipcMain } from 'electron';
import * as path from 'path';
import CallbackPipeManager from './pipe';

class PipeElectronApp {
    private mainWindow: BrowserWindow | null = null;
    private pipeManager: CallbackPipeManager | null = null;
    private isShuttingDown: boolean = false;

    constructor() {
        this.initializeApp();
    }

    private initializeApp(): void {
        // 处理未捕获的异常
        process.on('uncaughtException', (error) => {
            console.error('未捕获的异常:', error);
        });

        process.on('unhandledRejection', (reason, promise) => {
            console.error('未处理的Promise拒绝:', reason);
        });

        app.whenReady().then(() => {
            console.log('Electron应用已就绪');
            this.createWindow();
            this.setupPipeManager();
            this.setupIpcHandlers();
            this.setupErrorHandling();
        });

        app.on('window-all-closed', () => {
            if (process.platform !== 'darwin') {
                console.log('所有窗口已关闭，正在退出应用...');
                app.quit();
            }
        });

        app.on('activate', () => {
            if (BrowserWindow.getAllWindows().length === 0) {
                console.log('重新创建窗口');
                this.createWindow();
            }
        });

        app.on('before-quit', (event) => {
            if (!this.isShuttingDown) {
                event.preventDefault();
                this.cleanup().then(() => {
                    this.isShuttingDown = true;
                    app.quit();
                });
            }
        });
    }

    private createWindow(): void {
        console.log('创建主窗口...');

        this.mainWindow = new BrowserWindow({
            width: 1400,
            height: 900,
            minWidth: 800,
            minHeight: 600,
            show: false, // 先隐藏，等内容加载完成再显示
            webPreferences: {
                nodeIntegration: false,
                contextIsolation: true,
                preload: path.join(__dirname, 'preload.js'),
                webSecurity: false,
                spellcheck: false
            },
            icon: path.join(__dirname, 'icon.ico'),
            title: 'Electron管道通信应用',
            backgroundColor: '#f5f5f5'
        });

        // 加载启动页面
        const loadingPage = this.createLoadingPage();
        this.mainWindow.loadURL(`data:text/html;charset=utf-8,${encodeURIComponent(loadingPage)}`);

        // 当页面加载完成时显示窗口
        this.mainWindow.once('ready-to-show', () => {
            console.log('窗口已准备就绪，显示窗口');
            if (this.mainWindow) {
                this.mainWindow.show();
                this.mainWindow.focus();
            }
        });

        // 处理窗口关闭
        this.mainWindow.on('closed', () => {
            console.log('主窗口已关闭');
            this.mainWindow = null;
        });

        // 开发时打开调试工具
        if (process.env.NODE_ENV === 'development') {
            this.mainWindow.webContents.openDevTools({ mode: 'right' });
        }

        // 监听页面加载失败
        this.mainWindow.webContents.on('did-fail-load', (event, errorCode, errorDescription) => {
            console.error(`页面加载失败: ${errorCode} - ${errorDescription}`);
            this.showErrorPage(`页面加载失败: ${errorDescription}`);
        });
    }

    private createLoadingPage(): string {
        return `
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <title>加载中...</title>
            <style>
                * { margin: 0; padding: 0; box-sizing: border-box; }
                body {
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    height: 100vh;
                    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                    color: white;
                }
                .loading-container {
                    text-align: center;
                    max-width: 500px;
                    padding: 40px;
                }
                .logo {
                    font-size: 3rem;
                    margin-bottom: 20px;
                    animation: pulse 2s infinite;
                }
                .title {
                    font-size: 2rem;
                    margin-bottom: 20px;
                    font-weight: 300;
                }
                .progress-container {
                    width: 100%;
                    height: 8px;
                    background: rgba(255,255,255,0.2);
                    border-radius: 4px;
                    overflow: hidden;
                    margin: 30px 0;
                }
                .progress-bar {
                    height: 100%;
                    background: white;
                    width: 30%;
                    border-radius: 4px;
                    animation: loading 1.5s ease-in-out infinite alternate;
                }
                .message {
                    margin-top: 20px;
                    font-size: 1.1rem;
                    opacity: 0.8;
                }
                .details {
                    margin-top: 15px;
                    font-size: 0.9rem;
                    opacity: 0.6;
                    font-family: monospace;
                }
                @keyframes pulse {
                    0%, 100% { opacity: 1; }
                    50% { opacity: 0.7; }
                }
                @keyframes loading {
                    0% { transform: translateX(-100%); }
                    100% { transform: translateX(300%); }
                }
            </style>
        </head>
        <body>
            <div class="loading-container">
                <div class="logo">🚀</div>
                <h1 class="title">Electron管道通信应用</h1>
                <div class="progress-container">
                    <div class="progress-bar"></div>
                </div>
                <div class="message">正在初始化管道通信系统...</div>
                <div class="details">
                    正在加载DLL: webhelper.dll<br>
                    缓冲区: 50MB<br>
                    模式: 非阻塞回调
                </div>
            </div>
            <script>
                // 更新加载消息
                const messages = [
                    "正在初始化通信管道...",
                    "正在加载DLL模块...",
                    "正在建立回调监听...",
                    "正在准备HTML内容...",
                    "即将完成初始化..."
                ];
                
                let index = 0;
                const messageElement = document.querySelector('.message');
                
                setInterval(() => {
                    messageElement.textContent = messages[index];
                    index = (index + 1) % messages.length;
                }, 2000);
            </script>
        </body>
        </html>`;
    }

    private setupPipeManager(): void {
        console.log('初始化管道管理器...');

        try {
            // 从环境变量获取DLL路径或使用默认值
            const dllPath = process.env.WEBHELPER_DLL_PATH || 'webhelper.dll';
            console.log(`DLL路径: ${dllPath}`);

            // 检查环境变量
            const envVars = {
                ELECTRON_HTML_PIPE: process.env.ELECTRON_HTML_PIPE || '未设置',
                ELECTRON_INPUT_PIPE: process.env.ELECTRON_INPUT_PIPE || '未设置',
                ELECTRON_OUTPUT_PIPE: process.env.ELECTRON_OUTPUT_PIPE || '未设置'
            };

            console.log('环境变量检查:', envVars);

            // 使用50MB缓冲区
            this.pipeManager = new CallbackPipeManager(dllPath, 50 * 1024 * 1024);

            if (!this.pipeManager.isReady()) {
                throw new Error('CallbackPipeManager初始化失败 - DLL可能未正确加载');
            }

            console.log('✅ 管道管理器初始化成功');

            // 设置回调监听
            this.setupInputCallback();

            // 加载HTML内容
            this.loadHtmlContent();

        } catch (error: any) {
            console.error(`初始化管道管理器失败: ${error.message}`);
            this.showErrorPage(`初始化失败: ${error.message}`);
        }
    }

    private setupInputCallback(): void {
        if (!this.pipeManager) {
            console.error('管道管理器不可用，无法设置回调');
            return;
        }

        console.log('设置非阻塞输入回调...');

        const success = this.pipeManager.setInputContentCallback((data: string) => {
            console.log(`📥 接收到输入数据: ${data.length}字节`);

            // 处理接收到的数据
            this.handleIncomingData(data);

            // 发送给渲染进程
            this.sendMessageToRenderer(data);
        });

        if (success) {
            console.log('✅ 非阻塞输入回调设置成功');
        } else {
            console.error('❌ 设置输入回调失败');
            // 尝试使用阻塞方式作为后备
            console.log('尝试使用阻塞方式作为后备...');
            this.setupBlockingInputFallback();
        }
    }

    private setupBlockingInputFallback(): void {
        // 如果没有非阻塞接口，使用轮询方式
        console.log('使用轮询方式监听输入...');

        const pollInterval = setInterval(async () => {
            if (!this.pipeManager || !this.pipeManager.isReady()) {
                clearInterval(pollInterval);
                return;
            }

            try {
                // 注意：WaitInputContent是阻塞调用，可能会影响性能
                // 在实际应用中，应该使用单独的Worker线程
                const data = await this.pipeManager.waitForInput();
                if (data) {
                    console.log(`📥 [轮询] 接收到输入数据: ${data.length}字节`);
                    this.sendMessageToRenderer(data);
                }
            } catch (error) {
                console.error('轮询输入失败:', error);
            }
        }, 100); // 每100ms轮询一次
    }

    private handleIncomingData(data: string): void {
        // 这里可以添加对接收数据的处理逻辑
        // 例如：解析JSON、记录日志、触发事件等

        try {
            // 尝试解析为JSON
            const jsonData = JSON.parse(data);
            console.log('接收到JSON数据:', {
                type: jsonData.type || 'unknown',
                size: data.length,
                timestamp: new Date().toISOString()
            });

            // 触发自定义事件
            if (jsonData.type === 'command') {
                this.handleCommand(jsonData);
            }
        } catch {
            // 不是JSON，当作普通文本处理
            console.log('接收到文本数据:', {
                preview: data.length > 100 ? data.substring(0, 100) + '...' : data,
                size: data.length
            });
        }
    }

    private handleCommand(command: any): void {
        console.log('处理命令:', command);

        // 这里可以添加命令处理逻辑
        switch (command.command) {
            case 'reload':
                if (this.mainWindow) {
                    this.mainWindow.reload();
                }
                break;
            case 'get_status':
                this.sendStatusUpdate();
                break;
            case 'log':
                console.log('远程日志:', command.message);
                break;
            default:
                console.log('未知命令:', command);
        }
    }

    private sendStatusUpdate(): void {
        const status = {
            timestamp: new Date().toISOString(),
            window: {
                isVisible: this.mainWindow?.isVisible() || false,
                isFocused: this.mainWindow?.isFocused() || false
            },
            pipeManager: {
                isReady: this.pipeManager?.isReady() || false,
                isListening: this.pipeManager?.isListeningActive() || false
            },
            memory: process.memoryUsage()
        };

        this.sendMessageToRenderer(JSON.stringify({
            type: 'status',
            data: status
        }));
    }

    private async loadHtmlContent(): Promise<void> {
        if (!this.pipeManager || !this.mainWindow) {
            console.error('管道管理器或主窗口不可用');
            return;
        }

        try {
            console.log('正在从管道获取HTML内容...');

            // 显示加载状态
            this.mainWindow.webContents.send('pipe-message', JSON.stringify({
                type: 'loading',
                message: '正在获取HTML内容...'
            }));

            const htmlContent = await this.pipeManager.getHtmlContent();

            if (htmlContent && htmlContent.length > 0) {
                console.log(`✅ 获取HTML内容成功，长度: ${htmlContent.length}字节`);

                // 发送加载完成消息
                this.mainWindow.webContents.send('pipe-message', JSON.stringify({
                    type: 'loading',
                    message: 'HTML内容加载完成',
                    size: htmlContent.length
                }));

                // 替换窗口内容
                this.replaceWindowContent(htmlContent);
            } else {
                console.error('获取的HTML内容为空');
                this.showErrorPage('无法从管道获取HTML内容（内容为空）');
            }
        } catch (error: any) {
            console.error(`获取HTML内容时出错: ${error.message}`);
            this.showErrorPage(`获取HTML内容失败: ${error.message}`);
        }
    }

    private replaceWindowContent(htmlContent: string): void {
        if (!this.mainWindow) return;

        console.log('替换窗口内容...');

        // 使用data URL加载HTML内容
        const dataUrl = `data:text/html;charset=utf-8,${encodeURIComponent(htmlContent)}`;

        // 在加载新内容前发送通知
        this.mainWindow.webContents.send('pipe-message', JSON.stringify({
            type: 'navigation',
            message: '正在加载新内容...'
        }));

        this.mainWindow.loadURL(dataUrl).catch(error => {
            console.error('加载HTML内容失败:', error);
            this.showErrorPage(`加载HTML内容失败: ${error.message}`);
        });
    }

    private showErrorPage(errorMessage: string): void {
        if (!this.mainWindow) return;

        console.error('显示错误页面:', errorMessage);

        const errorHtml = `
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <title>错误 - Electron管道通信应用</title>
            <style>
                * { margin: 0; padding: 0; box-sizing: border-box; }
                body {
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    min-height: 100vh;
                    background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
                    padding: 20px;
                }
                .error-container {
                    background: white;
                    border-radius: 12px;
                    padding: 40px;
                    max-width: 600px;
                    width: 100%;
                    box-shadow: 0 20px 60px rgba(0,0,0,0.2);
                    text-align: center;
                }
                .error-icon {
                    font-size: 4rem;
                    margin-bottom: 20px;
                }
                h1 {
                    color: #e74c3c;
                    margin-bottom: 15px;
                    font-size: 1.8rem;
                }
                .error-message {
                    color: #555;
                    margin-bottom: 25px;
                    line-height: 1.6;
                    font-size: 1.1rem;
                }
                .error-details {
                    background: #f8f9fa;
                    padding: 20px;
                    border-radius: 8px;
                    margin-bottom: 25px;
                    text-align: left;
                    font-family: 'Monaco', 'Consolas', monospace;
                    font-size: 0.9rem;
                    white-space: pre-wrap;
                    word-wrap: break-word;
                    max-height: 200px;
                    overflow-y: auto;
                }
                .actions {
                    display: flex;
                    gap: 10px;
                    justify-content: center;
                    flex-wrap: wrap;
                }
                .btn {
                    padding: 12px 24px;
                    border: none;
                    border-radius: 6px;
                    font-size: 1rem;
                    font-weight: 600;
                    cursor: pointer;
                    transition: all 0.3s;
                    display: inline-flex;
                    align-items: center;
                    justify-content: center;
                    text-decoration: none;
                    color: white;
                    min-width: 140px;
                }
                .btn-retry {
                    background: #3498db;
                }
                .btn-retry:hover {
                    background: #2980b9;
                    transform: translateY(-2px);
                }
                .btn-exit {
                    background: #95a5a6;
                }
                .btn-exit:hover {
                    background: #7f8c8d;
                    transform: translateY(-2px);
                }
                .btn-console {
                    background: #2ecc71;
                }
                .btn-console:hover {
                    background: #27ae60;
                    transform: translateY(-2px);
                }
                .btn-icon {
                    margin-right: 8px;
                }
            </style>
        </head>
        <body>
            <div class="error-container">
                <div class="error-icon">❌</div>
                <h1>应用程序遇到错误</h1>
                <div class="error-message">${escapeHtml(errorMessage)}</div>
                
                <div class="error-details">
                    时间: ${new Date().toLocaleString()}
                    环境变量:
                      ELECTRON_HTML_PIPE: ${process.env.ELECTRON_HTML_PIPE || '未设置'}
                      ELECTRON_INPUT_PIPE: ${process.env.ELECTRON_INPUT_PIPE || '未设置'}
                      ELECTRON_OUTPUT_PIPE: ${process.env.ELECTRON_OUTPUT_PIPE || '未设置'}
                    平台: ${process.platform} ${process.arch}
                    Node版本: ${process.version}
                    Electron版本: ${process.versions.electron}
                </div>
                
                <div class="actions">
                    <button class="btn btn-retry" onclick="window.location.reload()">
                        <span class="btn-icon">🔄</span> 重新加载
                    </button>
                    <button class="btn btn-console" onclick="alert('请查看开发者工具控制台获取详细信息')">
                        <span class="btn-icon">📋</span> 查看控制台
                    </button>
                    <button class="btn btn-exit" onclick="window.close()">
                        <span class="btn-icon">🚪</span> 退出应用
                    </button>
                </div>
            </div>
            
            <script>
                function escapeHtml(text) {
                    const div = document.createElement('div');
                    div.textContent = text;
                    return div.innerHTML;
                }
            </script>
        </body>
        </html>`;

        this.mainWindow.loadURL(`data:text/html;charset=utf-8,${encodeURIComponent(errorHtml)}`);
    }

    private sendMessageToRenderer(message: string): void {
        if (!this.mainWindow || this.mainWindow.isDestroyed()) {
            console.warn('主窗口不可用，无法发送消息');
            return;
        }

        try {
            this.mainWindow.webContents.send('pipe-message', message);
        } catch (error) {
            console.error('发送消息到渲染进程失败:', error);
        }
    }

    private setupIpcHandlers(): void {
        console.log('设置IPC处理器...');

        // 处理来自渲染进程的消息
        ipcMain.handle('send-to-pipe', async (event, message: string) => {
            console.log(`📤 收到渲染进程消息: ${message.length}字节`);
            return this.sendToOutputPipe(message);
        });

        // 处理渲染进程就绪事件
        ipcMain.handle('renderer-ready', async () => {
            console.log('渲染进程已就绪');
            return true;
        });

        // 调试接口
        ipcMain.handle('debug-get-html', async () => {
            console.log('调试: 获取HTML内容');
            if (!this.pipeManager) return null;
            return this.pipeManager.getHtmlContent();
        });

        ipcMain.handle('debug-set-output', async (event, message: string) => {
            console.log(`调试: 设置输出内容: ${message.length}字节`);
            if (!this.pipeManager) return false;
            return this.pipeManager.setOutputContent(message);
        });

        ipcMain.handle('debug-start-listener', async () => {
            console.log('调试: 启动输入监听');
            if (!this.pipeManager) return false;
            return this.pipeManager.setInputContentCallback((data: string) => {
                console.log(`调试回调接收到数据: ${data.length}字节`);
                this.sendMessageToRenderer(`[调试] ${data}`);
                return true;
            });
        });

        ipcMain.handle('debug-stop-listener', async () => {
            console.log('调试: 停止输入监听');
            if (!this.pipeManager) return false;
            return this.pipeManager.stopInputListener();
        });

        // 环境信息接口
        ipcMain.handle('get-environment-info', async () => {
            return {
                ELECTRON_HTML_PIPE: process.env.ELECTRON_HTML_PIPE,
                ELECTRON_INPUT_PIPE: process.env.ELECTRON_INPUT_PIPE,
                ELECTRON_OUTPUT_PIPE: process.env.ELECTRON_OUTPUT_PIPE,
                dllPath: process.env.WEBHELPER_DLL_PATH || 'webhelper.dll',
                bufferSize: 50 * 1024 * 1024,
                platform: process.platform,
                arch: process.arch,
                nodeVersion: process.version,
                electronVersion: process.versions.electron
            };
        });

        // 系统状态接口
        ipcMain.handle('get-system-status', async () => {
            return {
                isDllLoaded: this.pipeManager?.isReady() || false,
                isListening: this.pipeManager?.isListeningActive() || false,
                isReady: !!this.pipeManager?.isReady(),
                bufferSize: 50 * 1024 * 1024,
                windowVisible: this.mainWindow?.isVisible() || false,
                uptime: process.uptime()
            };
        });

        // 其他控制接口
        ipcMain.handle('reload-html', async () => {
            console.log('重新加载HTML内容');
            this.loadHtmlContent();
            return true;
        });

        ipcMain.handle('restart-listener', async () => {
            console.log('重启输入监听');
            if (this.pipeManager) {
                this.pipeManager.stopInputListener();
                return this.pipeManager.setInputContentCallback((data: string) => {
                    this.sendMessageToRenderer(data);
                    return true;
                });
            }
            return false;
        });

        console.log('✅ IPC处理器设置完成');
    }

    private setupErrorHandling(): void {
        // 主进程错误处理
        process.on('uncaughtException', (error) => {
            console.error('主进程未捕获异常:', error);

            // 发送错误信息到渲染进程
            this.sendMessageToRenderer(JSON.stringify({
                type: 'error',
                message: '主进程异常',
                error: error.message,
                stack: error.stack
            }));
        });
    }

    private sendToOutputPipe(message: string): boolean {
        if (!this.pipeManager) {
            console.error('管道管理器不可用');
            return false;
        }

        try {
            const success = this.pipeManager.setOutputContent(message);
            console.log(`📤 发送到输出管道: ${success ? '成功' : '失败'} (${message.length}字节)`);
            return success;
        } catch (error: any) {
            console.error(`发送消息到输出管道时出错: ${error.message}`);
            return false;
        }
    }

    private async cleanup(): Promise<void> {
        console.log('开始清理资源...');

        try {
            // 停止管道监听
            if (this.pipeManager) {
                console.log('停止管道监听...');
                this.pipeManager.dispose();
                this.pipeManager = null;
            }

            // 关闭所有窗口
            BrowserWindow.getAllWindows().forEach(window => {
                if (!window.isDestroyed()) {
                    window.destroy();
                }
            });

            console.log('✅ 资源清理完成');
        } catch (error) {
            console.error('清理资源时出错:', error);
        }
    }
}

// HTML转义辅助函数
function escapeHtml(text: string): string {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// 启动应用
console.log('启动Electron管道通信应用...');
new PipeElectronApp();