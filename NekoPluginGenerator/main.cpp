#include<iostream>
#include<fstream>
#include<filesystem>
#include <vector>
#include<sstream>
#include<any>
#include <string>
#include <boost/json/src.hpp>

#define BOOST_ALL_NO_LIB

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#pragma comment(lib,"libcrypto.lib")
#pragma comment(lib,"libssl.lib")

class AESEncryptor {
private:
    std::vector<unsigned char> key;      // AES密钥（16, 24或32字节）
    std::vector<unsigned char> iv;       // 初始化向量（16字节）

public:
    // 构造函数
    AESEncryptor(const std::vector<unsigned char>& key,
        const std::vector<unsigned char>& iv = {})
        : key(key), iv(iv) {

        // 如果未提供IV，生成随机IV
        if (this->iv.empty()) {
            this->iv.resize(16);
            RAND_bytes(this->iv.data(), 16);
        }

        validateKeySize();
    }

    // 验证密钥长度
    void validateKeySize() {
        if (key.size() != 16 && key.size() != 24 && key.size() != 32) {
            throw std::runtime_error("AES key length must be 16(AES-128), 24(AES-192) or 32(AES-256) bytes");
        }
    }

    // AES加密函数
    std::vector<unsigned char> encrypt(const std::vector<unsigned char>& plaintext) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create EVP context");
        }

        try {
            // 根据密钥长度选择加密算法
            const EVP_CIPHER* cipher = nullptr;
            if (key.size() == 16) {
                cipher = EVP_aes_128_cbc();
            }
            else if (key.size() == 24) {
                cipher = EVP_aes_192_cbc();
            }
            else {
                cipher = EVP_aes_256_cbc();
            }

            // 初始化加密操作
            if (1 != EVP_EncryptInit_ex(ctx, cipher, nullptr,
                key.data(), iv.data())) {
                throw std::runtime_error("Encryption initialization failed");
            }

            // 计算输出缓冲区大小（明文长度 + 块大小）
            std::vector<unsigned char> ciphertext(plaintext.size() + EVP_CIPHER_CTX_block_size(ctx));

            int len = 0;
            int ciphertext_len = 0;

            // 处理数据
            if (1 != EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                plaintext.data(), plaintext.size())) {
                throw std::runtime_error("Encryption update failed");
            }
            ciphertext_len = len;

            // 完成加密
            if (1 != EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) {
                throw std::runtime_error("Encryption finalization failed");
            }
            ciphertext_len += len;

            // 调整密文大小为实际长度
            ciphertext.resize(ciphertext_len);

            EVP_CIPHER_CTX_free(ctx);
            return ciphertext;

        }
        catch (...) {
            EVP_CIPHER_CTX_free(ctx);
            throw;
        }
    }

    // AES解密函数
    std::vector<unsigned char> decrypt(const std::vector<unsigned char>& ciphertext) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("Failed to create EVP context");
        }

        try {
            // 根据密钥长度选择解密算法
            const EVP_CIPHER* cipher = nullptr;
            if (key.size() == 16) {
                cipher = EVP_aes_128_cbc();
            }
            else if (key.size() == 24) {
                cipher = EVP_aes_192_cbc();
            }
            else {
                cipher = EVP_aes_256_cbc();
            }

            // 初始化解密操作
            if (1 != EVP_DecryptInit_ex(ctx, cipher, nullptr,
                key.data(), iv.data())) {
                throw std::runtime_error("Decryption initialization failed");
            }

            // 计算输出缓冲区大小
            std::vector<unsigned char> plaintext(ciphertext.size() + EVP_CIPHER_CTX_block_size(ctx));

            int len = 0;
            int plaintext_len = 0;

            // 处理数据
            if (1 != EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                ciphertext.data(), ciphertext.size())) {
                throw std::runtime_error("Decryption update failed");
            }
            plaintext_len = len;

            // 完成解密
            if (1 != EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len)) {
                throw std::runtime_error("Decryption finalization failed");
            }
            plaintext_len += len;

            // 调整明文大小为实际长度
            plaintext.resize(plaintext_len);

            EVP_CIPHER_CTX_free(ctx);
            return plaintext;

        }
        catch (...) {
            EVP_CIPHER_CTX_free(ctx);
            throw;
        }
    }

    // 获取IV（用于传输或存储）
    const std::vector<unsigned char>& getIV() const {
        return iv;
    }

    // 设置IV（用于解密时）
    void setIV(const std::vector<unsigned char>& new_iv) {
        if (new_iv.size() != 16) {
            throw std::runtime_error("IV must be 16 bytes");
        }
        iv = new_iv;
    }

    // 静态方法：生成随机密钥
    static std::vector<unsigned char> generateKey(int key_size = 32) {
        if (key_size != 16 && key_size != 24 && key_size != 32) {
            throw std::runtime_error("Key length must be 16, 24 or 32 bytes");
        }

        std::vector<unsigned char> key(key_size);
        RAND_bytes(key.data(), key_size);
        return key;
    }

    // 静态方法：生成随机IV
    static std::vector<unsigned char> generateIV() {
        std::vector<unsigned char> iv(16);
        RAND_bytes(iv.data(), 16);
        return iv;
    }
};

#pragma pack(1)
namespace _nkp{
	struct _Header {
		const uint8_t flag[4] = { 'M','E','A','O' };
		const uint8_t nkPVer = 2;
        uint8_t key[32];
        uint8_t iv[16];
        uint8_t hash[32];
		uint8_t mainVer, patchVer, BuildVer;
		uint32_t l_manifest, l_PE;
	};

	struct _Manifest {
		uint32_t nameLen;
		uint32_t plgNum;
        uint32_t pvdNum;
        uint32_t reserved = 0;
        uint64_t plgLen;
		uint64_t pvdLen;
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
}
#pragma pack()

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
		res.first.plgLen += sizeof(_nkp::_Plg) - 24 + d_plg.nameLen + d_plg.entryLen + d_plg.descriptionLen;
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
		res.first.pvdLen += sizeof(_nkp::_Pvd) - 16 + d_pvd.callLen + d_pvd.entryLen;
	}
	return res;
}

std::vector<unsigned char> FileGenerator(std::pair<_nkp::_Manifest, std::pair<std::vector<_nkp::_Plg>, std::vector<_nkp::_Pvd>>> manifest,
                                         _nkp::_Header &header,
                                         std::string &p_PE) {
    std::vector<unsigned char> fileheaderWithoutHeader;
    fileheaderWithoutHeader.reserve(sizeof(_nkp::_Manifest) - 8 + manifest.first.nameLen + manifest.first.plgLen + manifest.first.pvdLen);
    fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
        reinterpret_cast<unsigned char*>(&manifest.first),
        reinterpret_cast<unsigned char*>(&manifest.first) + sizeof(_nkp::_Manifest) - 8);
    fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
        reinterpret_cast<unsigned char*>(manifest.first.Name),
        reinterpret_cast<unsigned char*>(manifest.first.Name + manifest.first.nameLen));
    for (auto &d_plg : manifest.second.first) {
        fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
            reinterpret_cast<unsigned char*>(&d_plg),
            reinterpret_cast<unsigned char*>(&d_plg) + sizeof(_nkp::_Plg) - 24);
        fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
            reinterpret_cast<unsigned char*>(d_plg.name),
            reinterpret_cast<unsigned char*>(d_plg.name + d_plg.nameLen));
        fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
            reinterpret_cast<unsigned char*>(d_plg.entry),
            reinterpret_cast<unsigned char*>(d_plg.entry + d_plg.entryLen));
        fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
            reinterpret_cast<unsigned char*>(d_plg.description),
            reinterpret_cast<unsigned char*>(d_plg.description + d_plg.descriptionLen));
    }
    for (auto& d_pvd : manifest.second.second) {
        fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
            reinterpret_cast<unsigned char*>(&d_pvd),
            reinterpret_cast<unsigned char*>(&d_pvd) + sizeof(_nkp::_Pvd) - 16);
        fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
            reinterpret_cast<unsigned char*>(d_pvd.callName),
            reinterpret_cast<unsigned char*>(d_pvd.callName + d_pvd.callLen));
        fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(),
            reinterpret_cast<unsigned char*>(d_pvd.entryName),
            reinterpret_cast<unsigned char*>(d_pvd.entryName + d_pvd.entryLen));
    }
    namespace fs = std::filesystem;
    header.l_PE = fs::file_size(p_PE);
    header.l_manifest = fileheaderWithoutHeader.size();
    std::ifstream in_PE(p_PE);
    unsigned char* c_PE = new unsigned char[header.l_PE];
    in_PE.read(reinterpret_cast<char*>(c_PE), header.l_PE);
    fileheaderWithoutHeader.insert(fileheaderWithoutHeader.end(), c_PE, c_PE + header.l_PE);
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH); 
    SHA256(fileheaderWithoutHeader.data(), fileheaderWithoutHeader.size(), hash.data());
    memcpy_s(header.hash, 32, hash.data(), hash.size());
    auto iv = AESEncryptor::generateIV();
    memcpy_s(header.iv, 16, iv.data(), iv.size());
    auto key = AESEncryptor::generateKey(32);
    memcpy_s(header.key, 32, key.data(), key.size());
    AESEncryptor aes(key, iv);
    auto encryptData = aes.encrypt(fileheaderWithoutHeader);
    //auto encryptData = fileheaderWithoutHeader;
    std::vector<unsigned char> finalData;
    finalData.reserve(sizeof(_nkp::_Header) + encryptData.size());
    const unsigned char* headerPtr = reinterpret_cast<const unsigned char*>(&header);
    finalData.insert(finalData.end(), headerPtr, headerPtr + sizeof(_nkp::_Header));
    finalData.insert(finalData.end(), encryptData.begin(), encryptData.end());

    return finalData;
    return encryptData;
}

int main() {
    _nkp::_Header header;
    header.mainVer = 1;
    header.patchVer = 0;
    header.BuildVer = 0;
    system("pwd");
    std::ofstream nkp("out.nkp", std::ios::out | std::ios::binary);
    std::string PE_p = "plugin.dll";
    std::string Manifest_p = ".manifest";
    auto nkpf = FileGenerator(AnalysisManifest(Manifest_p), header, PE_p);
    nkp.write(reinterpret_cast<char*>(nkpf.data()), nkpf.size());
    return 0;
}
