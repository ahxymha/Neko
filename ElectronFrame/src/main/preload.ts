import { contextBridge, ipcRenderer } from 'electron';

// 暴露受保护的 API，使渲染进程能够使用
contextBridge.exposeInMainWorld('electronAPI', {
    getAppVersion: () => ipcRenderer.invoke('get-app-version'),
    showMessage: (message: string) => ipcRenderer.invoke('show-message', message),
    onMessageReply: (callback: (message: string) => void) =>
        ipcRenderer.on('message-reply', (event, message) => callback(message))
});

// 类型定义
export interface ElectronAPI {
    getAppVersion: () => Promise<string>;
    showMessage: (message: string) => Promise<string>;
    onMessageReply: (callback: (message: string) => void) => void;
}

declare global {
    interface Window {
        electronAPI: ElectronAPI;
    }
}