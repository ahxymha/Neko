#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
#ifdef WEBHELPER_EXPORTS
#define WEBHELPER_API __declspec(dllexport) 
#else
#define WEBHELPER_API __declspec(dllimport)
#endif

extern"C" WEBHELPER_API bool __stdcall GetHtmlContent(char* res, const unsigned int len);
extern"C" WEBHELPER_API bool __stdcall WaitInputContent(char* res, const unsigned int len);
extern"C" WEBHELPER_API bool __stdcall SetOutputContent(char* str, const unsigned int len);
