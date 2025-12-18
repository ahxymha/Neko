#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>

struct ElecDataT {

};

enum class PromissionLevel {
	USER=0,
	ADMIN=1,
	SYSTEM=2,
	TRUST=3,
};

struct Func
{
	bool isConfigFunc;
	bool isElecBack;
	PromissionLevel promis;
	LPWSTR funcName;

};

struct Plugin
{

};