#pragma once
#include <iostream>
#include <queue>
#include <cstring>
#include <cstdint>
#include <string>

#include "platform-specific.h"

// Message Type Constants
const uint8_t BASE_MESSAGE = 0;
const uint8_t TEXT_MESSAGE = 1;
const uint8_t EVENT_MESSAGE = 2;
const uint8_t SNAPSHOT_MESSAGE = 3;
const uint8_t CLIENT_LIST_MESSAGE = 4;
const uint8_t CLIENT_ID_MESSAGE = 5;

// Client Information Structure
struct ClientInfo {
    uint8_t clientID;
    std::string ipAddress;
    uint16_t port;

    ClientInfo() : clientID(0), ipAddress(""), port(0) {}
    ClientInfo(uint8_t id, const std::string& ip, uint16_t p) : clientID(id), ipAddress(ip), port(p) {}
};

// Base class for messages
class BaseMessage {
public:
    uint8_t messageType;
    uint8_t senderID;
    std::queue<uint8_t> message;

    BaseMessage() : messageType(BASE_MESSAGE), senderID(0) {}
    BaseMessage(uint8_t type, uint8_t sender) : messageType(type), senderID(sender) {}

    static void serializeMessage(const BaseMessage& msg, std::vector<uint8_t>& buffer);
    static BaseMessage* deserializeMessage(const std::vector<uint8_t>& buffer);

    virtual ~BaseMessage() {}
};