#include<MemoryModule.h>
#include<detours/detours.h>
#include"../Logger/framework.h"
#include <Windows.h>
#include <thread>
#include "../PluginsMgr/framework.h"

namespace pm = PluginsMgr;

BOOL CreateSandboxEnv(BOOL isMain) {
	HANDLE sandboxToken = nullptr;
	CreateRestrictedToken(GetCurrentProcessToken(), DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &sandboxToken);

}
