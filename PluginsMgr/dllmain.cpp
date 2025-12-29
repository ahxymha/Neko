// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include <filesystem>
#include <fstream>
#include <vector>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <MemoryModule.h>



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



namespace pm = ::PluginsMgr;

pm::PluginContent pm::AnalysisPlugin(std::filesystem::path nkpPath) {
    auto nkpSize = std::filesystem::file_size(nkpPath);
    std::ifstream nkpIN(nkpPath, std::ios::binary);
    std::vector<unsigned char> nkpFile(nkpSize);
    nkpIN.read(reinterpret_cast<char*>(nkpFile.data()), nkpSize);
    pm::_nkp::_Header nkpHeader = {};
    std::copy(nkpFile.begin(), nkpFile.begin() + sizeof(pm::_nkp::_Header), reinterpret_cast<unsigned char*>(&nkpHeader));
    nkpFile.erase(nkpFile.begin(), nkpFile.begin() + sizeof(pm::_nkp::_Header));
    std::pair<_nkp::_Header, std::pair<_nkp::_Header, std::vector<unsigned char>>> res;
    if (nkpHeader.flag[0] != 'M' || nkpHeader.flag[1] != 'E' || nkpHeader.flag[2] != 'A' || nkpHeader.flag[3] != 'O') {
        res.first = std::move(nkpHeader);
        return res;
    }
    if (nkpHeader.nkPVer != 2) {
        res.first = std::move(nkpHeader);
        return res;
    }
    std::vector<unsigned char> iv, key;
    iv.insert(iv.begin(), nkpHeader.iv, nkpHeader.iv + 16);
    key.insert(key.begin(), nkpHeader.key, nkpHeader.key + 32);
    AESEncryptor decryptor(key, iv);
    auto decryptedData = decryptor.decrypt(nkpFile);
}

BOOL LoadPlugins(std::string &floader) {
	namespace fs = std::filesystem;
	if (floader.empty()) return FALSE;
	if (!fs::is_directory(floader))return FALSE;;
	if (fs::is_empty(floader))return FALSE;
	for (auto& file : fs::directory_iterator(floader)) {
		
	}
}