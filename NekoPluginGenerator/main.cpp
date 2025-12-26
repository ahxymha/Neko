#include<iostream>
#include<fstream>
#include<sstream>

struct _Header {
	const uint8_t flag[4] = { 'M','E','A','O' };
	const uint8_t nkPVer = 2;
	uint8_t mainVer, patchVer, BuildVer;
	uint32_t l_manifest, l_egConfig, l_PE;
};

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
