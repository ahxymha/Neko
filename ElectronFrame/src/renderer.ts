// 在文件顶部添加类型声明
interface ElectronAPI {
    sendToPipe: (message: string) => Promise<boolean>;
    notifyReady: () => Promise<boolean>;
    onPipeMessage: (callback: (message: string) => void) => void;
    removeAllListeners: (channel: string) => void;
}

// 扩展 Window 接口
declare global {
    interface Window {
        electronAPI: ElectronAPI;
        PipeAPI: {
            sendMessage: (message: string | object) => Promise<boolean>;
            isReady: () => boolean;
            onMessage: (callback: (data: any) => void) => void;
        };
        onPipeMessage?: (data: any) => void;
    }
}

class RendererPipeHandler {
    private isInitialized: boolean = false;

    constructor() {
        this.initialize();
    }

    private async initialize(): Promise<void> {
        // 检查 electronAPI 是否存在
        if (!window.electronAPI) {
            console.error('Electron API not available');
            // 等待一段时间再重试，因为 preload 脚本可能还在加载
            setTimeout(() => this.initialize(), 100);
            return;
        }

        try {
            // 通知主进程渲染进程已就绪
            await window.electronAPI.notifyReady();

            // 设置消息监听器
            this.setupMessageListener();

            this.isInitialized = true;
            console.log('Renderer pipe handler initialized');
        } catch (error) {
            console.error('Failed to initialize renderer pipe handler:', error);
        }
    }

    private setupMessageListener(): void {
        window.electronAPI.onPipeMessage((message: string) => {
            console.log('Received message in renderer:', message);
            this.handleIncomingMessage(message);
        });
    }

    private handleIncomingMessage(message: string): void {
        try {
            // 解析消息（假设是 JSON 格式）
            const data = JSON.parse(message);

            // 触发自定义事件，让网页可以监听
            const event = new CustomEvent('pipe-data-received', {
                detail: data
            });
            window.dispatchEvent(event);

            // 也可以直接调用全局函数（如果存在）
            if (typeof window.onPipeMessage === 'function') {
                window.onPipeMessage(data);
            }
        } catch (error) {
            // 如果不是 JSON，直接作为字符串处理
            const event = new CustomEvent('pipe-data-received', {
                detail: message
            });
            window.dispatchEvent(event);

            if (typeof window.onPipeMessage === 'function') {
                window.onPipeMessage(message);
            }
        }
    }

    // 公共方法：发送消息到主进程（输出管道）
    public async sendMessage(message: string | object): Promise<boolean> {
        if (!this.isInitialized) {
            console.error('Renderer pipe handler not initialized');
            return false;
        }

        try {
            const messageStr = typeof message === 'string' ? message : JSON.stringify(message);
            return await window.electronAPI.sendToPipe(messageStr);
        } catch (error) {
            console.error('Error sending message:', error);
            return false;
        }
    }

    // 检查是否已初始化
    public isReady(): boolean {
        return this.isInitialized;
    }
}

// 等待 DOM 加载完成后初始化
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        initializeApp();
    });
} else {
    initializeApp();
}

function initializeApp(): void {
    // 创建全局实例
    const pipeHandler = new RendererPipeHandler();

    // 暴露给网页的全局 API
    window.PipeAPI = {
        sendMessage: (message: string | object) => pipeHandler.sendMessage(message),
        isReady: () => pipeHandler.isReady(),
        onMessage: (callback: (data: any) => void) => {
            window.addEventListener('pipe-data-received', (event: any) => {
                callback(event.detail);
            });
        }
    };

    console.log('PipeAPI initialized');
}
export { };