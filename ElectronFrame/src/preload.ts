import { contextBridge, ipcRenderer } from 'electron';

// 定义 API 的类型
interface ElectronAPI {
    sendToPipe: (message: string) => Promise<boolean>;
    notifyReady: () => Promise<boolean>;
    onPipeMessage: (callback: (message: string) => void) => void;
    removeAllListeners: (channel: string) => void;
}

// 暴露安全的 API 给渲染进程
const electronAPI: ElectronAPI = {
    sendToPipe: (message: string): Promise<boolean> => {
        return ipcRenderer.invoke('send-to-pipe', message);
    },

    notifyReady: (): Promise<boolean> => {
        return ipcRenderer.invoke('renderer-ready');
    },

    onPipeMessage: (callback: (message: string) => void): void => {
        ipcRenderer.on('pipe-message', (event, message) => {
            callback(message);
        });
    },

    removeAllListeners: (channel: string): void => {
        ipcRenderer.removeAllListeners(channel);
    }
};

contextBridge.exposeInMainWorld('electronAPI', electronAPI);