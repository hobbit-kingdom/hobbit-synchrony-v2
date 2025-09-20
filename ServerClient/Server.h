#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <string>
#include <deque>
#include <cstdint>
#include <memory>

#include "platform-specific.h"
#include "Message.h"
#include "../LogSystem/LogManager.h"
#include "TLS.h"

#define PORT 54000

struct ClientHandler {
    SOCKET socket;
    uint8_t clientID;
    std::string ipAddress;
    uint16_t port;
    std::thread thread;

    // TLS handle
    SSL* ssl = nullptr;
};

class Server {
public:
    Server();
    ~Server() { stop(); }

    bool start();
    void stop();

    void processMessage(const BaseMessage& msg, ClientHandler* clientHandler);

    bool getIsRunning() { return isRunning; }

    void broadcastMessage(const BaseMessage& msg, uint8_t excludeID = 0);
    bool sendToClient(uint8_t clientID, const BaseMessage& msg);

    // No custom enum: send a generic EVENT_MESSAGE to everyone
    void notifyClients();

private:
    LogOption::Ptr logOption_;
    SOCKET listeningSocket = INVALID_SOCKET;
    std::vector<ClientHandler*> clients;
    std::mutex clientsMutex;
    bool isRunning = false;
    uint8_t nextID = 1;

    TLSServer tls_;

    void acceptClients();
    void handleClient(ClientHandler* clientHandler);

    uint8_t allocateClientID() {
        // Simple ID allocator (1..254)
        if (nextID == 0 || nextID == 255) nextID = 1;
        return nextID++;
    }
};
