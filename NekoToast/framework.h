#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
#ifdef NEKOTOAST_EXPORTS
#define NEKOTOAST_API __declspec(dllexport)
#else
#define NEKOTOAST_API 
#endif
// Windows 头文件
#include <windows.h>


extern"C" {
	NEKOTOAST_API BOOL APIENTRY PlgEntry();
}