// file: preload_final.ts
import { contextBridge, ipcRenderer } from 'electron';

// 定义API类型
interface ElectronAPI {
    sendToPipe: (message: string) => Promise<boolean>;
    notifyReady: () => Promise<boolean>;
    onPipeMessage: (callback: (message: string) => void) => void;
    debugGetHtml: () => Promise<string | null>;
    debugSetOutput: (message: string) => Promise<boolean>;
    debugStartListener: () => Promise<boolean>;
    debugStopListener: () => Promise<boolean>;
    reloadHtml: () => Promise<boolean>;
}

// 暴露安全的API给渲染进程
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

    debugGetHtml: (): Promise<string | null> => {
        return ipcRenderer.invoke('debug-get-html');
    },

    debugSetOutput: (message: string): Promise<boolean> => {
        return ipcRenderer.invoke('debug-set-output', message);
    },

    debugStartListener: (): Promise<boolean> => {
        return ipcRenderer.invoke('debug-start-listener');
    },

    debugStopListener: (): Promise<boolean> => {
        return ipcRenderer.invoke('debug-stop-listener');
    },

    reloadHtml: (): Promise<boolean> => {
        return ipcRenderer.invoke('reload-html');
    }
};

try {
    contextBridge.exposeInMainWorld('electronAPI', electronAPI);
    console.log('Electron API暴露成功');
} catch (error) {
    console.error('暴露Electron API失败:', error);
}

// 导出类型
export type { ElectronAPI };