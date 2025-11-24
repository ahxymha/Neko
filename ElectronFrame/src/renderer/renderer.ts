import { ElectronAPI } from '../main/preload';

class AppRenderer {
    private electronAPI: ElectronAPI;

    constructor() {
        this.electronAPI = window.electronAPI;
        this.initialize();
    }

    private async initialize(): Promise<void> {
        await this.loadAppVersion();
        this.setupEventListeners();
        this.setupMessageListener();
    }

    private async loadAppVersion(): Promise<void> {
        try {
            const version = await this.electronAPI.getAppVersion();
            const versionElement = document.getElementById('app-version');
            if (versionElement) {
                versionElement.textContent = `Version: ${version}`;
            }
        } catch (error) {
            console.error('Failed to load app version:', error);
        }
    }

    private setupEventListeners(): void {
        const sendButton = document.getElementById('send-message');
        const messageInput = document.getElementById('message-input') as HTMLInputElement;
        const messageList = document.getElementById('message-list');

        sendButton?.addEventListener('click', async () => {
            if (messageInput?.value) {
                try {
                    const response = await this.electronAPI.showMessage(messageInput.value);
                    this.addMessageToList(messageList, `Sent: ${messageInput.value} -> Response: ${response}`);
                    messageInput.value = '';
                } catch (error) {
                    console.error('Failed to send message:', error);
                }
            }
        });

        messageInput?.addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                sendButton?.click();
            }
        });
    }

    private setupMessageListener(): void {
        this.electronAPI.onMessageReply((message: string) => {
            const messageList = document.getElementById('message-list');
            this.addMessageToList(messageList, `Received: ${message}`);
        });
    }

    private addMessageToList(list: HTMLElement | null, message: string): void {
        if (!list) return;

        const messageItem = document.createElement('div');
        messageItem.className = 'message-item';
        messageItem.textContent = message;
        list.appendChild(messageItem);
        list.scrollTop = list.scrollHeight;
    }
}

// 当 DOM 加载完成时初始化应用
document.addEventListener('DOMContentLoaded', () => {
    new AppRenderer();
});