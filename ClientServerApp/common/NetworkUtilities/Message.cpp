#include "Message.h"
#include "WinCrypto.h" // For WinCrypto methods
#include <sstream>     // For std::stringstream
#include <stdexcept>   // For std::runtime_error, std::invalid_argument, std::out_of_range

// Define static const members
const std::string Message::PART_DELIMITER = "::PART::";
const std::string Message::KEY_BODY_DELIMITER = "::KEYBODY::";
const std::string Message::KEY_IV_DELIMITER = "::IV::";

// Helper to convert MessageType to string
std::string messageTypeToString(Message::MessageType type) {
    return std::to_string(static_cast<uint32_t>(type));
}

// Helper to convert string to MessageType
Message::MessageType stringToMessageType(const std::string& s) {
    try {
        return static_cast<Message::MessageType>(std::stoul(s));
    } catch (const std::exception& e) {
        // std::cerr << "Error converting string to MessageType: " << e.what() << std::endl;
        return Message::MessageType::UNDEFINED; // Or throw
    }
}

// Constructor for creating a message to be sent
Message::Message(MessageType type, const WinCrypto::PublicKey& recipientPubKey, 
                 const std::string& sender, const std::string& receiver)
    : type_(type), 
      sender_(sender), 
      receiver_(receiver), 
      recipientPublicKey_(recipientPubKey), 
      myPrivateKey_(NULL) // Not used for sending
{
}

// Constructor for parsing a received raw message string
Message::Message(const std::string& rawReceivedMessage, HCRYPTKEY hMyPrivateKey, WinCrypto& crypto)
    : myPrivateKey_(hMyPrivateKey) 
{
    size_t keyBodyPos = rawReceivedMessage.find(KEY_BODY_DELIMITER);
    if (keyBodyPos == std::string::npos) {
        throw std::runtime_error("Invalid raw message format: KEY_BODY_DELIMITER not found.");
    }

    encryptedSessionKeyHex_ = rawReceivedMessage.substr(0, keyBodyPos);
    encryptedMessageBodyHex_ = rawReceivedMessage.substr(keyBodyPos + KEY_BODY_DELIMITER.length());

    if (encryptedSessionKeyHex_.empty() || encryptedMessageBodyHex_.empty()) {
        throw std::runtime_error("Invalid raw message format: Empty key or body part.");
    }
    
    // Decrypt session key and IV
    std::string sessionKeyAndIvCombined = crypto.decryptWithPrivateKey(encryptedSessionKeyHex_, myPrivateKey_);
    
    size_t keyIvPos = sessionKeyAndIvCombined.find(KEY_IV_DELIMITER);
    if (keyIvPos == std::string::npos) {
        throw std::runtime_error("Invalid decrypted session key format: KEY_IV_DELIMITER not found.");
    }
    plaintextSessionKey_ = sessionKeyAndIvCombined.substr(0, keyIvPos);
    iv_ = sessionKeyAndIvCombined.substr(keyIvPos + KEY_IV_DELIMITER.length());

    if (plaintextSessionKey_.empty() || iv_.empty()) {
        throw std::runtime_error("Invalid decrypted session key format: Empty session key or IV.");
    }

    // Decrypt the main message body
    std::string decryptedCoreData = crypto.aesDecrypt(encryptedMessageBodyHex_, plaintextSessionKey_, iv_);
    
    // Parse the decrypted core data
    deserializeCoreData(decryptedCoreData);
}

void Message::pushBody(const std::string& part) {
    messageBodyParts_.push_back(part);
}

std::string Message::serializeCoreData() const {
    std::string coreDataStr = messageTypeToString(type_);
    coreDataStr += PART_DELIMITER + sender_;
    coreDataStr += PART_DELIMITER + receiver_;

    for (const auto& part : messageBodyParts_) {
        coreDataStr += PART_DELIMITER + part;
    }
    return coreDataStr;
}

void Message::deserializeCoreData(const std::string& decryptedCoreData) {
    messageBodyParts_.clear();
    std::stringstream ss(decryptedCoreData);
    std::string segment;
    int partIndex = 0;

    // Type
    if (std::getline(ss, segment, PART_DELIMITER[0])) { // Assuming delimiter is single char for stringstream or use find
         // More robust splitting for multi-character delimiters:
    } //This simple ss split won't work well with multi-char delimiter. Let's use find.

    size_t start = 0;
    size_t end = decryptedCoreData.find(PART_DELIMITER);
    
    auto get_segment = [&](size_t& s, size_t& e, const std::string& data) {
        if (e == std::string::npos) { // Last segment
            std::string seg = data.substr(s);
            s = data.length(); // Move start to end
            return seg;
        }
        std::string seg = data.substr(s, e - s);
        s = e + PART_DELIMITER.length();
        e = data.find(PART_DELIMITER, s);
        return seg;
    };


    if (start > decryptedCoreData.length()) throw std::runtime_error("Error parsing core data: unexpected end of string.");
    type_ = stringToMessageType(get_segment(start, end, decryptedCoreData));
    partIndex++;

    if (start > decryptedCoreData.length()) throw std::runtime_error("Error parsing core data: missing sender.");
    sender_ = get_segment(start, end, decryptedCoreData);
    partIndex++;
    
    if (start > decryptedCoreData.length()) throw std::runtime_error("Error parsing core data: missing receiver.");
    receiver_ = get_segment(start, end, decryptedCoreData);
    partIndex++;

    while(start < decryptedCoreData.length()) {
        messageBodyParts_.push_back(get_segment(start, end, decryptedCoreData));
        partIndex++;
    }
    // If the last part was empty and followed by a delimiter, it might add an empty string.
    // This depends on how sender formats it. If sender always adds parts, this is fine.
}


void Message::generateEncryptedMessageForSending(WinCrypto& crypto) {
    // 1. Generate plaintext AES session key and IV
    plaintextSessionKey_ = crypto.generateRandomSymmetricKey(); // Default 32 bytes (256-bit)
    iv_ = crypto.generateRandomSymmetricKey(16); // AES IV is 16 bytes (128-bit)

    if (plaintextSessionKey_.length() != 32) {
        throw std::runtime_error("Generated session key is not 32 bytes long.");
    }
    if (iv_.length() != 16) {
        throw std::runtime_error("Generated IV is not 16 bytes long.");
    }

    // 2. Combine session key and IV, then encrypt with RSA public key
    std::string sessionKeyAndIvCombined = plaintextSessionKey_ + KEY_IV_DELIMITER + iv_;
    encryptedSessionKeyHex_ = crypto.encryptWithPublicKey(sessionKeyAndIvCombined, recipientPublicKey_);

    // 3. Serialize core data (type, sender, receiver, body parts)
    std::string coreDataToEncrypt = serializeCoreData();

    // 4. Encrypt core data with AES
    encryptedMessageBodyHex_ = crypto.aesEncrypt(coreDataToEncrypt, plaintextSessionKey_, iv_);
}

std::string Message::getMessageToSend() const {
    if (encryptedSessionKeyHex_.empty() || encryptedMessageBodyHex_.empty()) {
        throw std::runtime_error("Message has not been encrypted yet. Call generateEncryptedMessageForSending().");
    }
    return encryptedSessionKeyHex_ + KEY_BODY_DELIMITER + encryptedMessageBodyHex_;
}

// Accessors
Message::MessageType Message::getMessageType() const {
    return type_;
}

std::string Message::getSender() const {
    return sender_;
}

std::string Message::getReceiver() const {
    return receiver_;
}

const std::vector<std::string>& Message::getMessageBodyParts() const {
    return messageBodyParts_;
}
