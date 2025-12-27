#include<iostream>
#include<fstream>
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
            throw std::runtime_error("AES密钥长度必须是16(AES-128), 24(AES-192)或32(AES-256)字节");
        }
    }

    // AES加密函数
    std::vector<unsigned char> encrypt(const std::vector<unsigned char>& plaintext) {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) {
            throw std::runtime_error("无法创建EVP上下文");
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
                throw std::runtime_error("加密初始化失败");
            }

            // 计算输出缓冲区大小（明文长度 + 块大小）
            std::vector<unsigned char> ciphertext(plaintext.size() + EVP_CIPHER_CTX_block_size(ctx));

            int len = 0;
            int ciphertext_len = 0;

            // 处理数据
            if (1 != EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                plaintext.data(), plaintext.size())) {
                throw std::runtime_error("加密更新失败");
            }
            ciphertext_len = len;

            // 完成加密
            if (1 != EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len)) {
                throw std::runtime_error("加密最终化失败");
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
            throw std::runtime_error("无法创建EVP上下文");
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
                throw std::runtime_error("解密初始化失败");
            }

            // 计算输出缓冲区大小
            std::vector<unsigned char> plaintext(ciphertext.size() + EVP_CIPHER_CTX_block_size(ctx));

            int len = 0;
            int plaintext_len = 0;

            // 处理数据
            if (1 != EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                ciphertext.data(), ciphertext.size())) {
                throw std::runtime_error("解密更新失败");
            }
            plaintext_len = len;

            // 完成解密
            if (1 != EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len)) {
                throw std::runtime_error("解密最终化失败");
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
            throw std::runtime_error("IV必须是16字节");
        }
        iv = new_iv;
    }

    // 静态方法：生成随机密钥
    static std::vector<unsigned char> generateKey(int key_size = 32) {
        if (key_size != 16 && key_size != 24 && key_size != 32) {
            throw std::runtime_error("密钥长度必须是16, 24或32字节");
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

std::vector<unsigned char> sha256Simple(const unsigned char* data, size_t len) {
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
    SHA256(data, len, hash.data());
    return hash;
}

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
		d_pvd.callLen = Pvd.at("Call").as_string().size() + 1;
		d_pvd.callName = new char[d_pvd.callLen];
		strcpy_s(d_pvd.callName, d_pvd.callLen, Pvd.at("Call").as_string().c_str());
		d_pvd.entryLen = Pvd.at("Entry").as_string().size() + 1;
		d_pvd.entryName = new char[d_pvd.entryLen];
		strcpy_s(d_pvd.entryName, d_pvd.entryLen, Pvd.at("Entry").as_string().c_str());
		res.second.second.push_back(std::move(d_pvd));
	}
	return res;
}

std::vector<unsigned char> FileHeaderGenerator(std::pair<_nkp::_Manifest, std::pair<std::vector<_nkp::_Plg>, std::vector<_nkp::_Pvd>>> manifest,
                                               _nkp::_Header header) {
    std::vector<unsigned char> fileheader;
    struct TrulyFileManifestHeader {
        uint32_t nameLen;
        uint32_t plgNum;
        uint32_t pvdNum;
    }tf;
    tf.nameLen = manifest.first.nameLen;
    tf.plgNum = manifest.first.plgNum;
    tf.pvdNum = manifest.first.pvdNum;
    

}

int main() {

}
