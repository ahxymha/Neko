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
#include "../Logger/framework.hpp"
#include <thread>

#pragma pack(1)
struct Plugin2Server {
    DWORD start_pid;                // 启动程序的PID
    char uuid_pipe[37];             // 命名管道UUID字符串
    uint8_t flag;                   // 标志位
    uint64_t checksum;              // 校验和，用于检测损坏
};
#pragma pack(pop)

#pragma data_seg(".shared")
Plugin2Server g_connection_info = {
    0,
    "",
    0,
    0
};
#pragma data_seg()
#pragma comment(linker, "/SECTION:.shared,RWS")

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

// 消息队列模板类
template <typename T>
class MessageQueue {
public:
    void push(const T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        _queue.push(msg);
        _cv.notify_one();
    }

    bool poll(T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        if (!_queue.empty()) {
            msg = _queue.front();
            _queue.pop();
            return true;
        }
        return false;
    }

    void wait(T& msg) {
        std::unique_lock<std::mutex> lck(_mtx);
        while (_queue.empty()) _cv.wait(lck);
        msg = _queue.front();
        _queue.pop();
    }

    void wait(T& msg, HANDLE stop) {
        std::unique_lock<std::mutex> lck(_mtx);
        while (_queue.empty()) {
            _cv.wait_for(lck, std::chrono::milliseconds(100));
            if (WaitForSingleObject(stop, 0) != WAIT_TIMEOUT) {
                throw 4;
            }
        }
        msg = _queue.front();
        _queue.pop();
    }

    size_t size() {
        std::unique_lock<std::mutex> lck(_mtx);
        return _queue.size();
    }

private:
    std::queue<T> _queue;
    std::mutex _mtx;
    std::condition_variable _cv;
};

MessageQueue<std::vector<unsigned char>> g_c2scommq;
MessageQueue<std::vector<unsigned char>> g_s2ccommq;
HANDLE g_hstop;

static uint64_t CalculateChecksum(const struct Plugin2Server* data) {
    struct Plugin2Server temp = *data;
    temp.checksum = 0;

    const uint8_t* bytes = (const uint8_t*)&temp;
    size_t size = sizeof(struct Plugin2Server);

    uint32_t sum1 = 0;
    uint32_t sum2 = 0;

    for (size_t i = 0; i < size; i++) {
        sum1 = (sum1 + bytes[i]) & 0xFFFFFFFF;
        sum2 = (sum2 + sum1) & 0xFFFFFFFF;
    }

    // 合并两个32位校验和
    return ((uint64_t)sum2 << 32) | sum1;
}


namespace pm = ::PluginsMgr;

pm::PluginContent pm::AnalysisPlugin(std::filesystem::path nkpPath) {
    auto nkpSize = std::filesystem::file_size(nkpPath);
    std::ifstream nkpIN(nkpPath, std::ios::binary);
    std::vector<unsigned char> nkpFile(nkpSize);
    nkpIN.read(reinterpret_cast<char*>(nkpFile.data()), nkpSize);
    pm::_nkp::_Header nkpHeader = {};
    std::copy(nkpFile.begin(), nkpFile.begin() + sizeof(pm::_nkp::_Header), reinterpret_cast<unsigned char*>(&nkpHeader));
    nkpFile.erase(nkpFile.begin(), nkpFile.begin() + sizeof(pm::_nkp::_Header));
    PluginContent res;
    if (nkpHeader.flag[0] != 'M' || nkpHeader.flag[1] != 'E' || nkpHeader.flag[2] != 'A' || nkpHeader.flag[3] != 'O') {
        res.first = std::move(nkpHeader);
        return res;
    }
    if (nkpHeader.nkPVer != 2) {
        res.first = std::move(nkpHeader);
        return res;
    }
    res.first = std::move(nkpHeader);
    std::vector<unsigned char> iv, key;
    iv.insert(iv.begin(), nkpHeader.iv, nkpHeader.iv + 16);
    key.insert(key.begin(), nkpHeader.key, nkpHeader.key + 32);
    AESEncryptor decryptor(key, iv);
    auto decryptedData = decryptor.decrypt(nkpFile);
    nkpFile.clear();
    std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
    SHA256(decryptedData.data(), decryptedData.size(), hash.data());
    std::vector<unsigned char> shash;
    shash.insert(shash.begin(), nkpHeader.hash, nkpHeader.hash + SHA256_DIGEST_LENGTH);
    if (shash != hash) {
        res.first = std::move(nkpHeader);
        return res;
    }
    pm::_nkp::_Manifest manifest;
    std::copy(decryptedData.begin(), decryptedData.begin() + sizeof(pm::_nkp::_Manifest::ManifestData), reinterpret_cast<unsigned char*>(&manifest.manifestPODdata));
    decryptedData.erase(decryptedData.begin(), decryptedData.begin() + sizeof(pm::_nkp::_Manifest::ManifestData));
    manifest.Name.reserve(manifest.manifestPODdata.nameLen + 1);
    std::copy(decryptedData.begin(), decryptedData.begin() + manifest.manifestPODdata.nameLen, reinterpret_cast<unsigned char*>(manifest.Name.data()));
    decryptedData.erase(decryptedData.begin(), decryptedData.begin() + manifest.manifestPODdata.nameLen);
    for (uint32_t i = 0; i < manifest.manifestPODdata.plgNum; i++) {
        pm::_nkp::_Plg Plg;
        std::copy(decryptedData.begin(), decryptedData.begin() + sizeof(pm::_nkp::_Plg::PlgData), reinterpret_cast<unsigned char*>(&Plg.plgPODdata));
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + sizeof(pm::_nkp::_Plg::PlgData));
        Plg.name.reserve(Plg.plgPODdata.nameLen + 1);
        Plg.entry.reserve(Plg.plgPODdata.entryLen + 1);
        Plg.description.reserve(Plg.plgPODdata.descriptionLen + 1);
        std::copy(decryptedData.begin(), decryptedData.begin() + Plg.plgPODdata.nameLen, reinterpret_cast<unsigned char*>(Plg.name.data()));
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + Plg.plgPODdata.nameLen);
        std::copy(decryptedData.begin(), decryptedData.begin() + Plg.plgPODdata.entryLen, reinterpret_cast<unsigned char*>(Plg.entry.data()));
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + Plg.plgPODdata.entryLen);
        std::copy(decryptedData.begin(), decryptedData.begin() + Plg.plgPODdata.descriptionLen, reinterpret_cast<unsigned char*>(Plg.description.data()));
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + Plg.plgPODdata.descriptionLen);
        manifest.plgs.push_back(std::move(Plg));
    }
    for (uint32_t i = 0; i < manifest.manifestPODdata.pvdNum; i++) {
        pm::_nkp::_Pvd pvd;
        std::copy(decryptedData.begin(), decryptedData.begin() + sizeof(pm::_nkp::_Pvd::PvdData), reinterpret_cast<unsigned char*>(&pvd.pvdPODdata));
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + sizeof(pm::_nkp::_Pvd::PvdData));
        pvd.callName.reserve(pvd.pvdPODdata.callLen + 1);
        pvd.entryName.reserve(pvd.pvdPODdata.entryLen + 1);
        std::copy(decryptedData.begin(), decryptedData.begin() + pvd.pvdPODdata.callLen, reinterpret_cast<unsigned char*>(pvd.callName.data()));
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + pvd.pvdPODdata.callLen);
        std::copy(decryptedData.begin(), decryptedData.begin() + pvd.pvdPODdata.entryLen, reinterpret_cast<unsigned char*>(pvd.entryName.data()));
        decryptedData.erase(decryptedData.begin(), decryptedData.begin() + pvd.pvdPODdata.entryLen);
        manifest.pvds.push_back(std::move(pvd));
    }
    iv.clear();
    key.clear();
    iv.insert(iv.begin(), manifest.manifestPODdata.iv, manifest.manifestPODdata.iv + 16);
    key.insert(key.begin(), manifest.manifestPODdata.key, manifest.manifestPODdata.key + 32);
    AESEncryptor peDecryptor(key, iv);
    auto PEData = decryptor.decrypt(decryptedData);
    decryptedData.clear();
    std::vector<unsigned char> pe_hash(SHA256_DIGEST_LENGTH);
    SHA256(PEData.data(), PEData.size(), pe_hash.data());
    res.second.second = std::move(PEData);
    shash.clear();
    shash.insert(shash.begin(), manifest.manifestPODdata.hash, manifest.manifestPODdata.hash + SHA256_DIGEST_LENGTH);    
    if (hash != shash) {
        return res;
    }
    res.second.first = std::move(manifest);
    return res;
}

BOOL LoadPlugins(std::string &floader) {
	namespace fs = std::filesystem;
	if (floader.empty()) return FALSE;
	if (!fs::is_directory(floader))return FALSE;;
	if (fs::is_empty(floader))return FALSE;
	for (auto& file : fs::directory_iterator(floader)) {
		
	}
}

HANDLE StringToHandle(const std::string& str) {
    std::stringstream ss;
    uintptr_t handleValue;

    ss << std::hex << str;
    ss >> handleValue;

    if (ss.fail()) {
        return INVALID_HANDLE_VALUE;
    }

    HANDLE h = reinterpret_cast<HANDLE>(handleValue);
    return h;
}

HANDLE GetHandleFromEnvironment(const char* varName) {
    char buffer[256];
    DWORD length = GetEnvironmentVariableA(varName, buffer, sizeof(buffer));

    if (length == 0) {
        return INVALID_HANDLE_VALUE;
    }

    if (length >= sizeof(buffer)) {
        return INVALID_HANDLE_VALUE;
    }

    std::string str(buffer);
    return StringToHandle(str);
}

HANDLE RunPluginWithoutSandbox(pm::_nkp::_Plg plg,HMEMORYMODULE memLib,pm::PluginLoader::plgHost argv,HANDLE h_stop) {
    namespace pl = pm::PluginLoader;
    auto plgEntry = (pl::PluginEntry)MemoryGetProcAddress(memLib, plg.name.c_str());
    plgEntry(argv.MutexLocker, argv.UpdateFlag, argv.ContentLen, argv.Content, h_stop);

}

HANDLE RunPlugin(pm::PluginContent plg) {
    //TODO: Put your Sandbox carete code here.
}

BOOL IsPlgHost(BOOL &sandbox) {
    char buffer[256];
    DWORD length = GetEnvironmentVariableA("PLGHOST", buffer, sizeof(buffer));
    if (length == 0) {
        return FALSE;
    }

    if (length >= sizeof(buffer)) {
        return FALSE;
    }

    if (std::string(buffer) == "SANDBOX") {
        sandbox = TRUE;
    }
    else {
        sandbox = FALSE;
    }

    return TRUE;
}

void LSComPlgHost() {
    if (!g_connection_info.uuid_pipe[0] || !g_connection_info.start_pid || !g_connection_info.checksum) {
        LogClient::error() << "No connection information" << std::endl;
        throw std::runtime_error("No connection information");
    }
    if (g_connection_info.flag != 2) {
        LogClient::error() << "Server is not ready to connect" << std::endl;
        throw std::runtime_error("Server is not ready to connect");
    }
    if (CalculateChecksum(&g_connection_info) != g_connection_info.checksum) {
        LogClient::error() << "Server connection block has been broken" << std::endl;
        throw std::runtime_error("Server connection block has been broken");

    }
    std::stringstream s_pipe;
    s_pipe << "\\\\.\\pipe\\" << g_connection_info.uuid_pipe;
    std::string m_pipe = std::move(s_pipe.str());
    HANDLE pipe = CreateFileA(m_pipe.c_str(), GENERIC_READ | GENERIC_WRITE, NULL, NULL, OPEN_EXISTING, NULL, NULL);
    if (!pipe) {
        LogClient::error() << "Open communication pipe error,Code:" << GetLastError() << std::endl;
        throw std::runtime_error("Open communication pipe error");
    }
    std::vector<unsigned char> key;
    key.resize(32);
    {
        std::unique_ptr<unsigned char> buffer(new unsigned char[32]);
        DWORD rn;
        if (!ReadFile(pipe, buffer.get(), 32, &rn, NULL)) {
            LogClient::error() << "Request communication key error,Code:" << GetLastError() << std::endl;
            throw std::runtime_error("Request communication key error");
        }
        if (rn != 32) {
            LogClient::error() << "We successfully dispatched the key request, but the server responded with what can only be described as 'encrypted confusion',Code:" << GetLastError() << std::endl;
            throw std::runtime_error("We successfully dispatched the key request, but the server responded with what can only be described as 'encrypted confusion'.");
        }
        memcpy_s(key.data(), 32, buffer.get(), 32);
    }
    AESEncryptor Aes256(key);
    {
        DWORD wn;
        if (!WriteFile(pipe, Aes256.getIV().data(), Aes256.getIV().size(), &wn, NULL)) {
            LogClient::error() << "Cannot send IV to server,Code:" << GetLastError() << std::endl;
            throw std::runtime_error("Cannot send IV to server");
        }
        if (wn == 0) {
            LogClient::error() << "Cannot send IV to server,Code:" << GetLastError() << std::endl;
            throw std::runtime_error("Cannot send IV to server");
        }
    }
    {
        std::unique_ptr<unsigned char> buffer(new unsigned char[SHA256_DIGEST_LENGTH]);
        DWORD rn;
        if (!ReadFile(pipe, buffer.get(), SHA256_DIGEST_LENGTH, &rn, NULL)) {
            LogClient::error() << "Request communication hash error,Code:" << GetLastError() << std::endl;
            throw std::runtime_error("Request communication hash error");
        }
        if (rn != SHA256_DIGEST_LENGTH) {
            LogClient::error() << "We successfully dispatched the hash request, but the server responded with what can only be described as 'encrypted confusion',Code:" << GetLastError() << std::endl;
            throw std::runtime_error("We successfully dispatched the hash request, but the server responded with what can only be described as 'encrypted confusion'.");
        }
        std::vector<unsigned char> tmp(Aes256.getIV());
        key.insert(key.end(), tmp.begin(), tmp.end());
        tmp.resize(SHA256_DIGEST_LENGTH);
        SHA256(key.data(), key.size(), tmp.data());
        std::vector<unsigned char> shash(SHA256_DIGEST_LENGTH);
        memcpy_s(shash.data(), shash.size(), buffer.get(), SHA256_DIGEST_LENGTH);
        if (tmp != shash) {
            LogClient::error() << "Server response hash for key and IV is not match to local" << std::endl;
            throw std::runtime_error("Server response hash for key and IV is not match to local");
        }
    }
    key.clear();
    LogClient::info() << "Keys exchange done!" << std::endl;
    auto WritePipe = [&Aes256, &pipe](std::vector<unsigned char>& data, DWORD& wn)->BOOL {
        std::vector<unsigned char> encryptData = Aes256.encrypt(data);
        if (encryptData.size() > 65536) {
            LogClient::error() << "Body is too large";
            return FALSE;
        }
        DWORD sum = 0;
        do {
            DWORD wn;
            if (!WriteFile(pipe, encryptData.data(), encryptData.size(), &wn, NULL)) {
                LogClient::error() << "Call server error,Code:" << GetLastError();
                return FALSE;
            }
            if (wn == 0) {
                LogClient::error() << "Call server error,Code:" << GetLastError();
                return FALSE;
            }
            sum += wn;
        } while (sum != data.size());
        wn = sum;
        return TRUE;
        };
    auto ReadPipe = [&Aes256, &pipe](std::vector<unsigned char>& data, DWORD& rn)->BOOL {
        data.clear();
        data.resize(65536);
        if (!ReadFile(pipe, data.data(), 65536, &rn, NULL)) {
            rn = 0;
            data.clear();
            if (GetLastError() == ERROR_OPERATION_ABORTED) {
                return FALSE;
            }
            LogClient::error() << "Get data error,Code:" << GetLastError();
            return FALSE;
        }
        if (rn == 0) {
            data.clear();
            LogClient::error() << "Get data error,Code:" << GetLastError();
            return FALSE;
        }
        data.resize(rn);
        return TRUE;
        };
    std::thread s2cWorker([&ReadPipe](void)->void {
        while (WaitForSingleObject(g_hstop, 0) == WAIT_TIMEOUT) {
            DWORD rn;
            std::vector<unsigned char> data;
            if (!ReadPipe(data, rn)) {
                continue;
            }
            if (WaitForSingleObject(g_hstop, 0) != WAIT_TIMEOUT) {
                break;
            }
            g_s2ccommq.push(std::move(data));
        }
        });
    std::thread c2sWorker([&WritePipe](void)->void {
        while (WaitForSingleObject(g_hstop, 0) == WAIT_TIMEOUT) {
            DWORD wn;
            std::vector<unsigned char> data;
            try {
                g_c2scommq.wait(data, g_hstop);
            }
            catch (int& e) {
                if (e == 4) {
                    return;
                }
            }
            if (!WritePipe(data, wn)) {
                continue;
            }
        }
        });
    HANDLE c2s = c2sWorker.native_handle(), s2c = s2cWorker.native_handle();
    c2sWorker.detach();
    s2cWorker.detach();
    WaitForSingleObject(g_hstop, INFINITE);
    CancelSynchronousIo(s2c);
    WaitForSingleObject(c2s, INFINITE);
    WaitForSingleObject(s2c, INFINITE);
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        g_hstop = CreateEvent(NULL, TRUE, FALSE, NULL);
        LogClient::debug() << "插件管理器已加载！" << std::endl;
        BOOL sandbox = FALSE;
        if (IsPlgHost(sandbox)) {
            if (sandbox) {
                
            }
        }
        break;
    }
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH: {
        break;
    }
    }
    return TRUE;
}