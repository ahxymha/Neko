// file: main_final.ts
import { app, BrowserWindow, ipcMain } from 'electron';
import * as path from 'path';
import PipeManager from './pipe';

class PipeElectronApp {
    private mainWindow: BrowserWindow | null = null;
    private pipeManager: PipeManager | null = null;

    constructor() {
        this.initializeApp();
    }

    private initializeApp(): void {
        app.whenReady().then(() => {
            console.log('Electron应用已就绪');

            this.createWindow();
            this.setupPipeManager();
            this.setupIpcHandlers();
            this.loadHtmlContent();
        });

        app.on('window-all-closed', () => {
            if (process.platform !== 'darwin') {
                app.quit();
            }
        });

        app.on('activate', () => {
            if (BrowserWindow.getAllWindows().length === 0) {
                this.createWindow();
            }
        });

        app.on('before-quit', () => {
            this.cleanup();
        });
    }

    private createWindow(): void {
        console.log('创建主窗口...');

        this.mainWindow = new BrowserWindow({
            width: 1200,
            height: 800,
            show: false,
            webPreferences: {
                nodeIntegration: false,
                contextIsolation: true,
                preload: path.join(__dirname, 'preload.js'),
                webSecurity: false,
                devTools: process.env.NODE_ENV === 'development'
            }
        });

        this.mainWindow.removeMenu();

        // 加载简单的加载页面
        this.loadLoadingPage();

        this.mainWindow.once('ready-to-show', () => {
            if (this.mainWindow) {
                this.mainWindow.show();
                this.mainWindow.focus();
            }
        });

        if (process.env.NODE_ENV === 'development') {
            this.mainWindow.webContents.openDevTools();
        }
    }

    private loadLoadingPage(): void {
        if (!this.mainWindow) return;

        const loadingHtml = `
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <title>加载中...</title>
            <style>
                body {
                    font-family: Arial, sans-serif;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    height: 100vh;
                    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                    color: white;
                    text-align: center;
                }
                .loading-container {
                    max-width: 500px;
                    padding: 40px;
                }
                .logo {
                    font-size: 4rem;
                    margin-bottom: 20px;
                }
                .title {
                    font-size: 2rem;
                    margin-bottom: 20px;
                }
                .status {
                    margin-top: 20px;
                    font-size: 1.1rem;
                }
                .progress {
                    width: 100%;
                    height: 4px;
                    background: rgba(255,255,255,0.2);
                    margin: 30px 0;
                    overflow: hidden;
                }
                .progress-bar {
                    height: 100%;
                    background: white;
                    width: 30%;
                    animation: loading 1.5s infinite;
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
                <h1 class="title">管道通信应用</h1>
                <div class="progress">
                    <div class="progress-bar"></div>
                </div>
                <div class="status" id="status">正在初始化管道通信...</div>
            </div>
            <script>
                const statusMessages = [
                    "正在初始化DLL通信...",
                    "正在建立管道连接...",
                    "正在加载HTML内容...",
                    "正在准备用户界面..."
                ];
                
                let index = 0;
                const statusElement = document.getElementById('status');
                
                setInterval(() => {
                    statusElement.textContent = statusMessages[index];
                    index = (index + 1) % statusMessages.length;
                }, 1500);
            </script>
        </body>
        </html>
        `;

        const dataUrl = `data:text/html;charset=utf-8,${encodeURIComponent(loadingHtml)}`;
        this.mainWindow.loadURL(dataUrl);
    }

    private setupPipeManager(): void {
        console.log('初始化管道管理器...');

        try {
            // 创建PipeManager实例，使用50MB缓冲区
            this.pipeManager = new PipeManager('./resources/webhelper.dll', 50 * 1024 * 1024);

            if (!this.pipeManager.isReady()) {
                throw new Error('无法加载DLL');
            }

            console.log('✅ 管道管理器初始化成功');

            // 设置非阻塞输入回调
            this.setupInputCallback();

        } catch (error: any) {
            console.error('初始化管道管理器失败:', error.message);
            this.showErrorPage(`初始化失败: ${error.message}`);
        }
    }

    private setupInputCallback(): void {
        if (!this.pipeManager) {
            console.error('管道管理器不可用');
            return;
        }

        console.log('设置非阻塞输入回调...');

        const success = this.pipeManager.setInputContentCallback((data: string) => {
            console.log(`📥 接收到输入数据: ${data.length}字节`);
            this.handleIncomingData(data);
        });

        if (success) {
            console.log('✅ 非阻塞输入回调设置成功');
        } else {
            console.error('❌ 设置输入回调失败，使用轮询模式');
            this.setupPollingInput();
        }
    }

    private setupPollingInput(): void {
        if (!this.pipeManager) return;

        console.log('使用轮询模式监听输入...');

        // 每100ms轮询一次输入
        const pollInterval = setInterval(() => {
            if (!this.pipeManager) {
                clearInterval(pollInterval);
                return;
            }

            // 注意：由于DLL没有WaitInputContent函数，这里我们只能依赖回调
            // 如果回调失败，轮询也无法工作
            // 这里只是作为一个示例
        }, 100);
    }

    private handleIncomingData(data: string): void {
        console.log('处理接收到的数据...');

        try {
            // 尝试解析为JSON
            const parsedData = JSON.parse(data);
            console.log('接收到JSON数据:', {
                type: parsedData.type || 'unknown',
                size: data.length
            });

            // 根据数据类型处理
            this.processDataByType(parsedData);

        } catch (error) {
            // 不是JSON，作为文本处理
            console.log('接收到文本数据:', data.substring(0, 100));
            this.sendMessageToRenderer(data);
        }
    }

    private processDataByType(data: any): void {
        if (!data.type) {
            this.sendMessageToRenderer(JSON.stringify(data));
            return;
        }
        if (data.type == 'html') {
            if (data.content && this.mainWindow) {
                console.log('加载HTML内容到窗口...');
                this.replaceWindowContent(data.content);
                return;
            }
        }
        this.sendMessageToRenderer(JSON.stringify(data));
        return;
    }

    private loadHtmlContent(): void {
        if (!this.pipeManager || !this.mainWindow) {
            console.error('管道管理器或主窗口不可用');
            return;
        }

        console.log('正在从DLL获取HTML内容...');

        // 通知渲染进程正在加载
        this.sendMessageToRenderer(JSON.stringify({
            type: 'loading',
            message: '正在获取HTML内容...'
        }));

        const htmlContent = this.pipeManager.getHtmlContent();

        if (htmlContent && htmlContent.length > 0) {
            console.log(`✅ 获取HTML内容成功，长度: ${htmlContent.length}字节`);

            // 通知加载完成
            this.sendMessageToRenderer(JSON.stringify({
                type: 'loading',
                message: 'HTML内容加载完成',
                size: htmlContent.length
            }));

            // 加载HTML内容到窗口
            this.replaceWindowContent(htmlContent);

        } else {
            console.error('获取的HTML内容为空或失败');
            this.showErrorPage('无法从管道获取HTML内容');
        }
    }

    private replaceWindowContent(htmlContent: string): void {
        if (!this.mainWindow) return;

        console.log('替换窗口内容...');

        const dataUrl = `data:text/html;charset=utf-8,${encodeURIComponent(htmlContent)}`;

        this.mainWindow.loadURL(dataUrl).catch(error => {
            console.error('加载HTML内容失败:', error);
            this.showErrorPage(`加载HTML内容失败: ${error.message}`);
        });
    }

    private showErrorPage(errorMessage: string): void {
        if (!this.mainWindow) return;

        const errorHtml = `
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <title>错误</title>
            <style>
                body {
                    font-family: Arial, sans-serif;
                    display: flex;
                    justify-content: center;
                    align-items: center;
                    height: 100vh;
                    background: #f8f9fa;
                    color: #333;
                    text-align: center;
                }
                .error-container {
                    max-width: 600px;
                    padding: 40px;
                    background: white;
                    border-radius: 8px;
                    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
                }
                .error-icon {
                    font-size: 4rem;
                    margin-bottom: 20px;
                }
                h1 {
                    color: #dc3545;
                    margin-bottom: 20px;
                }
                .error-message {
                    margin-bottom: 30px;
                    line-height: 1.6;
                }
                .actions {
                    display: flex;
                    gap: 10px;
                    justify-content: center;
                }
                .btn {
                    padding: 10px 20px;
                    border: none;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 1rem;
                }
                .btn-primary {
                    background: #007bff;
                    color: white;
                }
                .btn-secondary {
                    background: #6c757d;
                    color: white;
                }
            </style>
        </head>
        <body>
            <div class="error-container">
                <div class="error-icon">❌</div>
                <h1>应用程序错误</h1>
                <div class="error-message">${errorMessage}</div>
                <div class="actions">
                    <button class="btn btn-primary" onclick="window.location.reload()">重新加载</button>
                    <button class="btn btn-secondary" onclick="window.close()">关闭应用</button>
                </div>
            </div>
        </body>
        </html>
        `;

        const dataUrl = `data:text/html;charset=utf-8,${encodeURIComponent(errorHtml)}`;
        this.mainWindow.loadURL(dataUrl);
    }

    private sendMessageToRenderer(message: string): void {
        if (!this.mainWindow) return;

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

        // 重新加载HTML内容
        ipcMain.handle('reload-html', async () => {
            console.log('重新加载HTML内容');
            this.loadHtmlContent();
            return true;
        });
    }

    private sendToOutputPipe(message: string): boolean {
        if (!this.pipeManager) {
            console.error('管道管理器不可用');
            return false;
        }

        try {
            return this.pipeManager.setOutputContent(message);
        } catch (error: any) {
            console.error('发送消息到输出管道失败:', error.message);
            return false;
        }
    }

    private cleanup(): void {
        console.log('清理资源...');

        if (this.pipeManager) {
            // 停止输入监听
            this.pipeManager.stopInputListener();
            this.pipeManager = null;
        }

        console.log('✅ 资源清理完成');
    }
}

// 启动应用
new PipeElectronApp();