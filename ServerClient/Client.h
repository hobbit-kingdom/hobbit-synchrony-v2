#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <cstring>
#include <cstdint>
#include <string>
#include <limits> // Required for std::numeric_limits
#include <optional> // Include the optional header
#include <set>
#include <functional> // For std::function

// Platform-specific includes
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
    void sendMessage(BaseMessage* msg);

    std::optional<TextMessage> getTextMessage();
    std::optional<EventMessage> getEventMessage();

    std::map<uint8_t, SnapshotMessage> getSnapshotMessages();

    void updateClientList(const std::vector<uint8_t>& clientIDs);

    // Function to add a listener
    void addListener(std::function<void(const std::vector<uint8_t>&)> listener);

    std::vector<uint8_t> connectedClients; // Set of client IDs
    uint8_t getClientID()
    {
        return clientID;
    }
private:
    SOCKET serverSocket;
    std::thread receiveThread;
    bool isConnected;
    uint8_t clientID;

    std::vector<TextMessage> textMessages;
    std::vector<EventMessage> eventMessages;
    std::map<uint8_t, SnapshotMessage> snapshotMessages;

    std::mutex messageMutex; // Mutex for thread-safe access to message containers

    void receiveMessages();
    void sortMessageByType(BaseMessage* msg);

    // Member variable for listeners
    std::vector<std::function<void(const std::vector<uint8_t>&)>> listeners;
};

