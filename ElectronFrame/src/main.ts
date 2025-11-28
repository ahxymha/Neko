import { app, BrowserWindow, ipcMain } from 'electron';
import * as path from 'path';
import * as fs from 'fs';

interface PipeHandles {
    htmlPipe: number;
    inputPipe: number;
    outputPipe: number;
}

class PipeElectronApp {
    private mainWindow: BrowserWindow | null = null;
    private pipeHandles: PipeHandles | null = null;
    private outputPipeHandle: number | null = null;

    constructor() {
        this.initializeApp();
    }

    private initializeApp(): void {
        app.whenReady().then(() => {
            this.createWindow();
            this.parseCommandLineArgs();
            this.setupPipes();
            this.setupIpcHandlers();
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

        this.mainWindow.loadFile(path.join(__dirname, 'index.html'));

        // 开发时打开调试工具
        if (process.env.NODE_ENV === 'development') {
            this.mainWindow.webContents.openDevTools();
        }
    }

    private parseCommandLineArgs(): void {
  // 从环境变量读取管道句柄
  const htmlPipeEnv = process.env.ELECTRON_HTML_PIPE;
  const inputPipeEnv = process.env.ELECTRON_INPUT_PIPE;
  const outputPipeEnv = process.env.ELECTRON_OUTPUT_PIPE;

  console.log('Environment variables:', {
    ELECTRON_HTML_PIPE: htmlPipeEnv,
    ELECTRON_INPUT_PIPE: inputPipeEnv,
    ELECTRON_OUTPUT_PIPE: outputPipeEnv
  });

  if (htmlPipeEnv && inputPipeEnv && outputPipeEnv) {
    this.pipeHandles = {
      htmlPipe: parseInt(htmlPipeEnv, 10),
      inputPipe: parseInt(inputPipeEnv, 10),
      outputPipe: parseInt(outputPipeEnv, 10)
    };
    console.log('Parsed pipe handles from environment:', this.pipeHandles);
  } else {
    console.warn('Not all pipe handles available in environment variables');
    console.warn('Available:', { htmlPipeEnv, inputPipeEnv, outputPipeEnv });
    this.pipeHandles = {
      htmlPipe: 0,
      inputPipe: 0,
      outputPipe: 0
    };
  }
}

    private setupPipes(): void {
        if (!this.pipeHandles) {
            console.warn('No pipe handles available');
            return;
        }

        this.setupHtmlPipe();
        this.setupInputPipe();
        this.outputPipeHandle = this.pipeHandles.outputPipe;
    }

    private setupHtmlPipe(): void {
        if (!this.pipeHandles) return;

        try {
            const htmlPipe = fs.createReadStream('', {
                fd: this.pipeHandles.htmlPipe
            });

            let htmlContent = '';

            // 修复：使用 any 类型来避免类型检查问题
            htmlPipe.on('data', (chunk: any) => {
                if (chunk instanceof Buffer) {
                    htmlContent += chunk.toString('utf8');
                } else if (typeof chunk === 'string') {
                    htmlContent += chunk;
                } else {
                    htmlContent += Buffer.from(chunk).toString('utf8');
                }
            });

            htmlPipe.on('end', () => {
                console.log('Received HTML content, length:', htmlContent.length);
                this.replaceWindowContent(htmlContent);
            });

            htmlPipe.on('error', (error: Error) => {
                console.error('Error reading HTML pipe:', error);
            });
        } catch (error) {
            console.error('Failed to setup HTML pipe:', error);
        }
    }

    private setupInputPipe(): void {
        if (!this.pipeHandles) return;

        try {
            const inputPipe = fs.createReadStream('', {
                fd: this.pipeHandles.inputPipe
            });

            // 修复：使用 any 类型来避免类型检查问题
            inputPipe.on('data', (chunk: any) => {
                let message: string;
                if (chunk instanceof Buffer) {
                    message = chunk.toString('utf8');
                } else if (typeof chunk === 'string') {
                    message = chunk;
                } else {
                    message = Buffer.from(chunk).toString('utf8');
                }

                console.log('Received message from input pipe:', message);
                this.sendMessageToRenderer(message);
            });

            inputPipe.on('error', (error: Error) => {
                console.error('Error reading input pipe:', error);
            });
        } catch (error) {
            console.error('Failed to setup input pipe:', error);
        }
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
            return this.sendToOutputPipe(message);
        });

        // 处理渲染进程就绪事件
        ipcMain.handle('renderer-ready', async () => {
            console.log('Renderer process is ready');
            return true;
        });
    }

    private sendToOutputPipe(message: string): boolean {
        if (!this.outputPipeHandle) {
            console.error('Output pipe not available');
            return false;
        }

        try {
            const buffer = Buffer.from(message, 'utf8');
            const bytesWritten = fs.writeSync(this.outputPipeHandle, buffer);
            console.log(`Sent ${bytesWritten} bytes to output pipe`);
            return bytesWritten > 0;
        } catch (error) {
            console.error('Error writing to output pipe:', error);
            return false;
        }
    }
}

// 启动应用
new PipeElectronApp();