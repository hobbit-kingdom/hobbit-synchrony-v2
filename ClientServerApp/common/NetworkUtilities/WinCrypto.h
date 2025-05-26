#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <wincrypt.h>
#include <stdexcept> // For std::runtime_error
#include <iomanip>   // For std::setw, std::setfill, std::hex
#include <sstream>   // For std::ostringstream, std::istringstream

struct PublicKey {
    std::vector<BYTE> blob; 
};

// For RsaKeyPair, it's already defined and seems fine.
// It holds HCRYPTKEY hPrivateKey and std::vector<BYTE> publicKeyBlob.
struct RsaKeyPair {
    HCRYPTKEY hPrivateKey = NULL;
    std::vector<BYTE> publicKeyBlob; // This is the public key.
    PublicKey getPublicKeyStruct() const { return {publicKeyBlob}; }
    // No specific PrivateKey struct needed if decryptWithPrivateKey takes HCRYPTKEY
};


class WinCrypto {
public:
    WinCrypto();
    ~WinCrypto();

    // --- RSA Key Pair Management ---
    void generateRsaKeyPair(RsaKeyPair& pair, DWORD dwKeySize = 2048, ALG_ID keyAlgorithm = CALG_RSA_KEYX);
    void exportPublicKeyBlob(HCRYPTKEY hKeyPair, std::vector<BYTE>& publicKeyBlob);
    HCRYPTKEY importPublicKeyBlob(const std::vector<BYTE>& publicKeyBlob, ALG_ID keyAlgorithm = CALG_RSA_KEYX);
    void destroyKey(HCRYPTKEY hKey);

    // --- RSA Encryption/Decryption for Session Keys (as per subtask) ---
    // Encrypts small data (e.g., a session key string) using RSA public key.
    // Output is hex-encoded encrypted string.
    std::string encryptWithPublicKey(const std::string& data, const PublicKey& pubKey);

    // Decrypts hex-encoded encrypted data (e.g., an encrypted session key) using RSA private key handle.
    // Output is decrypted string.
    std::string decryptWithPrivateKey(const std::string& encryptedDataHex, HCRYPTKEY hPrivKey);

    // --- AES Symmetric Key Operations (as per subtask) ---
    // Generates a random symmetric key. Returns raw key bytes as a string.
    std::string generateRandomSymmetricKey(DWORD keyLengthBytes = 32); // e.g., 32 bytes for AES-256

    // AES encrypts data string using a raw symmetric key string and IV string.
    // IV should be random (16 bytes for AES). Output is hex-encoded encrypted string.
    std::string aesEncrypt(const std::string& data, const std::string& key, const std::string& iv);

    // AES decrypts hex-encoded encrypted data string using a raw symmetric key string and IV string.
    // Output is decrypted data string.
    std::string aesDecrypt(const std::string& encryptedDataHex, const std::string& key, const std::string& iv);

    // --- Utility functions ---
    static std::string bytesToHex(const std::vector<BYTE>& bytes);
    static std::vector<BYTE> hexToBytes(const std::string& hex);
    // Overload for string
    static std::string bytesToHex(const BYTE* bytes, size_t len);


    // --- Existing Symmetric Key Operations using HCRYPTKEY (Kept for internal/other uses) ---
    std::vector<BYTE> encryptSymmetric(const std::string& data, HCRYPTKEY hKey); // Returns raw bytes
    std::string decryptSymmetric(const std::vector<BYTE>& data, HCRYPTKEY hKey); // Takes raw bytes
    void generateSymmetricKeyHandle(HCRYPTKEY* hKey, ALG_ID algorithm = CALG_AES_256);
    void exportSymmetricKeyHandle(HCRYPTKEY hKey, std::vector<BYTE>& keyBlob);
    HCRYPTKEY importSymmetricKeyHandle(const std::vector<BYTE>& keyBlob, ALG_ID algorithm = CALG_AES_256);


private:
    HCRYPTPROV hProv_;
    
    // Helper for AES with raw keys: imports key, sets IV, encrypts/decrypts, destroys imported key.
    HCRYPTKEY importAesKeyBytes(const std::vector<BYTE>& keyBytes, ALG_ID algId = CALG_AES_256);
    void setKeyIV(HCRYPTKEY hKey, const std::vector<BYTE>& iv);
};
