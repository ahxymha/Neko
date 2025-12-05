// file: renderer_final.ts
interface Window {
    electronAPI: {
        sendToPipe: (message: string) => Promise<boolean>;
        notifyReady: () => Promise<boolean>;
        onPipeMessage: (callback: (message: string) => void) => void;
        debugGetHtml: () => Promise<string | null>;
        debugSetOutput: (message: string) => Promise<boolean>;
        debugStartListener: () => Promise<boolean>;
        debugStopListener: () => Promise<boolean>;
        reloadHtml: () => Promise<boolean>;
    };
}

class RendererApp {
    constructor() {
        this.initialize();
    }

    private async initialize(): Promise<void> {
        console.log('渲染进程初始化...');

        // 检查electronAPI是否存在
        if (!window.electronAPI) {
            console.error('Electron API不可用');
            setTimeout(() => this.initialize(), 500);
            return;
        }

        try {
            // 通知主进程渲染进程已就绪
            await window.electronAPI.notifyReady();

            // 设置消息监听器
            this.setupMessageListener();

            console.log('✅ 渲染进程初始化完成');

        } catch (error) {
            console.error('渲染进程初始化失败:', error);
        }
    }

    private setupMessageListener(): void {
        window.electronAPI.onPipeMessage((message: string) => {
            console.log('接收到主进程消息:', message);
            this.handleIncomingMessage(message);
        });
    }

    private handleIncomingMessage(message: string): void {
        try {
            const data = JSON.parse(message);

            // 根据消息类型处理
            switch (data.type) {
                case 'loading':
                    this.updateLoadingStatus(data.message);
                    break;
                default:
                    // 触发自定义事件
                    const event = new CustomEvent('pipe-data', {
                        detail: data
                    });
                    window.dispatchEvent(event);
            }
        } catch (error) {
            // 不是JSON，直接处理
            const event = new CustomEvent('pipe-data', {
                detail: { message }
            });
            window.dispatchEvent(event);
        }
    }

    private updateLoadingStatus(message: string): void {
        const statusElement = document.getElementById('loading-status');
        if (statusElement) {
            statusElement.textContent = message;
        }
    }

    // 公开方法
    public async sendMessage(message: string | object): Promise<boolean> {
        if (!window.electronAPI) {
            console.error('Electron API不可用');
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
}

// 初始化
document.addEventListener('DOMContentLoaded', () => {
    const app = new RendererApp();

    // 暴露给全局
    (window as any).PipeApp = {
        sendMessage: (message: string | object) => app.sendMessage(message)
    };

    console.log('PipeApp已初始化');
});