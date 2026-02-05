#pragma once
// Windows 头文件
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <utility>
#include <cstdint>

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
	namespace PluginLoader {
		using  PluginEntry = BOOL(*)(HANDLE hostMutex, uint64_t& updateFlag, uint64_t& contentLen, uint8_t* content, HANDLE Stop);
		struct plgHost{
			HANDLE MutexLocker;
			uint64_t UpdateFlag;
			uint64_t ContentLen;
			uint8_t* Content;
		};
	}
	namespace CSCommunication {
		enum ResponseType {
			CallBack,
			Status,
			Data,
			Internal
		};
		enum ResponseStatus {
			Ok,
			More,
			Wait,
			Error
		};
#pragma pack(push, 2)
		struct ResponseBody {
			ResponseType type;
			ResponseStatus status;
			__int32 sessionId;
			__int64 datalen;
			unsigned char* data;
		};
#pragma pack(pop)
		namespace Reciver {
			struct ResponseBody {
				ResponseType type;
				ResponseStatus status;
				__int32 sessionId;
				__int64 datalen;
				std::vector<unsigned char> data;
			};
		}
	}
	using PluginContent = std::pair<_nkp::_Header, std::pair<_nkp::_Manifest,std::vector<unsigned char>>>;
	PluginContent AnalysisPlugin(std::filesystem::path nkpPath);
}

namespace NekoPlugin {
	using PlgMgrCallback = BOOL(*)(uint8_t,void*);
	BOOL CallForAction(uint8_t ActionType, void* pIN);
}

BOOL LoadPlugins(std::string floader);