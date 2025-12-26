// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <filesystem>
#include <vector>

namespace pm = ::PluginsMgr;

pm::PlgManifest ReverseStringManifest(std::string tmpstrL) {
	using namespace std;
	string tmpstrF;
	pm::PlgManifest res;
	for (;;) {
		int end = tmpstrL.find_first_of(';');
		bool exitFlag = true;
		tmpstrF = tmpstrL.substr(0, end);
		if (end + 2 < tmpstrL.size() && end != -1) {
			tmpstrL = tmpstrL.substr(end + 1, tmpstrL.size());
			exitFlag = false;
		}
		end = tmpstrF.find_first_of(':');
		std::pair<std::string, std::string> strp;
		strp.first = std::move(tmpstrF.substr(0, end));
		strp.second = std::move(tmpstrF.substr(end + 1, tmpstrF.size()));
		res.push_back(std::move(strp));
		tmpstrF.clear();
		if (exitFlag || tmpstrL.empty()) {
			break;
		}
	}
	return std::move(res);
}

BOOL LoadPlugins(std::string &floader) {
	namespace fs = std::filesystem;
	if (floader.empty()) return FALSE;
	if (!fs::is_directory(floader))return FALSE;;
	if (fs::is_empty(floader))return FALSE;
	for (auto& file : fs::directory_iterator(floader)) {
		if (!file.is_regular_file()) continue;
		HMODULE h_plg = LoadLibrary(file.path().c_str());
		if (h_plg == 0) {
			LocalFree(h_plg);
			continue;
		}
		pm::PGetManifest GetManifest = (pm::PGetManifest)GetProcAddress(h_plg, "GetManifest");
		std::string manifest = GetManifest();
	}
}