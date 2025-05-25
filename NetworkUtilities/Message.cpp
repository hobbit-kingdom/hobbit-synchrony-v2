#include "Message.h"
#include <iostream> // For std::cerr
#include <stdexcept> // For std::runtime_error

// Helper function to convert MessageType to string
std::string messageTypeToString(MessageType type) {
    switch (type) {
    case MessageType::GREETING: return "GREETING";
    case MessageType::EVENT:    return "EVENT";
    case MessageType::SNAP:     return "SNAP";
    default:                    return "UNKNOWN";
    }
}

// Constructor for sending (requires recipient's public key)
Message::Message(MessageType type, const WinCrypto::PublicKey& recipientPublicKey)
    : type(type), messageSize(0) {
    try {
        // 1. Generate a new, random symmetric session key (e.g., 32-byte for AES-256)
        this->symmetricSessionKey = WinCrypto::generateRandomSymmetricKey(32);
        // 2. Encrypt this symmetric session key using the recipient's public key
        this->encryptedSessionKey = WinCrypto::encryptWithPublicKey(this->symmetricSessionKey, recipientPublicKey);
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Error in Message constructor (sending): " << e.what() << std::endl;
        // Handle error appropriately, e.g., throw, set error state, etc.
    }
}

// Constructor for receiving (requires your private key)
Message::Message(const std::string& rawReceivedMessage, const WinCrypto::PrivateKey& myPrivateKey)
    : type(MessageType::UNKNOWN), messageSize(0) {
    try {
        // --- Parsing the raw received message which contains hex-encoded parts ---
        std::stringstream ss(rawReceivedMessage);
        std::string line;
        std::string hex_encrypted_key;
        std::string hex_encrypted_body;

        while (std::getline(ss, line)) {
            if (line.rfind("TYPE:", 0) == 0) {
                std::string type_str = line.substr(5);
                if (type_str == "GREETING") type = MessageType::GREETING;
                else if (type_str == "EVENT") type = MessageType::EVENT;
                else if (type_str == "SNAP") type = MessageType::SNAP;
            }
            else if (line.rfind("ENCRYPTED_KEY:", 0) == 0) {
                hex_encrypted_key = line.substr(14);
            }
            else if (line.rfind("ENCRYPTED_BODY:", 0) == 0) {
                hex_encrypted_body = line.substr(15);
            }
            // We ignore SIZE for now as it's for external parsing, not decryption logic
        }

        // Convert hex-encoded parts back to raw bytes for decryption
        this->encryptedSessionKey = hexToBytes(hex_encrypted_key);
        this->encryptedMessageBody = hexToBytes(hex_encrypted_body);

        // 1. Decrypt the session key using your private key
        this->symmetricSessionKey = WinCrypto::decryptWithPrivateKey(this->encryptedSessionKey, myPrivateKey);

        // 2. Decrypt the message body using the now-decrypted symmetric session key
        std::string decryptedBodyContent = WinCrypto::aesDecrypt(this->encryptedMessageBody, this->symmetricSessionKey);

        // 3. Parse the decrypted body content back into messageBodyParts
        std::stringstream bodyStream(decryptedBodyContent);
        std::string bodyLine;
        bool in_body = false;
        while (std::getline(bodyStream, bodyLine)) {
            if (bodyLine == "BODY_START") {
                in_body = true;
                continue;
            }
            if (bodyLine == "BODY_END") {
                in_body = false;
                break;
            }
            if (in_body) {
                messageBodyParts.push_back(bodyLine);
            }
        }
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Error in Message constructor (receiving): " << e.what() << std::endl;
        // Handle error appropriately
        // Consider clearing messageBodyParts or setting a "corrupted" flag
    }
}

// Overload for C-style strings
void Message::pushBody(const char* data) {
    messageBodyParts.push_back(data);
    updateMessageSize();
}

// Generate the encrypted message for sending
void Message::generateEncryptedMessageForSending() {
    std::stringstream bodyContentStream;
    bodyContentStream << "BODY_START\n";
    for (const auto& part : messageBodyParts) {
        bodyContentStream << part << "\n";
    }
    bodyContentStream << "BODY_END";

    std::string rawBodyContent = bodyContentStream.str();
    // Encrypt the *body content* using the symmetric session key
    try {
        encryptedMessageBody = WinCrypto::aesEncrypt(rawBodyContent, symmetricSessionKey);
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Error during AES encryption: " << e.what() << std::endl;
        encryptedMessageBody = ""; // Clear on error
    }
}

// Get the full message string to send over the wire
std::string Message::getMessageToSend() const {
    std::stringstream fullMessageStream;
    fullMessageStream << "TYPE:" << messageTypeToString(type) << "\n";
    // Convert encryptedSessionKey to hex for display/transport
    fullMessageStream << "ENCRYPTED_KEY:" << bytesToHex(encryptedSessionKey) << "\n";
    fullMessageStream << "SIZE:" << messageSize << "\n";
    // Convert encryptedMessageBody to hex for display/transport
    fullMessageStream << "ENCRYPTED_BODY:" << bytesToHex(encryptedMessageBody);
    return fullMessageStream.str();
}

// Helper to get the full raw message body (for display/debugging before symmetric encryption)
std::string Message::getFullMessageRawBody() const {
    std::stringstream ss;
    ss << "--- HEADER ---\n";
    ss << "Type: " << messageTypeToString(type) << "\n";
    // Display encryptedSessionKey as hex
    ss << "Encrypted Session Key (with PubKey): " << bytesToHex(encryptedSessionKey) << "\n";
    ss << "Symmetric Session Key (unencrypted, for debug): " << "OMITTED_FOR_SECURITY_IN_DEBUG" << "\n"; // Don't print actual key in debug
    ss << "Message Size: " << messageSize << " bytes\n";
    ss << "--- BODY (Before Symmetric Encryption) ---\n";
    for (const auto& part : messageBodyParts) {
        ss << part << "\n";
    }
    return ss.str();
}

// Cout operator overload for easy display
std::ostream& operator<<(std::ostream& os, const Message& msg) {
    os << "\n----- MESSAGE DETAILS -----\n";
    os << "--- Message Body (Raw Input) ---\n";
    for (const auto& part : msg.messageBodyParts) {
        os << part << "\n";
    }
    os << "------------------------\n";
    os << "--- Full Message (Raw Header + Encrypted Body) ---\n";
    os << msg.getMessageToSend() << "\n"; // Show the full message as it would be sent (with hex encoding)
    os << "------------------------\n";
    return os;
}

// Private helper to update messageSize
void Message::updateMessageSize() {
    messageSize = 0;
    for (const auto& part : messageBodyParts) {
        messageSize += part.length();
    }
    // Add an estimate for header size for more realistic 'messageSize'
    messageSize += (messageTypeToString(type).length() + 5); // "TYPE:" + type_str + '\n'
    messageSize += (bytesToHex(encryptedSessionKey).length() + 14); // "ENCRYPTED_KEY:" + key_str + '\n' (use hex length)
    messageSize += (std::to_string(messageSize).length() + 6); // "SIZE:" + size_str + '\n'
    messageSize += (std::string("ENCRYPTED_BODY:\n").length());
    messageSize += (std::string("BODY_START\n").length()); // These are part of the content that gets sym-encrypted
    messageSize += (std::string("BODY_END").length());
    messageSize += (messageBodyParts.size() * 1); // For the newlines between body parts
}