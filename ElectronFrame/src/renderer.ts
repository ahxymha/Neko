// file: renderer.ts
interface ElectronAPI {
    sendToPipe: (message: string) => Promise<boolean>;
    notifyReady: () => Promise<boolean>;
    onPipeMessage: (callback: (message: string) => void) => void;
    removeAllListeners: (channel: string) => void;
    debugGetHtml: () => Promise<string | null>;
    debugSetOutput: (message: string) => Promise<boolean>;
    debugStartListener: () => Promise<boolean>;
    debugStopListener: () => Promise<boolean>;
    getEnvironmentInfo: () => Promise<any>;
    getSystemStatus: () => Promise<any>;
}

declare global {
    interface Window {
        electronAPI: ElectronAPI;
    }
}

class EnhancedRenderer {
    private isInitialized: boolean = false;
    private messageQueue: any[] = [];
    private connectionStatus: 'disconnected' | 'connecting' | 'connected' = 'disconnected';

    constructor() {
        this.initialize();
    }

    private async initialize(): Promise<void> {
        console.log('EnhancedRenderer 初始化...');

        // 检查 electronAPI 是否存在
        if (!window.electronAPI) {
            console.error('Electron API 不可用');
            setTimeout(() => this.initialize(), 500);
            return;
        }

        try {
            // 获取系统信息
            const envInfo = await window.electronAPI.getEnvironmentInfo();
            console.log('环境信息:', envInfo);

            const systemStatus = await window.electronAPI.getSystemStatus();
            console.log('系统状态:', systemStatus);

            // 通知主进程渲染进程已就绪
            await window.electronAPI.notifyReady();

            // 设置消息监听器
            this.setupMessageListener();

            // 设置全局错误处理
            this.setupErrorHandling();

            // 设置连接状态监控
            this.setupConnectionMonitoring();

            this.isInitialized = true;
            this.connectionStatus = 'connected';

            console.log('✅ EnhancedRenderer 初始化完成');

            // 处理队列中的消息
            this.processMessageQueue();

        } catch (error) {
            console.error('EnhancedRenderer 初始化失败:', error);
            setTimeout(() => this.initialize(), 1000);
        }
    }

    private setupMessageListener(): void {
        window.electronAPI.onPipeMessage((message: string) => {
            console.log('📨 收到主进程消息:', message.substring(0, 100));

            try {
                // 尝试解析为JSON
                const data = JSON.parse(message);
                this.handleParsedMessage(data);
            } catch {
                // 不是JSON，当作字符串处理
                this.handleStringMessage(message);
            }
        });
    }

    private handleParsedMessage(data: any): void {
        console.log('解析后的消息:', data);

        // 根据消息类型处理
        switch (data.type) {
            case 'status':
                this.updateConnectionStatus(data.data);
                break;
            case 'error':
                this.showError(data.message);
                break;
            case 'loading':
                this.showLoadingMessage(data.message);
                break;
            case 'command':
                this.executeCommand(data);
                break;
            default:
                // 触发自定义事件
                const event = new CustomEvent('electron-message', {
                    detail: data
                });
                window.dispatchEvent(event);
        }
    }

    private handleStringMessage(message: string): void {
        console.log('字符串消息:', message);

        // 触发自定义事件
        const event = new CustomEvent('electron-message', {
            detail: { type: 'string', message }
        });
        window.dispatchEvent(event);

        // 如果定义了全局处理函数
        if (typeof (window as any).onElectronMessage === 'function') {
            (window as any).onElectronMessage(message);
        }
    }

    private updateConnectionStatus(status: any): void {
        console.log('连接状态更新:', status);

        // 更新UI状态
        const statusElement = document.getElementById('connection-status');
        if (statusElement) {
            statusElement.textContent = status.isConnected ? '已连接' : '未连接';
            statusElement.className = status.isConnected ? 'connected' : 'disconnected';
        }
    }

    private showError(message: string): void {
        console.error('应用程序错误:', message);

        // 显示错误通知
        if (typeof (window as any).showNotification === 'function') {
            (window as any).showNotification('错误', message, 'error');
        }
    }

    private showLoadingMessage(message: string): void {
        console.log('加载消息:', message);

        // 更新加载状态
        const loadingElement = document.getElementById('loading-message');
        if (loadingElement) {
            loadingElement.textContent = message;
        }
    }

    private executeCommand(command: any): void {
        console.log('执行命令:', command);

        switch (command.command) {
            case 'reload':
                window.location.reload();
                break;
            case 'focus':
                window.focus();
                break;
            // 添加更多命令处理
        }
    }

    private setupErrorHandling(): void {
        // 捕获渲染进程错误
        window.addEventListener('error', (event) => {
            console.error('渲染进程错误:', event.error);

            // 发送错误信息到主进程
            this.sendMessage({
                type: 'renderer-error',
                message: event.error?.message || 'Unknown error',
                stack: event.error?.stack,
                filename: event.filename,
                lineno: event.lineno,
                colno: event.colno
            });
        });

        // 捕获未处理的Promise拒绝
        window.addEventListener('unhandledrejection', (event) => {
            console.error('未处理的Promise拒绝:', event.reason);

            this.sendMessage({
                type: 'unhandled-rejection',
                reason: event.reason?.toString()
            });
        });
    }

    private setupConnectionMonitoring(): void {
        // 定期检查连接状态
        setInterval(async () => {
            try {
                const status = await window.electronAPI.getSystemStatus();
                this.connectionStatus = status.isReady ? 'connected' : 'disconnected';

                // 触发连接状态变化事件
                const event = new CustomEvent('connection-status-change', {
                    detail: { status: this.connectionStatus }
                });
                window.dispatchEvent(event);
            } catch (error) {
                this.connectionStatus = 'disconnected';
                console.error('检查连接状态失败:', error);
            }
        }, 5000);
    }

    private processMessageQueue(): void {
        while (this.messageQueue.length > 0) {
            const message = this.messageQueue.shift();
            this.sendMessage(message);
        }
    }

    public async sendMessage(message: string | object): Promise<boolean> {
        if (!this.isInitialized) {
            console.log('消息排队等待:', message);
            this.messageQueue.push(message);
            return false;
        }

        try {
            const messageStr = typeof message === 'string' ? message : JSON.stringify(message);
            return await window.electronAPI.sendToPipe(messageStr);
        } catch (error) {
            console.error('发送消息失败:', error);
            return false;
        }
    }

    public async debugGetHtml(): Promise<string | null> {
        if (!this.isInitialized) return null;

        try {
            return await window.electronAPI.debugGetHtml();
        } catch (error) {
            console.error('调试获取HTML失败:', error);
            return null;
        }
    }

    public async getSystemInfo(): Promise<any> {
        if (!this.isInitialized) return null;

        try {
            const [envInfo, systemStatus] = await Promise.all([
                window.electronAPI.getEnvironmentInfo(),
                window.electronAPI.getSystemStatus()
            ]);

            return {
                environment: envInfo,
                system: systemStatus,
                renderer: {
                    userAgent: navigator.userAgent,
                    platform: navigator.platform,
                    language: navigator.language,
                    online: navigator.onLine
                }
            };
        } catch (error) {
            console.error('获取系统信息失败:', error);
            return null;
        }
    }

    public isReady(): boolean {
        return this.isInitialized && this.connectionStatus === 'connected';
    }

    public getConnectionStatus(): string {
        return this.connectionStatus;
    }
}

// 等待DOM加载完成后初始化
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        initializeEnhancedRenderer();
    });
} else {
    initializeEnhancedRenderer();
}

function initializeEnhancedRenderer(): void {
    // 创建全局实例
    const enhancedRenderer = new EnhancedRenderer();

    // 暴露给全局的API
    (window as any).EnhancedPipeAPI = {
        sendMessage: (message: string | object) => enhancedRenderer.sendMessage(message),
        debugGetHtml: () => enhancedRenderer.debugGetHtml(),
        getSystemInfo: () => enhancedRenderer.getSystemInfo(),
        isReady: () => enhancedRenderer.isReady(),
        getConnectionStatus: () => enhancedRenderer.getConnectionStatus(),
        onMessage: (callback: (data: any) => void) => {
            window.addEventListener('electron-message', (event: any) => {
                callback(event.detail);
            });
        },
        onConnectionStatusChange: (callback: (status: string) => void) => {
            window.addEventListener('connection-status-change', (event: any) => {
                callback(event.detail.status);
            });
        }
    };

    console.log('EnhancedPipeAPI 已初始化');
}

// 导出类型
export type { EnhancedRenderer };