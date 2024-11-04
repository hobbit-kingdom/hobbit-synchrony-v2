#ifndef SERVER_H
#define SERVER_H
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <cstring>
#include <cstdint>

#include "platform-specific.h"
#include "Message.h"
#define PORT 54000

// Client Handler Struct
struct ClientHandler {
    SOCKET socket;
    uint8_t clientID;
    std::thread thread;
};

class Server {


public:
    Server() : nextClientID(1), isRunning(true) {}
    ~Server() { stop(); }

    void start();
    void stop();

    void broadcastMessage(const BaseMessage &msg, uint8_t excludeID = 0);
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

