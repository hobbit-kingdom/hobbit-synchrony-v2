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
#include <memory>
#include <algorithm>

#include "platform-specific.h"
#include "Message.h"
#include "../LogSystem/LogManager.h"
#define PORT 54000

class Client {
public:
    Client();
    Client(std::string serverIP);
    ~Client() { stop(); }

    int start();
    int start(std::string serverIP);
    void stop();

    bool connectToServer(const std::string& serverIP);
    void disconnect();

    void updateClientList(const std::queue<uint8_t>& data);
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
    std::map<uint8_t, BaseMessage> snapMessage() { return snapshotMessages; }

    void popFrontTextMessage() { textMessages.pop_front(); }
    void popFrontEventMessage() { eventMessages.pop_front(); }
	uint32_t eventMessagesSize() { return eventMessages.size(); }
    void clearSnapMessage() { snapshotMessages.clear(); }

    uint8_t getClientID() { return clientID; }
    const std::map<uint8_t, ClientInfo>& getConnectedClients() const { return connectedClientsInfo; }

    void notifyServerDown();

private:
    LogOption::Ptr logOption_;
    SOCKET serverSocket = 0;
    std::thread receiveThread;
    std::mutex messageMutex;
    bool isConnected;
    uint8_t clientID = -1;
    std::map<uint8_t, ClientInfo> connectedClientsInfo;
    std::deque<BaseMessage> textMessages;
    std::deque<BaseMessage> eventMessages;
    std::map<uint8_t, BaseMessage> snapshotMessages;
    std::vector<std::function<void(const std::queue<uint8_t>&)>> listeners;

    void receiveMessages();
    void sortMessageByType(BaseMessage* msg);
};