// file: main_modified.ts
import { app, BrowserWindow, ipcMain } from 'electron';
import * as path from 'path';
import PipeManager from './pipe';

class PipeElectronApp {
    private mainWindow: BrowserWindow | null = null;
    private pipeManager: PipeManager | null = null;
    private stopInputListener: (() => void) | null = null;

    constructor() {
        this.initializeApp();
    }

    private initializeApp(): void {
        app.whenReady().then(() => {
            this.createWindow();
            this.setupPipeManager();
            this.setupIpcHandlers();
            this.loadHtmlContent();
            //this.startInputListening();
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

        // 清理资源
        app.on('before-quit', () => {
            this.cleanup();
        });
    }

    private createWindow(): void {
        this.mainWindow = new BrowserWindow({
            width: 1200,
            height: 800,
            webPreferences: {
                nodeIntegration: false,
                contextIsolation: true,
                preload: path.join(__dirname, 'preload.js'),
                webSecurity: false
            }
        });

        // 先加载一个占位页面
        this.mainWindow.loadFile(path.join(__dirname, 'loading.html'));

        // 开发时打开调试工具
        if (process.env.NODE_ENV === 'development') {
            this.mainWindow.webContents.openDevTools();
        }
    }

    private setupPipeManager(): void {
        try {
            // 创建PipeManager实例
            // 你可以根据需要调整DLL路径和缓冲区大小
            const dllPath = process.env.WEBHELPER_DLL_PATH || 'webhelper.dll';
            console.log(`Initializing PipeManager with DLL: ${dllPath}`);
            
            this.pipeManager = new PipeManager(dllPath, 15728640);
            
            
        } catch (error) {
            console.error(`Failed to setup PipeManager: ${error}`);
            this.showErrorPage('Failed to load communication library');
        }
    }

    private loadHtmlContent(): void {
        if (!this.pipeManager || !this.mainWindow) {
            console.error('PipeManager or mainWindow not available');
            return;
        }

        try {
            console.log('Loading HTML content from DLL...');
            const htmlContent = this.pipeManager.getHtmlContent();
            
            if (htmlContent) {
                console.log(`Successfully got HTML content, length: ${htmlContent.length}`);
                this.replaceWindowContent(htmlContent);
            } else {
                console.error('Failed to get HTML content from DLL');
                this.showErrorPage('Failed to load HTML content');
            }
        } catch (error) {
            console.error(`Error loading HTML content: ${error}`);
            this.showErrorPage('Error loading page content');
        }
    }

    private startInputListening(): void {
        if (!this.pipeManager) {
            console.warn('PipeManager not available for input listening');
            return;
        }

        console.log('Starting async input listening...');
        
        // 开始异步监听输入
        this.stopInputListener = this.pipeManager.waitForInputAsync(
            (message) => {
                if (message !== null) {
                    console.log(`Received message via async listener: ${message.length} chars`);
                    this.sendMessageToRenderer(message);
                }
            },
            100 // 轮询间隔
        );
    }

    private showErrorPage(errorMessage: string): void {
        if (!this.mainWindow) return;
        
        const errorHtml = `
            <!DOCTYPE html>
            <html>
            <head>
                <title>Error</title>
                <style>
                    body { 
                        font-family: Arial, sans-serif; 
                        display: flex; 
                        justify-content: center; 
                        align-items: center; 
                        height: 100vh; 
                        margin: 0; 
                        background-color: #f5f5f5;
                    }
                    .error-container { 
                        background: white; 
                        padding: 2rem; 
                        border-radius: 8px; 
                        box-shadow: 0 2px 10px rgba(0,0,0,0.1);
                        max-width: 500px;
                        text-align: center;
                    }
                    h1 { color: #d32f2f; }
                </style>
            </head>
            <body>
                <div class="error-container">
                    <h1>Error</h1>
                    <p>${errorMessage}</p>
                    <p>Please check the console for more details.</p>
                </div>
            </body>
            </html>
        `;
        
        this.replaceWindowContent(errorHtml);
    }

    private replaceWindowContent(htmlContent: string): void {
        if (!this.mainWindow) return;

        // 使用 data URL 加载 HTML 内容
        const dataUrl = `data:text/html;charset=utf-8,${encodeURIComponent(htmlContent)}`;
        this.mainWindow.loadURL(dataUrl);
    }

    private sendMessageToRenderer(message: string): void {
        if (!this.mainWindow) return;

        this.mainWindow.webContents.send('pipe-message', message);
    }

    private setupIpcHandlers(): void {
        // 处理来自渲染进程的消息
        ipcMain.handle('send-to-pipe', async (event, message: string) => {
            return this.sendToOutput(message);
        });

        // 处理渲染进程就绪事件
        ipcMain.handle('renderer-ready', async () => {
            console.log('Renderer process is ready');
            return true;
        });

        // 添加调试接口
        ipcMain.handle('debug-get-html', async () => {
            if (!this.pipeManager) return null;
            return this.pipeManager.getHtmlContent();
        });

    }

    private sendToOutput(message: string): boolean {
        if (!this.pipeManager) {
            console.error('PipeManager not available');
            return false;
        }

        try {
            return this.pipeManager.setOutputContent(message);
        } catch (error) {
            console.error(`Error sending to output: ${error}`);
            return false;
        }
    }

    private cleanup(): void {
        console.log('Cleaning up PipeElectronApp...');
        
        // 停止输入监听
        if (this.stopInputListener) {
            this.stopInputListener();
            this.stopInputListener = null;
        }
        
        // 释放PipeManager资源
        if (this.pipeManager) {
            this.pipeManager.dispose();
            this.pipeManager = null;
        }
    }
}

// 启动应用
new PipeElectronApp();