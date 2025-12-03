const koffi = require('koffi');
try {
    const lib = koffi.load('webhelper.dll');
    console.log('DLL loaded, available functions:');
    // 尝试列出所有函数
    console.log('Library object:', lib);
} catch (error) {
    console.error('DLL load error:', error);
}