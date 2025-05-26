#pragma once

#include <string>
#include <vector>
#include <utility> // For std::pair
#include <windows.h> // For Windows API types like HCRYPTPROV, HCRYPTKEY, BYTE, DWORD
#include <wincrypt.h> // For CryptoAPI definitions

// Add these defines BEFORE including <windows.h> if it's included elsewhere
// #define NOMINMAX      // Prevents Windows.h from defining min/max macros
// #define _WINSOCKAPI_  // Prevents Windows.h from including WinSock.h

// Link with Advapi32.lib for CryptoAPI
#pragma comment(lib, "advapi32.lib")

// Helper to convert std::string to std::wstring (for some CryptoAPI functions)
std::wstring s2ws(const std::string& s);

// Helper to convert std::wstring to std::string
std::string ws2s(const std::wstring& s);

// Helper: Convert a byte string to a hexadecimal string
std::string bytesToHex(const std::string& bytes);

// Helper: Convert a hexadecimal string to a byte string
std::string hexToBytes(const std::string& hex);

namespace WinCrypto {

    // Represents a public key
    struct PublicKey {
        std::vector<BYTE> blob; // PUBLICKEYBLOB format
    };

    // Represents a private key
    struct PrivateKey {
        std::vector<BYTE> blob; // PRIVATEKEYBLOB format
    };

    // Generates an RSA key pair
    std::pair<PublicKey, PrivateKey> generateRsaKeyPair();

    // Encrypts data with a public key (RSA)
    std::string encryptWithPublicKey(const std::string& data, const PublicKey& pubKey);

    // Decrypts data with a private key (RSA)
    std::string decryptWithPrivateKey(const std::string& data, const PrivateKey& privKey);

    // Generates a cryptographically secure random symmetric key (AES)
    std::string generateRandomSymmetricKey(size_t length);

    // Symmetric AES Encryption
    std::string aesEncrypt(const std::string& data, const std::string& key);

    // Symmetric AES Decryption
    std::string aesDecrypt(const std::string& data, const std::string& key);

} // namespace WinCrypto