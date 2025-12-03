// file: pipe.ts
import koffi from 'koffi';

// 定义DLL中的函数签名
interface PipeFunctions {
    GetHtmlContent: (buffer: Buffer, length: number) => boolean;
    WaitInputContent: (buffer: Buffer, length: number) => boolean;
    SetOutputContent: (buffer: Buffer, length: number) => boolean;
}

class PipeManager {
    private dllPath: string;
    private lib: any;
    private pipeFunctions: PipeFunctions;
    private inputBuffer: Buffer;
    private outputBuffer: Buffer;
    private htmlBuffer: Buffer;
    private bufferSize: number;
    private stopInputListener: (() => void) | null = null;

    constructor(dllPath: string = 'webhelper.dll', bufferSize: number = 65536) {
        this.dllPath = dllPath;
        this.bufferSize = bufferSize;

        // 创建缓冲区
        this.inputBuffer = Buffer.alloc(this.bufferSize);
        this.outputBuffer = Buffer.alloc(this.bufferSize);
        this.htmlBuffer = Buffer.alloc(this.bufferSize);

        // 加载DLL和函数
        this.lib = this.loadLibrary();
        this.pipeFunctions = this.loadFunctions();
    }

    private loadLibrary(): any {
        try {
            console.log(`Loading DLL from: ${this.dllPath}`);
            const lib = koffi.load(this.dllPath);
            console.log('DLL loaded successfully');
            return lib;
        } catch (error) {
            console.error(`Failed to load DLL: ${error}`);
            throw new Error(`Unable to load DLL: ${this.dllPath}`);
        }
    }

    private loadFunctions(): PipeFunctions {
        try {
            console.log('Loading DLL functions...');

            // 使用已弃用但可用的stdcall方法
            const GetHtmlContent = this.lib.stdcall('GetHtmlContent', 'bool', ['char *', 'uint']);
            const WaitInputContent = this.lib.stdcall('WaitInputContent', 'bool', ['char *', 'uint']);
            const SetOutputContent = this.lib.stdcall('SetOutputContent', 'bool', ['char *', 'uint']);

            console.log('Functions loaded successfully with stdcall');

            return {
                GetHtmlContent,
                WaitInputContent,
                SetOutputContent
            };
        } catch (error) {
            console.error(`Failed to load functions: ${error}`);

            // 备选方法
            console.log('Trying alternative function loading method...');

            return {
                GetHtmlContent: (buffer: Buffer, length: number) => {
                    console.log('Fallback: Calling GetHtmlContent');
                    try {
                        const func = this.lib.func('bool', 'GetHtmlContent', ['char *', 'uint']);
                        return func(buffer, length);
                    } catch (e: any) {
                        console.error('Fallback error:', e);
                        return false;
                    }
                },
                WaitInputContent: (buffer: Buffer, length: number) => {
                    console.log('Fallback: Calling WaitInputContent');
                    try {
                        const func = this.lib.func('bool', 'WaitInputContent', ['char *', 'uint']);
                        return func(buffer, length);
                    } catch (e: any) {
                        console.error('Fallback error:', e);
                        return false;
                    }
                },
                SetOutputContent: (buffer: Buffer, length: number) => {
                    console.log('Fallback: Calling SetOutputContent');
                    try {
                        const func = this.lib.func('bool', 'SetOutputContent', ['char *', 'uint']);
                        return func(buffer, length);
                    } catch (e: any) {
                        console.error('Fallback error:', e);
                        return false;
                    }
                }
            };
        }
    }

    /**
     * 获取HTML内容
     * @returns HTML内容字符串，如果失败则返回null
     */
    public getHtmlContent(): string | null {
        try {
            console.log('Calling GetHtmlContent...');

            // 清空缓冲区
            this.htmlBuffer.fill(0);

            // 调用DLL函数
            const success = this.pipeFunctions.GetHtmlContent(this.htmlBuffer, this.bufferSize);

            console.log(`GetHtmlContent returned: ${success}`);

            if (!success) {
                console.error('GetHtmlContent failed');
                return null;
            }

            // 将缓冲区转换为字符串（以null结尾）
            const content = this.readNullTerminatedString(this.htmlBuffer);
            console.log(`Got HTML content, length: ${content.length}`);

            return content;
        } catch (error) {
            console.error(`Error in getHtmlContent: ${error}`);
            return null;
        }
    }

    /**
     * 等待输入内容（阻塞调用）
     * @returns 接收到的消息字符串，如果失败则返回null
     */
    public waitForInput(): string | null {
        try {
            console.log('Waiting for input content...');

            // 清空缓冲区
            this.inputBuffer.fill(0);

            // 调用DLL函数（阻塞直到有数据）
            const success = this.pipeFunctions.WaitInputContent(this.inputBuffer, this.bufferSize);

            console.log(`WaitInputContent returned: ${success}`);

            if (!success) {
                console.error('WaitInputContent failed');
                return null;
            }

            // 将缓冲区转换为字符串（以null结尾）
            const message = this.readNullTerminatedString(this.inputBuffer);
            console.log(`Received message from input: ${message.length} characters`);

            return message;
        } catch (error) {
            console.error(`Error in waitForInput: ${error}`);
            return null;
        }
    }

    /**
     * 异步等待输入内容（非阻塞）
     * @param callback 接收到消息时的回调函数
     * @param interval 轮询间隔（毫秒），默认为100ms
     * @returns 返回一个清理函数，用于停止监听
     */
    public waitForInputAsync(callback: (message: string | null) => void, interval: number = 100): () => void {
        console.log('Starting async input listener');

        let isRunning = true;

        const checkInput = () => {
            if (!isRunning) return;

            try {
                // 直接调用（在Electron中，我们有Node.js环境，不需要Worker）
                const message = this.waitForInput();

                if (message !== null) {
                    callback(message);
                }
            } catch (error) {
                console.error(`Error in async input: ${error}`);
                callback(null);
            }

            // 继续监听
            if (isRunning) {
                setTimeout(checkInput, interval);
            }
        };

        // 开始监听
        checkInput();

        // 返回停止函数
        const stopFunction = () => {
            console.log('Stopping async input listener');
            isRunning = false;
        };

        this.stopInputListener = stopFunction;
        return stopFunction;
    }

    /**
     * 设置输出内容
     * @param content 要发送的内容
     * @returns 是否成功
     */
    public setOutputContent(content: string): boolean {
        try {
            console.log(`Setting output content: ${content.length} characters`);

            // 检查内容长度是否超过缓冲区大小
            if (content.length >= this.bufferSize) {
                console.warn(`Content too long (${content.length} chars), truncating to ${this.bufferSize - 1} chars`);
            }

            // 准备输出缓冲区
            this.outputBuffer.fill(0);

            // 将字符串写入缓冲区
            const bytesWritten = this.outputBuffer.write(content, 0, Math.min(content.length, this.bufferSize - 1), 'utf8');

            console.log(`Writing ${bytesWritten} bytes to output buffer`);

            // 调用DLL函数 - 传递Buffer和长度
            const success = this.pipeFunctions.SetOutputContent(this.outputBuffer, bytesWritten);

            console.log(`SetOutputContent returned: ${success}`);

            if (success) {
                console.log('Output content sent successfully');
            } else {
                console.error('SetOutputContent failed');
            }

            return success;
        } catch (error) {
            console.error(`Error in setOutputContent: ${error}`);
            return false;
        }
    }

    /**
     * 读取以null结尾的字符串
     */
    private readNullTerminatedString(buffer: Buffer): string {
        // 找到第一个null字节
        let nullIndex = buffer.indexOf(0);
        if (nullIndex === -1) {
            nullIndex = buffer.length;
        }

        // 提取有效部分
        const validBuffer = buffer.slice(0, nullIndex);
        return validBuffer.toString('utf8');
    }

    /**
     * 释放资源
     */
    public dispose(): void {
        console.log('Disposing PipeManager resources');

        // 停止输入监听
        if (this.stopInputListener) {
            this.stopInputListener();
            this.stopInputListener = null;
        }

        // 注意：koffi目前没有显式的卸载方法
        // 在实际应用中，可能需要考虑内存管理
    }


    /**
     * 获取环境变量信息
     */
    public getEnvironmentInfo(): any {
        return {
            ELECTRON_HTML_PIPE: process.env.ELECTRON_HTML_PIPE,
            ELECTRON_INPUT_PIPE: process.env.ELECTRON_INPUT_PIPE,
            ELECTRON_OUTPUT_PIPE: process.env.ELECTRON_OUTPUT_PIPE,
            dllPath: this.dllPath,
            bufferSize: this.bufferSize
        };
    }
}

// 导出类
export default PipeManager;

// 导出工厂函数
export function createPipeManager(dllPath?: string, bufferSize?: number): PipeManager {
    return new PipeManager(dllPath, bufferSize);
}