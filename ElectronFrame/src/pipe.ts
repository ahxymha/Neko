// file: pipe_callback.ts
import koffi from 'koffi';
import * as path from 'path';

// 回调函数类型定义
type InputCallback = (data: Buffer, length: number) => boolean;

class CallbackPipeManager {
    private dllPath: string;
    private lib: any;
    private bufferSize: number;
    private isDllLoaded: boolean = false;
    private callbackHandle: number = 0;
    private activeCallback: InputCallback | null = null;
    private isListening: boolean = false;

    // 默认缓冲区大小为50MB
    constructor(dllPath: string = 'webhelper.dll', bufferSize: number = 50 * 1024 * 1024) {
        this.dllPath = this.resolveDllPath(dllPath);
        this.bufferSize = bufferSize + 1; // 为null终止符额外分配1字节
        
        console.log(`CallbackPipeManager初始化，缓冲区大小: ${this.bufferSize / 1024 / 1024}MB`);
        console.log(`DLL路径: ${this.dllPath}`);
        
        try {
            this.lib = koffi.load(this.dllPath);
            this.isDllLoaded = true;
            console.log('✅ DLL加载成功');
        } catch (error) {
            console.error('❌ 加载DLL失败:', error);
            this.lib = null;
            this.isDllLoaded = false;
        }
    }

    private resolveDllPath(userPath: string): string {
        const pathModule = require('path');
        const fs = require('fs');
        
        // 尝试的路径列表
        const possiblePaths = [
            userPath,
            pathModule.resolve(userPath),
            pathModule.join(process.cwd(), userPath),
            pathModule.join(__dirname, userPath),
            pathModule.join(process.resourcesPath || process.cwd(), userPath),
            pathModule.join(process.resourcesPath || process.cwd(), 'app.asar.unpacked', userPath),
            pathModule.join(process.resourcesPath || process.cwd(), '..', userPath)
        ];
        
        for (const p of possiblePaths) {
            try {
                const fullPath = pathModule.resolve(p);
                if (fs.existsSync(fullPath)) {
                    console.log('找到DLL文件:', fullPath);
                    return fullPath;
                }
            } catch (e) {
                // 忽略错误
            }
        }
        
        console.warn(`未找到DLL文件，将使用路径: ${userPath}`);
        return userPath;
    }

    private getFunction(name: string): any {
        if (!this.isDllLoaded) {
            throw new Error('DLL未加载');
        }
        
        try {
            // 使用stdcall调用约定
            return this.lib.stdcall(name, 'bool', ['char *', 'uint']);
        } catch (error) {
            console.error(`加载函数 ${name} 失败:`, error);
            throw error;
        }
    }

    private getCallbackFunction(name: string, signature: string): any {
        if (!this.isDllLoaded) {
            throw new Error('DLL未加载');
        }
        
        try {
            // 使用stdcall调用约定
            return this.lib.stdcall(name, 'int', [signature]);
        } catch (error) {
            console.error(`加载回调函数 ${name} 失败:`, error);
            throw error;
        }
    }

    private getStopFunction(name: string): any {
        if (!this.isDllLoaded) {
            throw new Error('DLL未加载');
        }
        
        try {
            // 使用stdcall调用约定
            return this.lib.stdcall(name, 'bool', ['uintptr_t']);
        } catch (error) {
            console.error(`加载停止函数 ${name} 失败:`, error);
            throw error;
        }
    }

    /**
     * 获取HTML内容
     */
    public getHtmlContent(): string | null {
        if (!this.isDllLoaded) {
            console.error('DLL未加载，无法获取HTML内容');
            return null;
        }
        
        try {
            console.log(`调用GetHtmlContent，缓冲区大小: ${this.bufferSize}字节`);
            
            const buffer = Buffer.alloc(this.bufferSize, 0);
            const func = this.getFunction('GetHtmlContent');
            const success = func(buffer, this.bufferSize);
            
            console.log(`GetHtmlContent返回: ${success}`);
            
            if (!success) {
                console.error('GetHtmlContent调用失败');
                return null;
            }
            
            const nullIndex = buffer.indexOf(0);
            const content = nullIndex === -1 
                ? buffer.toString('utf8')
                : buffer.slice(0, nullIndex).toString('utf8');
            
            console.log(`获取到HTML内容，长度: ${content.length}字节`);
            
            return content;
        } catch (error) {
            console.error('获取HTML内容时出错:', error);
            return null;
        }
    }

    /**
     * 设置输入内容回调
     * @param callback 回调函数，当接收到输入数据时调用
     * @returns 是否成功
     */
    public setInputContentCallback(callback: (data: string) => void): boolean {
        if (!this.isDllLoaded) {
            console.error('DLL未加载，无法设置回调');
            return false;
        }
        
        if (this.isListening) {
            console.warn('已经在监听中，先停止当前监听');
            this.stopInputListener();
        }
        
        try {
            console.log('设置输入内容回调...');
            
            // 创建Koffi回调函数
            const koffiCallback = koffi.callback('bool', ['char *', 'int'], (dataPtr: Buffer, length: number) => {
                try {
                    // 将缓冲区数据转换为字符串
                    const data = this.bufferToString(dataPtr, length);
                    console.log(`回调接收到数据，长度: ${data.length}字节`);
                    
                    // 调用用户回调
                    callback(data);
                    return true;
                } catch (error) {
                    console.error('回调函数执行错误:', error);
                    return false;
                }
            });
            
            // 获取SetInputContentCallback函数
            const setCallbackFunc = this.getCallbackFunction('SetInputContentCallback', 'bool (*)(char*, int)');
            
            // 调用DLL设置回调
            this.callbackHandle = setCallbackFunc(koffiCallback);
            
            if (this.callbackHandle !== 0) {
                this.activeCallback = koffiCallback;
                this.isListening = true;
                console.log(`✅ 回调设置成功，句柄: ${this.callbackHandle}`);
                return true;
            } else {
                console.error('设置回调失败，返回句柄为0');
                return false;
            }
        } catch (error) {
            console.error('设置回调时出错:', error);
            return false;
        }
    }

    /**
     * 停止输入监听
     * @returns 是否成功
     */
    public stopInputListener(): boolean {
        if (!this.isDllLoaded || !this.isListening) {
            console.warn('未在监听状态或DLL未加载');
            return false;
        }
        
        try {
            console.log(`停止输入监听，句柄: ${this.callbackHandle}`);
            
            const stopFunc = this.getStopFunction('StopInputListener');
            const success = stopFunc(this.callbackHandle);
            
            if (success) {
                this.isListening = false;
                this.callbackHandle = 0;
                this.activeCallback = null;
                console.log('✅ 输入监听已停止');
            } else {
                console.error('停止输入监听失败');
            }
            
            return success;
        } catch (error) {
            console.error('停止输入监听时出错:', error);
            return false;
        }
    }

    /**
     * 设置输出内容
     */
    public setOutputContent(content: string): boolean {
        if (!this.isDllLoaded) {
            console.error('DLL未加载，无法设置输出内容');
            return false;
        }
        
        try {
            console.log(`调用SetOutputContent，内容长度: ${content.length}字节`);
            
            const maxLength = this.bufferSize - 1;
            const safeContent = content.length > maxLength 
                ? content.substring(0, maxLength)
                : content;
            
            const buffer = Buffer.from(safeContent, 'utf8');
            const func = this.getFunction('SetOutputContent');
            const success = func(buffer, buffer.length);
            
            console.log(`SetOutputContent返回: ${success}`);
            
            return success;
        } catch (error) {
            console.error('设置输出内容时出错:', error);
            return false;
        }
    }

    /**
     * 等待输入内容（阻塞调用）
     */
    public waitForInput(): string | null {
        if (!this.isDllLoaded) {
            console.error('DLL未加载，无法等待输入');
            return null;
        }
        
        try {
            console.log(`调用WaitInputContent，缓冲区大小: ${this.bufferSize}字节`);
            
            const buffer = Buffer.alloc(this.bufferSize, 0);
            const func = this.getFunction('WaitInputContent');
            const success = func(buffer, this.bufferSize);
            
            console.log(`WaitInputContent返回: ${success}`);
            
            if (!success) {
                console.error('WaitInputContent调用失败');
                return null;
            }
            
            const nullIndex = buffer.indexOf(0);
            const content = nullIndex === -1 
                ? buffer.toString('utf8')
                : buffer.slice(0, nullIndex).toString('utf8');
            
            console.log(`获取到输入内容，长度: ${content.length}字节`);
            
            return content;
        } catch (error) {
            console.error('等待输入时出错:', error);
            return null;
        }
    }

    /**
     * 将缓冲区转换为字符串
     */
    private bufferToString(buffer: Buffer, length: number): string {
        // 确保长度不超过缓冲区大小
        const safeLength = Math.min(length, buffer.length);
        
        // 查找null终止符
        const nullIndex = buffer.indexOf(0);
        const finalLength = nullIndex === -1 ? safeLength : Math.min(nullIndex, safeLength);
        
        return buffer.slice(0, finalLength).toString('utf8');
    }

    /**
     * 检查是否正在监听
     */
    public isListeningActive(): boolean {
        return this.isListening;
    }

    /**
     * 检查DLL是否就绪
     */
    public isReady(): boolean {
        return this.isDllLoaded;
    }

    /**
     * 清理资源
     */
    public dispose(): void {
        console.log('清理CallbackPipeManager资源...');
        
        // 停止监听
        if (this.isListening) {
            this.stopInputListener();
        }
        
        // 注意：koffi回调函数不需要手动清理，但我们可以清空引用
        this.activeCallback = null;
        this.lib = null;
        
        console.log('✅ 资源清理完成');
    }
}

export default CallbackPipeManager;