#include "WinCrypto.h"
#include <cstring>      // For memcpy
#include <vector>
#include <algorithm>    // For std::reverse in rsaEncrypt/Decrypt with CAPI

// --- Utility Functions ---
std::string WinCrypto::bytesToHex(const std::vector<BYTE>& bytes) {
    return bytesToHex(bytes.data(), bytes.size());
}

std::string WinCrypto::bytesToHex(const BYTE* bytes, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return oss.str();
}

std::vector<BYTE> WinCrypto::hexToBytes(const std::string& hex) {
    std::vector<BYTE> bytes;
    if (hex.length() % 2 != 0) {
        throw std::runtime_error("Hex string must have an even number of characters.");
    }
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        BYTE byte = static_cast<BYTE>(std::stoul(byteString, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// --- Constructor and Destructor ---
WinCrypto::WinCrypto() : hProv_(0) {
    if (!CryptAcquireContext(&hProv_, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (GetLastError() == NTE_BAD_KEYSET) { // Common error if keyset doesn't exist
            if (!CryptAcquireContext(&hProv_, nullptr, nullptr, PROV_RSA_AES, CRYPT_NEWKEYSET)) {
                throw std::runtime_error("WinCrypto: Could not create new keyset. Error: " + std::to_string(GetLastError()));
            }
        } else {
            throw std::runtime_error("WinCrypto: CryptAcquireContext failed. Error: " + std::to_string(GetLastError()));
        }
    }
}

WinCrypto::~WinCrypto() {
    if (hProv_) {
        CryptReleaseContext(hProv_, 0);
    }
}

// --- Key Management ---
void WinCrypto::destroyKey(HCRYPTKEY hKey) {
    if (hKey) {
        if (!CryptDestroyKey(hKey)) {
            // Consider logging this error instead of throwing, especially if called from destructors.
            // std::cerr << "WinCrypto: Failed to destroy key. Error: " + std::to_string(GetLastError()) << std::endl;
        }
    }
}

void WinCrypto::generateRsaKeyPair(RsaKeyPair& pair, DWORD dwKeySize, ALG_ID keyAlgorithm) {
    if (pair.hPrivateKey) {
        destroyKey(pair.hPrivateKey);
        pair.hPrivateKey = NULL;
    }
    pair.publicKeyBlob.clear();
    DWORD flags = CRYPT_EXPORTABLE | (dwKeySize << 16);
    if (!CryptGenKey(hProv_, keyAlgorithm, flags, &pair.hPrivateKey)) {
        throw std::runtime_error("WinCrypto: Error during CryptGenKey (RSA). Error: " + std::to_string(GetLastError()));
    }
    try {
        exportPublicKeyBlob(pair.hPrivateKey, pair.publicKeyBlob);
    } catch (...) {
        destroyKey(pair.hPrivateKey);
        pair.hPrivateKey = NULL;
        throw;
    }
}

void WinCrypto::exportPublicKeyBlob(HCRYPTKEY hKeyPair, std::vector<BYTE>& publicKeyBlobVec) {
    DWORD dwBlobLen = 0;
    if (!CryptExportKey(hKeyPair, 0, PUBLICKEYBLOB, 0, nullptr, &dwBlobLen)) {
        throw std::runtime_error("WinCrypto: Error computing public key BLOB length. Error: " + std::to_string(GetLastError()));
    }
    publicKeyBlobVec.resize(dwBlobLen);
    if (!CryptExportKey(hKeyPair, 0, PUBLICKEYBLOB, 0, publicKeyBlobVec.data(), &dwBlobLen)) {
        throw std::runtime_error("WinCrypto: Error during CryptExportKey (Public Key). Error: " + std::to_string(GetLastError()));
    }
}

HCRYPTKEY WinCrypto::importPublicKeyBlob(const std::vector<BYTE>& publicKeyBlobVec, ALG_ID keyAlgorithm) {
    HCRYPTKEY hPublicKey = NULL;
    if (!CryptImportKey(hProv_, publicKeyBlobVec.data(), publicKeyBlobVec.size(), 0, 0, &hPublicKey)) {
        throw std::runtime_error("WinCrypto: Error during CryptImportKey (Public Key). Error: " + std::to_string(GetLastError()));
    }
    return hPublicKey;
}

// --- RSA Encryption/Decryption for Session Keys ---
std::string WinCrypto::encryptWithPublicKey(const std::string& data, const PublicKey& pubKey) {
    HCRYPTKEY hTempPublicKey = NULL;
    try {
        hTempPublicKey = importPublicKeyBlob(pubKey.blob);
        if (!hTempPublicKey) {
            throw std::runtime_error("WinCrypto: Failed to import public key for RSA encryption.");
        }

        std::vector<BYTE> dataBytes(data.begin(), data.end());
        
        // Data for CryptEncrypt with RSA keys needs to be reversed for little-endian Windows CryptoAPI.
        std::reverse(dataBytes.begin(), dataBytes.end());

        DWORD dwDataLen = dataBytes.size();
        std::vector<BYTE> buffer = dataBytes; // Copy data to buffer that might be resized by CryptEncrypt

        // First call to get the required buffer size for encrypted data
        if (!CryptEncrypt(hTempPublicKey, 0, TRUE, 0, nullptr, &dwDataLen, 0)) {
             // This call with nullptr for buffer is to get the size in dwDataLen
        }
        buffer.resize(dwDataLen); // Resize buffer to the required size
        
        // Copy original (reversed) data again for actual encryption
        if (dataBytes.size() > buffer.size()) throw std::runtime_error("RSA encryption buffer too small after size query.");
        memcpy(buffer.data(), dataBytes.data(), dataBytes.size());
        dwDataLen = dataBytes.size(); // Set dwDataLen to actual size of data to encrypt

        if (!CryptEncrypt(hTempPublicKey, 0, TRUE, 0, buffer.data(), &dwDataLen, buffer.size())) {
            throw std::runtime_error("WinCrypto: CryptEncrypt with RSA public key failed. Error: " + std::to_string(GetLastError()));
        }
        // dwDataLen is now the size of the encrypted data in buffer.
        // buffer might be larger, so resize to actual encrypted data size.
        buffer.resize(dwDataLen);

        destroyKey(hTempPublicKey);
        return bytesToHex(buffer);
    } catch (...) {
        if (hTempPublicKey) destroyKey(hTempPublicKey);
        throw;
    }
}

std::string WinCrypto::decryptWithPrivateKey(const std::string& encryptedDataHex, HCRYPTKEY hPrivKey) {
    if (!hPrivKey) {
        throw std::runtime_error("WinCrypto: Invalid private key handle for RSA decryption.");
    }
    std::vector<BYTE> encryptedDataBytes = hexToBytes(encryptedDataHex);
    DWORD dwDataLen = encryptedDataBytes.size();

    // CryptDecrypt may modify the buffer in place.
    std::vector<BYTE> buffer = encryptedDataBytes;

    if (!CryptDecrypt(hPrivKey, 0, TRUE, 0, buffer.data(), &dwDataLen)) {
        throw std::runtime_error("WinCrypto: CryptDecrypt with RSA private key failed. Error: " + std::to_string(GetLastError()));
    }
    // dwDataLen is now the size of the decrypted data.
    buffer.resize(dwDataLen);

    // Data decrypted with RSA keys using Windows CryptoAPI is reversed (little-endian).
    std::reverse(buffer.begin(), buffer.end());

    return std::string(buffer.begin(), buffer.end());
}


// --- AES Symmetric Key Generation ---
std::string WinCrypto::generateRandomSymmetricKey(DWORD keyLengthBytes) {
    std::vector<BYTE> keyBytes(keyLengthBytes);
    if (!CryptGenRandom(hProv_, keyLengthBytes, keyBytes.data())) {
        throw std::runtime_error("WinCrypto: CryptGenRandom failed to generate symmetric key. Error: " + std::to_string(GetLastError()));
    }
    return std::string(keyBytes.begin(), keyBytes.end());
}

// --- Helper: Import raw AES key bytes and IV ---
HCRYPTKEY WinCrypto::importAesKeyBytes(const std::vector<BYTE>& keyBytes, ALG_ID algId) {
    // Construct a PLAINTEXTKEYBLOB structure
    // typedef struct {
    //   BYTE   bType;
    //   BYTE   bVersion;
    //   WORD   reserved;
    //   ALG_ID aiKeyAlg;
    //   DWORD  keyLength;
    // } BLOBHEADER;
    // struct MyPlaintextKeyBlob {
    //   BLOBHEADER hdr;
    //   BYTE       key[keyBytes.size()]; // Or rather, keyLength from hdr.keyLength
    // };
    if (keyBytes.size() != 16 && keyBytes.size() != 24 && keyBytes.size() != 32) {
        throw std::runtime_error("WinCrypto: Invalid AES key size. Must be 16, 24, or 32 bytes.");
    }

    std::vector<BYTE> keyBlob(sizeof(BLOBHEADER) + keyBytes.size());
    BLOBHEADER* pBlobHeader = reinterpret_cast<BLOBHEADER*>(keyBlob.data());
    pBlobHeader->bType = PLAINTEXTKEYBLOB;
    pBlobHeader->bVersion = CUR_BLOB_VERSION;
    pBlobHeader->reserved = 0;
    pBlobHeader->aiKeyAlg = algId; // e.g., CALG_AES_256
    pBlobHeader->keyLength = keyBytes.size();

    std::memcpy(keyBlob.data() + sizeof(BLOBHEADER), keyBytes.data(), keyBytes.size());

    HCRYPTKEY hKey = NULL;
    if (!CryptImportKey(hProv_, keyBlob.data(), keyBlob.size(), 0, CRYPT_IPSEC_HMAC_KEY, &hKey)) { // CRYPT_IPSEC_HMAC_KEY to prevent export issues with some CSPs for raw keys
        // If CRYPT_IPSEC_HMAC_KEY fails, try 0
        if (!CryptImportKey(hProv_, keyBlob.data(), keyBlob.size(), 0, 0, &hKey)) {
             throw std::runtime_error("WinCrypto: CryptImportKey for AES raw key failed. Error: " + std::to_string(GetLastError()));
        }
    }
    return hKey;
}

void WinCrypto::setKeyIV(HCRYPTKEY hKey, const std::vector<BYTE>& iv) {
    if (iv.empty()) return; // No IV to set

    // IV for AES must be 16 bytes (the block size)
    if (iv.size() != 16) {
        throw std::runtime_error("WinCrypto: IV for AES must be 16 bytes.");
    }
    if (!CryptSetKeyParam(hKey, KP_IV, const_cast<BYTE*>(iv.data()), 0)) {
        throw std::runtime_error("WinCrypto: CryptSetKeyParam (KP_IV) failed. Error: " + std::to_string(GetLastError()));
    }
}

// --- AES Encryption/Decryption with raw keys ---
std::string WinCrypto::aesEncrypt(const std::string& data, const std::string& key, const std::string& iv) {
    std::vector<BYTE> dataBytes(data.begin(), data.end());
    std::vector<BYTE> keyBytes(key.begin(), key.end());
    std::vector<BYTE> ivBytes(iv.begin(), iv.end());
    HCRYPTKEY hAesKey = NULL;

    try {
        hAesKey = importAesKeyBytes(keyBytes); // algId defaults to CALG_AES_256 in helper
        setKeyIV(hAesKey, ivBytes);

        DWORD dataLen = dataBytes.size();
        std::vector<BYTE> buffer = dataBytes; // Buffer for CryptEncrypt
        DWORD bufferSize = buffer.size();

        // First call to get required buffer size (includes padding)
        if (!CryptEncrypt(hAesKey, 0, TRUE, 0, nullptr, &dataLen, 0)) {
             // This call with nullptr for buffer is to get the size in dataLen
        }
        buffer.resize(dataLen); // Resize to required size

        // Copy original data for encryption
        if(dataBytes.size() > buffer.size()) throw std::runtime_error("AES encryption buffer too small after size query");
        memcpy(buffer.data(), dataBytes.data(), dataBytes.size());
        dataLen = dataBytes.size(); // Set dataLen to actual size of data to encrypt

        if (!CryptEncrypt(hAesKey, 0, TRUE, 0, buffer.data(), &dataLen, buffer.size())) {
            throw std::runtime_error("WinCrypto: CryptEncrypt (AES) failed. Error: " + std::to_string(GetLastError()));
        }
        buffer.resize(dataLen); // Resize to actual encrypted size

        destroyKey(hAesKey);
        return bytesToHex(buffer);
    } catch (...) {
        if (hAesKey) destroyKey(hAesKey);
        throw;
    }
}

std::string WinCrypto::aesDecrypt(const std::string& encryptedDataHex, const std::string& key, const std::string& iv) {
    std::vector<BYTE> encryptedBytes = hexToBytes(encryptedDataHex);
    std::vector<BYTE> keyBytes(key.begin(), key.end());
    std::vector<BYTE> ivBytes(iv.begin(), iv.end());
    HCRYPTKEY hAesKey = NULL;

    try {
        hAesKey = importAesKeyBytes(keyBytes);
        setKeyIV(hAesKey, ivBytes);

        std::vector<BYTE> buffer = encryptedBytes; // Buffer for CryptDecrypt
        DWORD dataLen = buffer.size();

        if (!CryptDecrypt(hAesKey, 0, TRUE, 0, buffer.data(), &dataLen)) {
            throw std::runtime_error("WinCrypto: CryptDecrypt (AES) failed. Error: " + std::to_string(GetLastError()));
        }
        buffer.resize(dataLen); // Resize to actual decrypted size

        destroyKey(hAesKey);
        return std::string(buffer.begin(), buffer.end());
    } catch (...) {
        if (hAesKey) destroyKey(hAesKey);
        throw;
    }
}


// --- Existing Symmetric Key Operations using HCRYPTKEY (Kept for internal/other uses) ---
// These were previously `encrypt` and `decrypt`. Renamed for clarity.
std::vector<BYTE> WinCrypto::encryptSymmetric(const std::string& data, HCRYPTKEY hKey) {
    DWORD dataLen = data.length();
    std::vector<BYTE> buffer(dataLen + 32); // Padding space, AES block size is 16, can be more with PKCS#7
    memcpy(buffer.data(), data.c_str(), dataLen);
    DWORD allocatedBufferSize = buffer.size();

    if (!CryptEncrypt(hKey, 0, TRUE, 0, buffer.data(), &dataLen, allocatedBufferSize)) {
        if (GetLastError() == ERROR_MORE_DATA || GetLastError() == NTE_BUFFER_TOO_SMALL) {
            buffer.resize(dataLen);
            memcpy(buffer.data(), data.c_str(), data.length()); 
            DWORD originalDataLen = data.length(); 
            allocatedBufferSize = buffer.size();
            if (!CryptEncrypt(hKey, 0, TRUE, 0, buffer.data(), &originalDataLen, allocatedBufferSize)) {
                 throw std::runtime_error("WinCrypto: Error during CryptEncrypt (Symmetric HKEY resize). Error: " + std::to_string(GetLastError()));
            }
            dataLen = originalDataLen;
        } else {
            throw std::runtime_error("WinCrypto: Error during CryptEncrypt (Symmetric HKEY). Error: " + std::to_string(GetLastError()));
        }
    }
    buffer.resize(dataLen);
    return buffer;
}

std::string WinCrypto::decryptSymmetric(const std::vector<BYTE>& data, HCRYPTKEY hKey) {
    std::vector<BYTE> buffer = data;
    DWORD dataLen = buffer.size();
    if (!CryptDecrypt(hKey, 0, TRUE, 0, buffer.data(), &dataLen)) {
        throw std::runtime_error("WinCrypto: Error during CryptDecrypt (Symmetric HKEY). Error: " + std::to_string(GetLastError()));
    }
    buffer.resize(dataLen);
    return std::string(reinterpret_cast<char*>(buffer.data()), dataLen);
}

void WinCrypto::generateSymmetricKeyHandle(HCRYPTKEY* hKey, ALG_ID algorithm) {
    if (!CryptGenKey(hProv_, algorithm, CRYPT_EXPORTABLE, hKey)) {
        throw std::runtime_error("WinCrypto: Error during CryptGenKey (Symmetric HKEY). Error: " + std::to_string(GetLastError()));
    }
}

void WinCrypto::exportSymmetricKeyHandle(HCRYPTKEY hKey, std::vector<BYTE>& keyBlob) {
    DWORD dwBlobLen;
    if (!CryptExportKey(hKey, 0, PLAINTEXTKEYBLOB, 0, nullptr, &dwBlobLen)) {
        throw std::runtime_error("WinCrypto: Error computing BLOB length for symmetric key (HKEY). Error: " + std::to_string(GetLastError()));
    }
    keyBlob.resize(dwBlobLen);
    if (!CryptExportKey(hKey, 0, PLAINTEXTKEYBLOB, 0, keyBlob.data(), &dwBlobLen)) {
        throw std::runtime_error("WinCrypto: Error during CryptExportKey (Symmetric HKEY). Error: " + std::to_string(GetLastError()));
    }
}

HCRYPTKEY WinCrypto::importSymmetricKeyHandle(const std::vector<BYTE>& keyBlob, ALG_ID algorithm) {
    HCRYPTKEY hKey;
    if (!CryptImportKey(hProv_, keyBlob.data(), keyBlob.size(), 0, 0, &hKey)) {
        throw std::runtime_error("WinCrypto: Error during CryptImportKey (Symmetric HKEY). Error: " + std::to_string(GetLastError()));
    }
    return hKey;
}
