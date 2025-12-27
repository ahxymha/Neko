#include<iostream>
#include<fstream>
#include <vector>
#include<sstream>
#include<any>
#include <string>
#include <boost/json/src.hpp>

#define BOOST_ALL_NO_LIB

namespace _nkp{
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

std::vector<std::any> AnalysisPlgs(std::string s_Plgs) {

}

std::pair<_nkp::_Manifest, std::pair<std::vector<_nkp::_Plg>, std::vector<_nkp::_Pvd>>> AnalysisManifest(const std::string &p_Manifest) {
	std::ifstream io_manifest(p_Manifest.c_str());
	std::pair<_nkp::_Manifest, std::pair<std::vector<_nkp::_Plg>,std::vector<_nkp::_Pvd>>> res;
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

	}
}

bool Generator(std::string p_Manifest,std::string p_PE,std::string p_egConfig,std::string n_out, uint8_t mainVer, uint8_t patchVer, uint8_t BuildVer) {
	using namespace std;
	ofstream _nkp(n_out.c_str(),ios::out|ios::binary);
	ifstream _manifest(p_Manifest.c_str());
	ifstream _egConfig(p_egConfig.c_str());
	ifstream _PE(p_PE.c_str(),ios::in|ios::binary);
	string s_Manifest, s_egConfig;
	_manifest >> s_Manifest;
	_egConfig >> s_egConfig;
	_Header header;
	header.mainVer = mainVer;
	header.patchVer = patchVer;
	header.BuildVer = BuildVer;
	header.l_egConfig = s_egConfig.size() + 1;
	header.l_manifest = s_Manifest.size() + 1;
	header.l_PE = _PE.tellg();
	unsigned char byteArray[sizeof(_Header)];
	memcpy(byteArray, reinterpret_cast<unsigned char*>(&header), sizeof(_Header));
	unsigned char* PE = new unsigned char[header.l_PE + 1];
	_PE.read(reinterpret_cast<char*>(PE), header.l_PE + 1);
	_nkp << byteArray << s_Manifest << s_egConfig << PE;
}

int main() {

}
