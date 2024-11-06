#pragma once

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <cstring>
#include <cstdint>
#include <string>
#include <limits>
#include <optional>
#include <set>
#include <functional>
#include <deque>
#include <cassert>

#include "platform-specific.h"
#include "Message.h"

#define PORT 54000

class Client {
public:
    Client() : isConnected(false) {}
    Client(std::string serverIP) : isConnected(false) {
        if (!connectToServer(serverIP)) {
            std::cerr << "Failed to connect to server.\n";
        }
    }
    ~Client() { stop(); }

    int start();
    int start(std::string serverIP);
    void stop();

    bool connectToServer(const std::string& serverIP);
    void disconnect();

    void updateClientList(const std::queue<uint8_t>& clientIDs);
    void addListener(std::function<void(const std::queue<uint8_t>&)> listener);

    void sendMessage(const BaseMessage& msg);

    BaseMessage frontTextMessage() { 
        if (textMessages.size() > 0)
            return textMessages.front();
        else
            return BaseMessage(-1, -1);
    }
    BaseMessage frontEventMessage() {
        if (eventMessages.size() > 0)
            return eventMessages.front(); 
        else
            return BaseMessage(-1, -1);
    }
    std::map<uint8_t, BaseMessage> snapMessage(){ return snapshotMessages; }


    void popFrontTextMessage() { textMessages.pop_front(); }
    void popFrontEventMessage() { eventMessages.pop_front(); }
    void clearSnapMessage() { snapshotMessages.clear(); }
   

    uint8_t getClientID() { return clientID; }
    std::queue<uint8_t> getConnectedClients() { return connectedClients; }
    void notifyServerDown();
private:
    SOCKET serverSocket = 0;

    std::thread receiveThread;
    std::mutex messageMutex; // Mutex for thread-safe access to message containers

    bool isConnected;

    uint8_t clientID = -1;
    std::queue<uint8_t> connectedClients; // Set of client IDs

    std::deque<BaseMessage> textMessages;
    std::deque<BaseMessage> eventMessages;
    std::map<uint8_t, BaseMessage> snapshotMessages;


    void receiveMessages();
    void sortMessageByType(BaseMessage* msg);

    std::vector<std::function<void(const std::queue<uint8_t>&)>> listeners; // Listeners
};

