#pragma once
#include <Windows.h>
#include<stdint.h>

namespace CfgMgr {
	struct Calls {
		struct Result {
			enum status {
				Done,
				Error
			}state;
			union {
				uint64_t code;
				char *str;
			};
		}rseult;
		enum CallType {
			GetACustomConfig,
			GetFullCustomConfig,
			SetACustomConfig,
			ReplaceFullCustomConfig,
			OpenConfigUI
		}ct;
		char* PlgName;
		char* ConfigSpaceName;
		char* ConfigName;
		char* ContentBugger;
		struct len {
			uint64_t len_PlgName;
			uint64_t len_ConfigSpaceName;
			uint64_t len_ConfigName;
			uint64_t len_ContentBugger;
		}lens;
	};

	typedef BOOL CallCfgMgr(Calls);
}
