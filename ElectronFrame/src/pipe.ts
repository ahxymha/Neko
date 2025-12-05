// file: pipe_final.ts
import koffi from 'koffi';

class PipeManager {
    private dllPath: string;
    private lib: any;
    private bufferSize: number;
    private isDllLoaded: boolean = false;
    private callbackHandle: number = 0;

    constructor(dllPath: string = 'webhelper.dll', bufferSize: number = 50 * 1024 * 1024) {
        this.dllPath = dllPath;
        this.bufferSize = bufferSize + 1;

        console.log(`PipeManager初始化，缓冲区大小: ${this.bufferSize / 1024 / 1024}MB`);

        try {
            this.lib = koffi.load(this.dllPath);
            this.isDllLoaded = true;
            console.log('✅ DLL加载成功');
        } catch (error) {
            console.error('❌ 加载DLL失败:', error);
            this.lib = null;
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
            console.log('调用GetHtmlContent...');

            const buffer = Buffer.alloc(this.bufferSize, 0);

            // 使用lib.stdcall加载函数（已废弃但在Koffi 2.7中仍然可用）
            const GetHtmlContent = this.lib.stdcall('GetHtmlContent', 'bool', ['char *', 'uint']);
            const success = GetHtmlContent(buffer, this.bufferSize);

            console.log(`GetHtmlContent返回: ${success}`);

            if (!success) {
                console.error('GetHtmlContent调用失败（可能环境变量未设置）');
                return null;
            }

            const nullIndex = buffer.indexOf(0);
            const content = nullIndex === -1
                ? buffer.toString('utf8')
                : buffer.slice(0, nullIndex).toString('utf8');

            console.log(`✅ 获取HTML内容成功，长度: ${content.length}字节`);
            return content;
        } catch (error) {
            console.error('获取HTML内容时出错:', error);
            return null;
        }
    }

    /**
     * 设置输入回调
     */
    public setInputContentCallback(callback: (data: string) => void): boolean {
        if (!this.isDllLoaded) {
            console.error('DLL未加载，无法设置回调');
            return false;
        }

        try {
            console.log('设置输入回调...');

            // 创建JavaScript回调函数
            const jsCallback = (dataPtr: Buffer, length: number): boolean => {
                try {
                    // 安全地读取数据
                    const safeLength = Math.min(length, dataPtr.length);
                    const nullIndex = dataPtr.indexOf(0);
                    const finalLength = nullIndex === -1 ? safeLength : Math.min(nullIndex, safeLength);

                    const data = dataPtr.slice(0, finalLength).toString('utf8');
                    console.log(`📥 回调接收到数据: ${data.length}字节`);

                    // 调用用户回调
                    callback(data);
                    return true;
                } catch (error) {
                    console.error('回调处理错误:', error);
                    return false;
                }
            };

            // 使用你提供的回调函数设置方法
            const koCallBackFunc = koffi.proto('bool __stdcall (char *res,int length)');
            const koffiCallback = koffi.register(jsCallback, koffi.pointer(koCallBackFunc));

            // 加载SetInputContentCallback函数
            const SetInputContentCallback = this.lib.stdcall('SetInputContentCallback', 'int', [koffi.pointer(koCallBackFunc)]);

            // 调用DLL设置回调
            this.callbackHandle = SetInputContentCallback(koffiCallback);

            console.log(`SetInputContentCallback返回句柄: ${this.callbackHandle}`);

            if (this.callbackHandle !== 0) {
                console.log('✅ 回调设置成功');
                return true;
            } else {
                console.error('❌ 回调设置失败，返回句柄为0');
                return false;
            }
        } catch (error) {
            console.error('设置回调时出错:', error);
            return false;
        }
    }

    /**
     * 停止输入监听
     */
    public stopInputListener(): boolean {
        if (!this.isDllLoaded || this.callbackHandle === 0) {
            console.warn('未在监听状态或DLL未加载');
            return false;
        }

        try {
            console.log(`停止输入监听，句柄: ${this.callbackHandle}`);

            const StopInputListener = this.lib.stdcall('StopInputListener', 'bool', ['uintptr_t']);
            const success = StopInputListener(this.callbackHandle);

            if (success) {
                this.callbackHandle = 0;
                console.log('✅ 输入监听已停止');
            } else {
                console.error('❌ 停止输入监听失败');
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
            console.log(`发送输出内容: ${content.length}字节`);

            const buffer = Buffer.from(content, 'utf8');
            const SetOutputContent = this.lib.stdcall('SetOutputContent', 'bool', ['char *', 'uint']);
            const success = SetOutputContent(buffer, buffer.length);

            console.log(`SetOutputContent返回: ${success}`);
            return success;
        } catch (error) {
            console.error('设置输出内容时出错:', error);
            return false;
        }
    }

    /**
     * 检查是否就绪
     */
    public isReady(): boolean {
        return this.isDllLoaded;
    }
}

export default PipeManager;