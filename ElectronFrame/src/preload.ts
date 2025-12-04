// file: preload.ts
import { contextBridge, ipcRenderer } from 'electron';

// 定义更全面的API类型
interface ElectronAPI {
    // 基础通信
    sendToPipe: (message: string) => Promise<boolean>;
    notifyReady: () => Promise<boolean>;
    onPipeMessage: (callback: (message: string) => void) => void;
    removeAllListeners: (channel: string) => void;

    // 调试和控制接口
    debugGetHtml: () => Promise<string | null>;
    debugSetOutput: (message: string) => Promise<boolean>;
    debugStartListener: () => Promise<boolean>;
    debugStopListener: () => Promise<boolean>;

    // 环境信息
    getEnvironmentInfo: () => Promise<{
        ELECTRON_HTML_PIPE?: string;
        ELECTRON_INPUT_PIPE?: string;
        ELECTRON_OUTPUT_PIPE?: string;
        dllPath?: string;
        bufferSize?: number;
    }>;

    // 系统状态
    getSystemStatus: () => Promise<{
        isDllLoaded: boolean;
        isListening: boolean;
        isReady: boolean;
        bufferSize: number;
    }>;
}

// 暴露安全的 API 给渲染进程
const electronAPI: ElectronAPI = {
    // 基础通信
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
    },

    // 调试和控制接口
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

    // 环境信息
    getEnvironmentInfo: async (): Promise<any> => {
        return ipcRenderer.invoke('get-environment-info');
    },

    // 系统状态
    getSystemStatus: async (): Promise<any> => {
        return ipcRenderer.invoke('get-system-status');
    }
};

// 暴露API给渲染进程
try {
    contextBridge.exposeInMainWorld('electronAPI', electronAPI);
    console.log('Electron API exposed successfully');
} catch (error) {
    console.error('Failed to expose Electron API:', error);
}

// 导出类型给TypeScript
export type { ElectronAPI };