#ifndef SERVER_H
#define SERVER_H
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <cstring>
#include <cstdint>
#include <string>

#include "platform-specific.h"
#include "Message.h"
#include "../LogSystem/LogManager.h"
#define PORT 54000

struct ClientHandler {
    SOCKET socket;
    uint8_t clientID;
    std::string ipAddress;
    uint16_t port;
    std::thread thread;
};

class Server {
    LogOption::Ptr logOption_;

public:
    Server() : nextClientID(1), isRunning(true), logOption_(LogManager::Instance().CreateLogOption("SERVER")) {}
    ~Server() { stop(); }

    void start();
    void stop();
    bool getIsRunning() { return isRunning; };
    void broadcastMessage(const BaseMessage& msg, uint8_t excludeID = 0);

private:
    SOCKET listeningSocket;
    std::vector<ClientHandler*> clients;
    uint8_t nextClientID;
    std::mutex clientsMutex;
    bool isRunning;

    void acceptClients();
    void handleClient(ClientHandler* clientHandler);
    void notifyClients();
};

#endif // SERVER_H