#pragma once
#include <vector>
namespace PluginsMgr {
	namespace CSCommunication {
		enum RequestMode {
			Read,
			Write,
			Delete,
			Create,
			Get,
			Post,
			Modify
		};
		enum RequestFunction {
			File,
			Pipe,
			WebUI,
			Settings,
			Plugin,
			Provider,
			System,
			Session,
			PluginKey
		};
#pragma pack(push, 2)
		struct RequstBody {
			RequestMode mode;
			RequestFunction function;
			__int32 sessionId;
			__int16 key;
			__int64 datalen;
			unsigned char* data;
		};
#pragma pack(pop)
		namespace Reciver {
			struct RequstBody {
				RequestMode mode;
				RequestFunction function;
				__int32 sessionId;
				__int16 key;
				__int64 datalen;
				std::vector<unsigned char> data;
			};
		}
	}
}