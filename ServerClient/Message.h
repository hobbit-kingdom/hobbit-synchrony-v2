#pragma once
#include <iostream>
#include <queue>
#include <cstring>
#include <cstdint>

#include "platform-specific.h"
// Message Type Constants
const uint8_t BASE_MESSAGE = 0; // Base message type
const uint8_t TEXT_MESSAGE = 1; // Base message type
const uint8_t EVENT_MESSAGE = 2; // Base message type
const uint8_t SNAPSHOT_MESSAGE = 3; // Base message type
const uint8_t CLIENT_LIST_MESSAGE = 4; // Base message type
const uint8_t CLIENT_ID_MESSAGE = 5; // Base message type

// Base class for messages
class BaseMessage {
public:
    uint8_t messageType;
    uint8_t senderID;
    std::queue<uint8_t> message;

    // Default constructor
    BaseMessage() : messageType(BASE_MESSAGE), senderID(0) {}

    // Constructor that initializes the message type and sender ID
    BaseMessage(uint8_t type, uint8_t sender)
        : messageType(type), senderID(sender) {}

    // Static function to serialize a message into a buffer
    static void serializeMessage(const BaseMessage &msg, std::vector<uint8_t>& buffer);

    // Static function to deserialize a message from a buffer
    static BaseMessage* deserializeMessage(const std::vector<uint8_t>& buffer);

    // Virtual destructor
    virtual ~BaseMessage() {}
};
