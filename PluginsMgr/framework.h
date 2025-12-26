#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
#include <string>
#include <vector>

namespace PluginsMgr{
	constexpr uint16_t itemNumOfPlgManifest = 2;
	using PlgManifest = std::vector<std::pair<std::string, std::string>>;
	constexpr char ManifestItemName[itemNumOfPlgManifest][6] = { "Name","Entry" };
	typedef char* (*PGetManifest)();
}

namespace NekoPlugin {
	using PlgMgrCallback = BOOL(*)(uint8_t,void*);
	BOOL CallForAction(uint8_t ActionType, void* pIN);
}

BOOL LoadPlugins(std::string floader);