#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include "WinCrypto.h" // For PublicKey, PrivateKey (HCRYPTKEY), and WinCrypto methods

// Forward declaration
// class WinCrypto; // Already in WinCrypto.h

class Message {
public:
    enum class MessageType : uint32_t { // Keep enum for clarity
        UNDEFINED = 0,
        TEXT,
        FILE,
        GREETING,
        ERROR_MSG,
        EVENT, // Added for client-sent events
        SNAP,  // Added for client-sent snapshots
        // Add other types as needed
    };

    // Constructor for creating a message to be sent
    // Recipient's public key is needed to encrypt the session key.
    Message(MessageType type, const WinCrypto::PublicKey& recipientPubKey, 
            const std::string& sender = "DefaultSender", const std::string& receiver = "DefaultReceiver");

    // Constructor for parsing a received raw message string
    // Your own private key is needed to decrypt the session key.
    Message(const std::string& rawReceivedMessage, HCRYPTKEY hMyPrivateKey, WinCrypto& crypto);


    void pushBody(const std::string& part);
    
    // Generates encryptedSessionKey and encryptedMessageBody
    void generateEncryptedMessageForSending(WinCrypto& crypto); 
    
    // Returns the string to be sent over the network
    std::string getMessageToSend() const;

    // Accessors (populated after construction or decryption)
    MessageType getMessageType() const;
    std::string getSender() const;
    std::string getReceiver() const;
    const std::vector<std::string>& getMessageBodyParts() const; // Direct access to parsed body parts


// private: // Making members public for easier debugging during this refactor, will make private later
public: 
    MessageType type_ = MessageType::UNDEFINED;
    std::string sender_;
    std::string receiver_;
    std::vector<std::string> messageBodyParts_; // Stores actual content parts

    // Data for encryption/decryption process
    WinCrypto::PublicKey recipientPublicKey_; // For sending
    HCRYPTKEY myPrivateKey_ = NULL;       // For receiving
    
    std::string plaintextSessionKey_;     // Temporary, raw bytes as string
    std::string iv_;                      // Temporary, raw bytes as string

    std::string encryptedSessionKeyHex_;  // Hex-encoded, RSA encrypted (session key + IV)
    std::string encryptedMessageBodyHex_; // Hex-encoded, AES encrypted (type + sender + receiver + body parts)

    // Delimiters
    static const std::string PART_DELIMITER;
    static const std::string KEY_BODY_DELIMITER;
    static const std::string KEY_IV_DELIMITER;

    // Helper to serialize header (type, sender, receiver) and body parts for AES encryption
    std::string serializeCoreData() const;
    // Helper to deserialize core data after AES decryption
    void deserializeCoreData(const std::string& decryptedCoreData);
};
