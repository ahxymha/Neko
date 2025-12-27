#include<iostream>
#include<fstream>
#include <vector>
#include<sstream>
#include<any>
#include <string>
#include <boost/json/src.hpp>

#define BOOST_ALL_NO_LIB

namespace _nkp {
	struct _Header {
		const uint8_t flag[4] = { 'M','E','A','O' };
		const uint8_t nkPVer = 2;
		uint8_t mainVer, patchVer, BuildVer;
		uint32_t l_manifest, l_egConfig, l_PE;
	};

	struct _Manifest {
		uint32_t nameLen;
		uint32_t plgNum;
		uint32_t pvdNum;
		uint8_t reserved[4];
		char* Name;
	};

	struct _Plg {
		uint32_t nameLen;
		uint32_t entryLen;
		uint32_t descriptionLen;
		uint8_t isEnableOnStartup;
		uint8_t reserved[3];
		char* name, * entry, * description;
	};

	struct _Pvd {
		uint32_t callLen;
		uint32_t entryLen;
		char* callName, * entryName;
	};

	struct _egConfig {
		uint32_t Id;
		uint32_t cfgLen;
		char* cfg;
	};
}

std::pair<_nkp::_Manifest, std::pair<std::vector<_nkp::_Plg>, std::vector<_nkp::_Pvd>>> AnalysisManifest(const std::string& p_Manifest) {
	std::ifstream io_manifest(p_Manifest.c_str());
	std::pair<_nkp::_Manifest, std::pair<std::vector<_nkp::_Plg>, std::vector<_nkp::_Pvd>>> res;
	namespace json = boost::json;
	std::stringstream ss;
	ss << io_manifest.rdbuf();
	auto jv = json::parse(ss.str().c_str());
	res.first.nameLen = jv.at("Name").as_string().size() + 1;
	res.first.Name = new char[res.first.nameLen];
	strcpy_s(res.first.Name, res.first.nameLen, jv.at("Name").as_string().c_str());
	auto& Plgs = jv.at("Plugins").as_array();
	res.first.plgNum = Plgs.size();
	for (auto& Plg : Plgs) {
		_nkp::_Plg d_plg;
		d_plg.nameLen = Plg.at("Name").as_string().size() + 1;
		d_plg.name = new char[d_plg.nameLen];
		strcpy_s(d_plg.name, d_plg.nameLen, Plg.at("Name").as_string().c_str());
		d_plg.entryLen = Plg.at("Entry").as_string().size() + 1;
		d_plg.entry = new char[d_plg.entryLen];
		strcpy_s(d_plg.entry, d_plg.entryLen, Plg.at("Entry").as_string().c_str());
		d_plg.descriptionLen = Plg.at("Description").as_string().size() + 1;
		d_plg.description = new char[d_plg.descriptionLen];
		strcpy_s(d_plg.description, d_plg.descriptionLen, Plg.at("Description").as_string().c_str());
		d_plg.isEnableOnStartup = (Plg.at("IsEnableOnStartup").as_bool() ? 1 : 0);
		res.second.first.push_back(std::move(d_plg));
	}
	auto& Pvds = jv.at("Provides").as_array();
	res.first.pvdNum = Pvds.size();
	for (auto& Pvd : Pvds) {
		_nkp::_Pvd d_pvd;
		d_pvd.callLen = Pvd.at("Call").as_string().size() + 1;
		d_pvd.callName = new char[d_pvd.callLen];
		strcpy_s(d_pvd.callName, d_pvd.callLen, Pvd.at("Call").as_string().c_str());
		d_pvd.entryLen = Pvd.at("Entry").as_string().size() + 1;
		d_pvd.entryName = new char[d_pvd.entryLen];
		strcpy_s(d_pvd.entryName, d_pvd.entryLen, Pvd.at("Entry").as_string().c_str());
		res.second.second.push_back(std::move(d_pvd));
	}
	return std::move(res);
}

int main() {
	auto res = AnalysisManifest("tst.json");
	__debugbreak();
}