// file: test_dll_correct.js
const koffi = require('koffi');
const path = require('path');
const fs = require('fs');

const dllPath = path.resolve('./webhelper.dll');
console.log('DLL路径:', dllPath);
console.log('DLL存在:', fs.existsSync(dllPath));

try {
    // 方法1: 使用不同方式加载
    console.log('\n=== 方法1: 使用koffi.load ===');
    const lib = koffi.load(dllPath);
    console.log('✅ DLL加载成功');

    // 列出所有符号
    console.log('\n=== 尝试列出所有符号 ===');
    console.log('lib对象类型:', typeof lib);
    console.log('lib对象属性:', Object.keys(lib));

    // 方法2: 尝试使用decorated names
    console.log('\n=== 方法2: 尝试使用装饰名 ===');

    // __stdcall函数在32位DLL中会添加装饰，但在64位中通常不会
    // 但在某些情况下，函数名可能有不同的装饰

    // 测试GetHtmlContent
    const functionNames = [
        'GetHtmlContent',
        '_GetHtmlContent@8',          // 32位stdcall装饰
        'GetHtmlContent@8',           // 另一种可能的装饰
        'GetHtmlContent',             // 原始名称
    ];

    for (const name of functionNames) {
        try {
            console.log(`\n尝试加载: ${name}`);
            const func = lib.func('bool', name, ['char *', 'uint']);
            console.log(`✅ ${name} 加载成功`);

            // 测试调用
            const buffer = Buffer.alloc(1024);
            console.log('调用函数...');
            const result = func(buffer, 1024);
            console.log(`调用结果: ${result}`);
            break;
        } catch (error) {
            console.log(`❌ ${name} 失败: ${error.message}`);
        }
    }

    // 方法3: 使用lib.stdcall
    console.log('\n=== 方法3: 使用lib.stdcall ===');
    try {
        const GetHtmlContent = lib.stdcall('GetHtmlContent', 'bool', ['char *', 'uint']);
        console.log('✅ lib.stdcall 加载成功');

        const buffer = Buffer.alloc(1024);
        const result = GetHtmlContent(buffer, 1024);
        console.log(`调用结果: ${result}`);
    } catch (error) {
        console.log(`❌ lib.stdcall 失败: ${error.message}`);
    }

    // 方法4: 使用原始函数调用
    console.log('\n=== 方法4: 直接调用 ===');
    try {
        // 直接调用lib上的函数
        if (typeof lib.GetHtmlContent === 'function') {
            console.log('✅ 直接调用lib.GetHtmlContent成功');
            const buffer = Buffer.alloc(1024);
            const result = lib.GetHtmlContent(buffer, 1024);
            console.log(`调用结果: ${result}`);
        } else {
            console.log('❌ lib.GetHtmlContent不是函数');
        }
    } catch (error) {
        console.log(`❌ 直接调用失败: ${error.message}`);
    }

    // 方法5: 使用koffi的通用函数加载
    console.log('\n=== 方法5: 使用koffi通用函数 ===');
    try {
        const GetHtmlContent = koffi.func(lib, 'bool GetHtmlContent(char *res, unsigned int len)');
        console.log('✅ koffi.func 加载成功');

        const buffer = Buffer.alloc(1024);
        const result = GetHtmlContent(buffer, 1024);
        console.log(`调用结果: ${result}`);
    } catch (error) {
        console.log(`❌ koffi.func 失败: ${error.message}`);
    }

} catch (error) {
    console.error('❌ 加载DLL失败:', error.message);
    console.error('完整错误:', error);
}