// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <filesystem>
#include <vector>

namespace pm = ::PluginsMgr;

pm::PlgManifest ReverseStringManifest(std::string &strManifest) {
	using namespace std;
	vector<string> fields;
	string tmpstr;
	uint16_t tmplen=0;
	for (int i = 0; i < pm::itemNumOfPlgManifest; i++) {
		int end = strManifest.find_first_of(';') - 1;
		tmpstr = strManifest.substr(tmplen, end);

	}

}

BOOL LoadPlugins(std::string &floader) {
	namespace fs = std::filesystem;
	if (floader.empty()) return FALSE;
	if (!fs::is_directory(floader))return FALSE;;
	if (fs::is_empty(floader))return FALSE;
	for (auto& file : fs::directory_iterator(floader)) {
		if (!file.is_regular_file()) continue;
		HMODULE h_plg = LoadLibrary(file.path().c_str());
		pm::PGetManifest GetManifest = (pm::PGetManifest)GetProcAddress(h_plg, "GetManifest");
		std::string manifest = GetManifest();
	}
}