#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
#include <string>

namespace PluginsMgr{
	const uint16_t itemNumOfPlgManifest = 2;
	struct PlgManifest {
		std::string Name;
		std::string Entry;
	};

	typedef char* (*PGetManifest)();
}

BOOL LoadPlugins(std::string floader);