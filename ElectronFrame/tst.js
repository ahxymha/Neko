"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
// file: test_stdcall.ts
const koffi_1 = __importDefault(require("koffi"));
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
async function testStdCallDLL() {
    const dllPath = path.resolve('./webhelper.dll');
    console.log(`Testing DLL with __stdcall: ${dllPath}`);
    console.log(`DLL exists: ${fs.existsSync(dllPath)}`);
    try {
        // 加载DLL
        const lib = koffi_1.default.load(dllPath);
        console.log('✓ DLL loaded');
        console.log('Library methods:', Object.keys(lib));
        // 测试不同的调用约定
        console.log('\n=== 测试不同的调用约定 ===');
        // 1. 尝试 __stdcall
        console.log('\n1. 使用 __stdcall:');
        try {
            const GetHtmlContent = lib.func('__stdcall bool GetHtmlContent(char *res, unsigned int len)');
            console.log('✓ __stdcall 函数定义成功');
            const buffer = Buffer.alloc(1024 * 1024); // 1MB buffer
            console.log('调用 GetHtmlContent...');
            const result = GetHtmlContent(buffer, buffer.length);
            console.log(`✓ GetHtmlContent 调用成功, 结果: ${result}`);
            if (result) {
                const nullIndex = buffer.indexOf(0);
                const content = nullIndex === -1 ? buffer.toString('utf8') : buffer.slice(0, nullIndex).toString('utf8');
                console.log(`✓ 获取到内容, 长度: ${content.length}`);
            }
        }
        catch (error) {
            console.error(`✗ __stdcall 失败: ${error}`);
        }
        // 2. 尝试使用 lib.stdcall
        console.log('\n2. 使用 lib.stdcall:');
        try {
            const GetHtmlContent = lib.stdcall('GetHtmlContent', 'bool', ['char *', 'uint']);
            console.log('✓ lib.stdcall 函数定义成功');
            const buffer = Buffer.alloc(1024);
            console.log('调用 GetHtmlContent...');
            const result = GetHtmlContent(buffer, buffer.length);
            console.log(`✓ GetHtmlContent 调用成功, 结果: ${result}`);
        }
        catch (error) {
            console.error(`✗ lib.stdcall 失败: ${error}`);
        }
        // 3. 尝试直接调用（无调用约定）
        console.log('\n3. 无调用约定:');
        try {
            const GetHtmlContent = lib.func('bool GetHtmlContent(char *res, unsigned int len)');
            console.log('✓ 函数定义成功（无调用约定）');
            const buffer = Buffer.alloc(1024);
            console.log('调用 GetHtmlContent...');
            const result = GetHtmlContent(buffer, buffer.length);
            console.log(`✓ GetHtmlContent 调用成功, 结果: ${result}`);
        }
        catch (error) {
            console.error(`✗ 无调用约定失败: ${error}`);
        }
        // 4. 尝试使用 WINAPI（通常是 __stdcall）
        console.log('\n4. 使用 WINAPI:');
        try {
            const GetHtmlContent = lib.func('WINAPI bool GetHtmlContent(char *res, unsigned int len)');
            console.log('✓ WINAPI 函数定义成功');
            const buffer = Buffer.alloc(1024);
            console.log('调用 GetHtmlContent...');
            const result = GetHtmlContent(buffer, buffer.length);
            console.log(`✓ GetHtmlContent 调用成功, 结果: ${result}`);
        }
        catch (error) {
            console.error(`✗ WINAPI 失败: ${error}`);
        }
        // 测试 SetOutputContent
        console.log('\n=== 测试 SetOutputContent ===');
        try {
            const SetOutputContent = lib.func('__stdcall bool SetOutputContent(char *str, unsigned int len)');
            console.log('✓ SetOutputContent 函数定义成功');
            const testString = 'Hello from Node.js!';
            const buffer = Buffer.from(testString, 'utf8');
            const result = SetOutputContent(buffer, buffer.length);
            console.log(`✓ SetOutputContent 调用成功, 结果: ${result}`);
        }
        catch (error) {
            console.error(`✗ SetOutputContent 失败: ${error}`);
        }
        // 测试 WaitInputContent
        console.log('\n=== 测试 WaitInputContent ===');
        try {
            const WaitInputContent = lib.func('__stdcall bool WaitInputContent(char *res, unsigned int len)');
            console.log('✓ WaitInputContent 函数定义成功');
            const buffer = Buffer.alloc(1024);
            console.log('调用 WaitInputContent（可能阻塞）...');
            const result = WaitInputContent(buffer, buffer.length);
            console.log(`✓ WaitInputContent 调用成功, 结果: ${result}`);
            if (result) {
                const nullIndex = buffer.indexOf(0);
                const content = nullIndex === -1 ? buffer.toString('utf8') : buffer.slice(0, nullIndex).toString('utf8');
                console.log(`✓ 接收到内容: ${content.substring(0, 100)}...`);
            }
        }
        catch (error) {
            console.error(`✗ WaitInputContent 失败: ${error}`);
        }
    }
    catch (error) {
        console.error(`✗ 加载DLL失败: ${error}`);
    }
}
// 运行测试
testStdCallDLL();
