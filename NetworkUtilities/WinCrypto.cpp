#include "WinCrypto.h"
#include <sstream>
#include <iomanip>   // For std::hex, std::setw, std::setfill
#include <stdexcept> // For exceptions
#include <iostream>  // For std::cerr (consider using a proper logging system)

// Helper to convert std::string to std::wstring (for some CryptoAPI functions)
std::wstring s2ws(const std::string& s) {
    int len;
    int slength = (int)s.length();
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    std::wstring r(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
    return r;
}

// Helper to convert std::wstring to std::string
std::string ws2s(const std::wstring& s) {
    int len;
    int slength = (int)s.length();
    len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, NULL, NULL);
    std::string r(len, '\0');
    WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, NULL, NULL);
    return r;
}

// Helper: Convert a byte string to a hexadecimal string
std::string bytesToHex(const std::string& bytes) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char byte : bytes) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

// Helper: Convert a hexadecimal string to a byte string
std::string hexToBytes(const std::string& hex) {
    std::string bytes;
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        char byte = (char)strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

namespace WinCrypto {

    // Generates an RSA key pair
    std::pair<PublicKey, PrivateKey> generateRsaKeyPair() {
        HCRYPTPROV hProv = 0;
        HCRYPTKEY hKey = 0;
        PublicKey pubKey;
        PrivateKey privKey;

        // Acquire a cryptographic context handle
        if (!CryptAcquireContext(
            &hProv,
            NULL, // Default key container
            MS_ENH_RSA_AES_PROV, // Microsoft Enhanced RSA and AES Cryptographic Provider
            PROV_RSA_AES,        // Provider type for RSA and AES
            CRYPT_NEWKEYSET))    // Create a new key container if one doesn't exist
        {
            if (GetLastError() == NTE_EXISTS) { // If keyset already exists, try acquiring without CRYPT_NEWKEYSET
                if (!CryptAcquireContext(
                    &hProv,
                    NULL,
                    MS_ENH_RSA_AES_PROV,
                    PROV_RSA_AES,
                    0)) {
                    throw std::runtime_error("Error acquiring CryptoAPI context: " + std::to_string(GetLastError()));
                }
            }
            else {
                throw std::runtime_error("Error acquiring CryptoAPI context: " + std::to_string(GetLastError()));
            }
        }

        // Generate an RSA key pair (2048 bits for reasonable security)
        if (!CryptGenKey(
            hProv,
            AT_KEYEXCHANGE, // For encryption/decryption of symmetric keys
            (2048 << 16) | CRYPT_EXPORTABLE, // 2048-bit key, exportable
            &hKey)) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error generating RSA key pair: " + std::to_string(GetLastError()));
        }

        // Export public key BLOB
        DWORD dwBlobLen = 0;
        CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, NULL, &dwBlobLen);
        pubKey.blob.resize(dwBlobLen);
        if (!CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, pubKey.blob.data(), &dwBlobLen)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error exporting public key: " + std::to_string(GetLastError()));
        }

        // Export private key BLOB
        dwBlobLen = 0;
        CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, NULL, &dwBlobLen);
        privKey.blob.resize(dwBlobLen);
        if (!CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, privKey.blob.data(), &dwBlobLen)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error exporting private key: " + std::to_string(GetLastError()));
        }

        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);

        return { pubKey, privKey };
    }

    // Encrypts data with a public key (RSA)
    std::string encryptWithPublicKey(const std::string& data, const PublicKey& pubKey) {
        HCRYPTPROV hProv = 0;
        HCRYPTKEY hPubKey = 0;
        std::string encrypted_data;

        if (!CryptAcquireContext(&hProv, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, 0)) {
            throw std::runtime_error("Error acquiring CryptoAPI context for encryption: " + std::to_string(GetLastError()));
        }

        // Import the public key
        if (!CryptImportKey(hProv, pubKey.blob.data(), (DWORD)pubKey.blob.size(), 0, 0, &hPubKey)) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error importing public key: " + std::to_string(GetLastError()));
        }

        // Calculate output buffer size (RSA encryption output is fixed size, usually key_size_bytes)
        // For 2048-bit RSA, this is 2048/8 = 256 bytes.
        // Data to encrypt should be less than (key_size_bytes - padding_overhead).
        // For PKCS#1 v1.5 padding, max data size is key_size_bytes - 11.
        // If data is larger, it must be chunked (which is why we use symmetric keys for bulk data).
        DWORD dwDataLen = (DWORD)data.length();
        DWORD dwBufLen = (DWORD)pubKey.blob.size(); // RSA public key BLOB size is often used to infer key size
        // Correct way to get key size from imported key
        DWORD dwKeyLenBits = 0;
        DWORD dwLen = sizeof(dwKeyLenBits);
        if (!CryptGetKeyParam(hPubKey, KP_KEYLEN, (BYTE*)&dwKeyLenBits, &dwLen, 0)) {
            CryptDestroyKey(hPubKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error getting key length: " + std::to_string(GetLastError()));
        }
        dwBufLen = dwKeyLenBits / 8; // Size of the encrypted output in bytes

        std::vector<BYTE> buffer(dwBufLen);
        if (dwDataLen > dwBufLen - 11) { // PKCS#1 v1.5 padding overhead
            CryptDestroyKey(hPubKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Data too large for direct RSA encryption. Use hybrid encryption.");
        }
        memcpy(buffer.data(), data.c_str(), dwDataLen); // Copy data to buffer

        // Encrypt with RSA public key
        if (!CryptEncrypt(
            hPubKey,
            0,    // No hash object
            TRUE,  // Final block
            0,    // Default padding (PKCS#1 v1.5 for RSA)
            buffer.data(),
            &dwDataLen, // Input data length, becomes output ciphertext length
            (DWORD)buffer.size())) // Size of the buffer
        {
            CryptDestroyKey(hPubKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error encrypting with public key: " + std::to_string(GetLastError()));
        }

        encrypted_data.assign((char*)buffer.data(), dwDataLen);

        CryptDestroyKey(hPubKey);
        CryptReleaseContext(hProv, 0);
        return encrypted_data;
    }

    // Decrypts data with a private key (RSA)
    std::string decryptWithPrivateKey(const std::string& data, const PrivateKey& privKey) {
        HCRYPTPROV hProv = 0;
        HCRYPTKEY hPrivKey = 0;
        std::string decrypted_data;

        if (!CryptAcquireContext(&hProv, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, 0)) {
            throw std::runtime_error("Error acquiring CryptoAPI context for decryption: " + std::to_string(GetLastError()));
        }

        // Import the private key
        if (!CryptImportKey(hProv, privKey.blob.data(), (DWORD)privKey.blob.size(), 0, 0, &hPrivKey)) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error importing private key: " + std::to_string(GetLastError()));
        }

        // RSA decryption output is also fixed size (key_size_bytes), then padded data is stripped.
        DWORD dwDataLen = (DWORD)data.length();
        std::vector<BYTE> buffer(dwDataLen);
        memcpy(buffer.data(), data.c_str(), dwDataLen);

        // Decrypt with RSA private key
        if (!CryptDecrypt(
            hPrivKey,
            0,    // No hash object
            TRUE,  // Final block (removes padding)
            0,    // No flags (PKCS#1 v1.5 for RSA)
            buffer.data(),
            &dwDataLen)) // Input ciphertext length, becomes output plaintext length
        {
            CryptDestroyKey(hPrivKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error decrypting with private key: " + std::to_string(GetLastError()));
        }

        decrypted_data.assign((char*)buffer.data(), dwDataLen);

        CryptDestroyKey(hPrivKey);
        CryptReleaseContext(hProv, 0);
        return decrypted_data;
    }

    // Generates a cryptographically secure random symmetric key (AES)
    std::string generateRandomSymmetricKey(size_t length) {
        HCRYPTPROV hProv = 0;
        std::string key(length, '\0');

        if (!CryptAcquireContext(&hProv, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, 0)) {
            throw std::runtime_error("Error acquiring CryptoAPI context for random key generation: " + std::to_string(GetLastError()));
        }

        if (!CryptGenRandom(hProv, (DWORD)length, (BYTE*)key.data())) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error generating random symmetric key: " + std::to_string(GetLastError()));
        }

        CryptReleaseContext(hProv, 0);
        return key;
    }

    // Symmetric AES Encryption
    std::string aesEncrypt(const std::string& data, const std::string& key) {
        HCRYPTPROV hProv = 0;
        HCRYPTKEY hKey = 0;
        std::string encrypted_data;

        if (!CryptAcquireContext(&hProv, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, 0)) {
            throw std::runtime_error("Error acquiring CryptoAPI context for AES encryption: " + std::to_string(GetLastError()));
        }

        // Import the symmetric key directly (not derived from hash)
        // A simple way to import a raw key is to create a PLAINTEXTKEYBLOB
        struct {
            BLOBHEADER header;
            DWORD dwKeyLen;
        } keyBlobHeader;

        keyBlobHeader.header.bType = PLAINTEXTKEYBLOB;
        keyBlobHeader.header.bVersion = CUR_BLOB_VERSION;
        keyBlobHeader.header.reserved = 0;
        keyBlobHeader.header.aiKeyAlg = CALG_AES_256; // Specify AES-256 for symmetric encryption
        keyBlobHeader.dwKeyLen = (DWORD)key.length();

        std::vector<BYTE> keyBlob(sizeof(keyBlobHeader) + key.length());
        memcpy(keyBlob.data(), &keyBlobHeader, sizeof(keyBlobHeader));
        memcpy(keyBlob.data() + sizeof(keyBlobHeader), key.data(), key.length());

        if (!CryptImportKey(hProv, keyBlob.data(), (DWORD)keyBlob.size(), 0, 0, &hKey)) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error importing AES symmetric key: " + std::to_string(GetLastError()));
        }

        // Set cipher mode to CBC (or GCM if CNG is used)
        DWORD dwMode = CRYPT_MODE_CBC;
        if (!CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error setting AES key mode: " + std::to_string(GetLastError()));
        }

        // Generate a random IV and set it (crucial for CBC mode)
        std::vector<BYTE> iv(16); // AES block size is 16 bytes
        if (!CryptGenRandom(hProv, (DWORD)iv.size(), iv.data())) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error generating IV for AES: " + std::to_string(GetLastError()));
        }
        if (!CryptSetKeyParam(hKey, KP_IV, iv.data(), 0)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error setting AES IV: " + std::to_string(GetLastError()));
        }
        // Prepend IV to ciphertext (common practice)
        encrypted_data.append((char*)iv.data(), iv.size());

        // Calculate buffer size for encryption (must be a multiple of block size + padding)
        DWORD dwDataLen = (DWORD)data.length();
        DWORD dwBufLen = dwDataLen;
        // Get block size for padding calculation
        DWORD dwBlockLen = 0;
        DWORD dwTempLen = sizeof(dwBlockLen);
        CryptGetKeyParam(hKey, KP_BLOCKLEN, (BYTE*)&dwBlockLen, &dwTempLen, 0);
        dwBlockLen /= 8; // Convert bits to bytes

        // Calculate actual buffer size needed after padding
        DWORD dwPaddingLen = dwBlockLen - (dwDataLen % dwBlockLen);
        if (dwPaddingLen == 0) dwPaddingLen = dwBlockLen; // If data is already a multiple, add a full block of padding
        dwBufLen += dwPaddingLen;

        std::vector<BYTE> buffer(dwBufLen);
        memcpy(buffer.data(), data.c_str(), dwDataLen); // Copy data to buffer

        // Encrypt data
        if (!CryptEncrypt(
            hKey,
            0,    // No hash object
            TRUE,  // Final block (applies padding)
            0,    // No flags (PKCS#5/PKCS#7 padding by default for block ciphers)
            buffer.data(),
            &dwDataLen, // Input data length, becomes output ciphertext length
            (DWORD)buffer.size())) // Size of the buffer
        {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error AES encrypting data: " + std::to_string(GetLastError()));
        }

        encrypted_data.append((char*)buffer.data(), dwDataLen);

        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return encrypted_data;
    }

    // Symmetric AES Decryption
    std::string aesDecrypt(const std::string& data, const std::string& key) {
        HCRYPTPROV hProv = 0;
        HCRYPTKEY hKey = 0;
        std::string decrypted_data;

        if (!CryptAcquireContext(&hProv, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, 0)) {
            throw std::runtime_error("Error acquiring CryptoAPI context for AES decryption: " + std::to_string(GetLastError()));
        }

        // Import the symmetric key
        struct {
            BLOBHEADER header;
            DWORD dwKeyLen;
        } keyBlobHeader;

        keyBlobHeader.header.bType = PLAINTEXTKEYBLOB;
        keyBlobHeader.header.bVersion = CUR_BLOB_VERSION;
        keyBlobHeader.header.reserved = 0;
        keyBlobHeader.header.aiKeyAlg = CALG_AES_256;
        keyBlobHeader.dwKeyLen = (DWORD)key.length();

        std::vector<BYTE> keyBlob(sizeof(keyBlobHeader) + key.length());
        memcpy(keyBlob.data(), &keyBlobHeader, sizeof(keyBlobHeader));
        memcpy(keyBlob.data() + sizeof(keyBlobHeader), key.data(), key.length());

        if (!CryptImportKey(hProv, keyBlob.data(), (DWORD)keyBlob.size(), 0, 0, &hKey)) {
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error importing AES symmetric key: " + std::to_string(GetLastError()));
        }

        // Set cipher mode to CBC
        DWORD dwMode = CRYPT_MODE_CBC;
        if (!CryptSetKeyParam(hKey, KP_MODE, (BYTE*)&dwMode, 0)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error setting AES key mode: " + std::to_string(GetLastError()));
        }

        // Extract IV from the beginning of the ciphertext
        std::vector<BYTE> iv(16); // AES block size is 16 bytes
        if (data.length() < iv.size()) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Ciphertext too short to contain IV for AES decryption.");
        }
        memcpy(iv.data(), data.c_str(), iv.size());
        if (!CryptSetKeyParam(hKey, KP_IV, iv.data(), 0)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error setting AES IV for decryption: " + std::to_string(GetLastError()));
        }

        // Copy ciphertext (excluding IV) to buffer
        DWORD dwDataLen = (DWORD)(data.length() - iv.size());
        std::vector<BYTE> buffer(dwDataLen);
        memcpy(buffer.data(), data.c_str() + iv.size(), dwDataLen);

        // Decrypt data
        if (!CryptDecrypt(
            hKey,
            0,    // No hash object
            TRUE,  // Final block (removes padding)
            0,    // No flags
            buffer.data(),
            &dwDataLen)) // Input ciphertext length, becomes output plaintext length
        {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            throw std::runtime_error("Error AES decrypting data: " + std::to_string(GetLastError()));
        }

        decrypted_data.assign((char*)buffer.data(), dwDataLen);

        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        return decrypted_data;
    }

} // namespace WinCrypto