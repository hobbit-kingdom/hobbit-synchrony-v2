#include "Message.h"


// Serialization Function
void BaseMessage::serializeMessage(const BaseMessage &msg, std::vector<uint8_t>& buffer) {
    // Add the message type to the buffer
    buffer.push_back(msg.messageType);
    // Add the sender ID to the buffer
    buffer.push_back(msg.senderID);

    // Add the message content to the buffer
    std::queue<uint8_t> tempQueue = msg.message; // Copy the queue to a temporary variable
    while (!tempQueue.empty()) {
        buffer.push_back(tempQueue.front());
        tempQueue.pop();
    }
}

// Deserialization Function
BaseMessage* BaseMessage::deserializeMessage(const std::vector<uint8_t>& buffer) {
    // Check if the buffer has at least 2 bytes (for message type and sender ID)
    if (buffer.size() < 2) return nullptr;

    uint8_t messageType = buffer[0]; // Get the message type from the buffer
    uint8_t senderID = buffer[1]; // Get the sender ID from the buffer

    // Create a new BaseMessage object
    BaseMessage* msg = new BaseMessage(messageType, senderID);

    // Copy the remaining data into the message queue
    for (size_t i = 2; i < buffer.size(); ++i) {
        msg->message.push(buffer[i]);
    }

    return msg; // Return the new BaseMessage object
}
