#pragma once

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
// Windows 头文件
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <utility>

namespace PluginsMgr{
	namespace _nkp {
		struct _Header {
			_Header() = default;
			uint8_t flag[4];
			uint8_t nkPVer;
			uint8_t key[32];
			uint8_t iv[16];
			uint8_t hash[32];
			uint8_t mainVer, patchVer, BuildVer;
			uint32_t l_manifest, l_PE;
		};

		struct _Plg {
			struct PlgData{
				uint32_t nameLen;
				uint32_t entryLen;
				uint32_t descriptionLen;
				uint8_t isEnableOnStartup;
				uint8_t reserved[3] = {};
			}plgPODdata;
			std::string name, entry, description;
		};

		struct _Pvd {
			struct PvdData {
				uint32_t callLen;
				uint32_t entryLen;
			}pvdPODdata;
			std::string callName, entryName;
		};

		struct _Manifest {
			struct ManifestData {
				uint32_t nameLen;
				uint32_t plgNum;
				uint32_t pvdNum;
				uint32_t reserved = 0;
				uint64_t plgLen;
				uint64_t pvdLen;
				uint8_t key[32];
				uint8_t iv[16];
				uint8_t hash[32];
			}manifestPODdata;
			std::string Name;
			std::vector<_Plg> plgs;
			std::vector<_Pvd> pvds;
		};

	}
	using PluginContent = std::pair<_nkp::_Header, std::pair<_nkp::_Manifest,std::vector<unsigned char>>>;
	PluginContent AnalysisPlugin(std::filesystem::path nkpPath);
}

namespace NekoPlugin {
	using PlgMgrCallback = BOOL(*)(uint8_t,void*);
	BOOL CallForAction(uint8_t ActionType, void* pIN);
}

BOOL LoadPlugins(std::string floader);