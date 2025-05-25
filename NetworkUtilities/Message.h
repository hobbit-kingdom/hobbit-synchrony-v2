#pragma once

#include <string>
#include <vector>
#include <sstream> // For std::stringstream
#include <iostream> // For std::ostream
#include "WinCrypto.h" // Include our separated CryptoAPI wrapper

// Enum for Message Types
enum class MessageType {
    GREETING,
    EVENT,
    SNAP,
    UNKNOWN
};

// Helper function to convert MessageType to string
std::string messageTypeToString(MessageType type);

class Message {
public:
    // Header members
    MessageType type;
    std::string encryptedSessionKey; // This will hold the session key, encrypted with recipient's public key
    size_t messageSize;              // Size of the body + header information if needed for external parsing

    // Body members
    std::vector<std::string> messageBodyParts; // Store body parts as strings

    // Encrypted full message body (encrypted with the symmetric session key)
    std::string encryptedMessageBody;
    std::string symmetricSessionKey; // The ephemeral symmetric key used for *this* message

    // Constructor for sending (requires recipient's public key)
    Message(MessageType type, const WinCrypto::PublicKey& recipientPublicKey);

    // Constructor for receiving (requires your private key)
    Message(const std::string& rawReceivedMessage, const WinCrypto::PrivateKey& myPrivateKey);

    // Function to push information into the body (template)
    template <typename T>
    void pushBody(const T& data) {
        std::stringstream ss;
        ss << data;
        messageBodyParts.push_back(ss.str());
        updateMessageSize();
    }

    // Overload for C-style strings
    void pushBody(const char* data);

    // Generate the encrypted message for sending
    void generateEncryptedMessageForSending();

    // Get the full message string to send over the wire
    std::string getMessageToSend() const;

    // Helper to get the full raw message body (for display/debugging before symmetric encryption)
    std::string getFullMessageRawBody() const;

    // Cout operator overload for easy display
    friend std::ostream& operator<<(std::ostream& os, const Message& msg);

private:
    // Updates messageSize based on current body parts (before symmetric encryption)
    void updateMessageSize();
};