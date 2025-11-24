import { app, BrowserWindow, ipcMain, Menu } from 'electron';
import * as path from 'path';

// 保持对窗口对象的全局引用
let mainWindow: BrowserWindow | null = null;

function createWindow(): void {
    // 创建浏览器窗口
    mainWindow = new BrowserWindow({
        width: 1200,
        height: 800,
        webPreferences: {
            nodeIntegration: false,
            contextIsolation: true,
            preload: path.join(__dirname, 'preload.js')
        },
        icon: path.join(__dirname, '../assets/icon.png')
    });

    // 加载应用的 index.html
    mainWindow.loadFile(path.join(__dirname, '../renderer/index.html'));

    // 开发时打开开发者工具
    if (process.env.NODE_ENV === 'development') {
        mainWindow.webContents.openDevTools();
    }

    // 当窗口被关闭时发出
    mainWindow.on('closed', () => {
        mainWindow = null;
    });

    // 创建菜单
    createMenu();
}

function createMenu(): void {
    const template = [
        {
            label: '文件',
            submenu: [
                {
                    label: '新建',
                    accelerator: 'CmdOrCtrl+N',
                    click: () => {
                        // 新建文件逻辑
                    }
                },
                { type: 'separator' },
                {
                    label: '退出',
                    accelerator: process.platform === 'darwin' ? 'Cmd+Q' : 'Ctrl+Q',
                    click: () => {
                        app.quit();
                    }
                }
            ]
        },
        {
            label: '编辑',
            submenu: [
                { role: 'undo', label: '撤销' },
                { role: 'redo', label: '重做' },
                { type: 'separator' },
                { role: 'cut', label: '剪切' },
                { role: 'copy', label: '复制' },
                { role: 'paste', label: '粘贴' }
            ]
        }
    ];

    const menu = Menu.buildFromTemplate(template as any);
    Menu.setApplicationMenu(menu);
}

// Electron 完成初始化并准备创建浏览器窗口时调用
app.whenReady().then(createWindow);

// 当所有窗口都关闭时退出应用
app.on('window-all-closed', () => {
    // 在 macOS 上，应用和菜单栏通常保持活动状态，直到用户使用 Cmd + Q 明确退出
    if (process.platform !== 'darwin') {
        app.quit();
    }
});

app.on('activate', () => {
    // 在 macOS 上，当单击停靠图标且没有其他窗口打开时，
    // 通常会在应用中重新创建一个窗口
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow();
    }
});

// IPC 处理程序
ipcMain.handle('get-app-version', () => {
    return app.getVersion();
});

ipcMain.handle('show-message', (event, message: string) => {
    if (mainWindow) {
        mainWindow.webContents.send('message-reply', `Server received: ${message}`);
    }
    return `Message "${message}" processed`;
});