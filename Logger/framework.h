#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <Windows.h>
#include <rpc.h>
#include <string>

#ifdef LOGGER_EXPORTS
#define LOG_API __declspec(dllexport) 
#endif

extern"C" LOG_API void __stdcall Sendlog(short level, char* log, int len);
extern"C" LOG_API void __stdcall Stop();