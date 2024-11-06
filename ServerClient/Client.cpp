#include "Client.h"



int Client::start()
{
    std::string serverIP;

    std::cout << "Enter server IP address: ";
    std::cin >> serverIP;

    if (!connectToServer(serverIP)) {
        std::cerr << "Failed to connect to server.\n";
        return 1;
    }

    std::cin.ignore();

    return 0;
}

int Client::start(std::string serverIP)
{
    if (!connectToServer(serverIP)) {
        std::cerr << "Failed to connect to server: " << serverIP << "\n";
        return 1;
    }
    return 0;
}

void Client::stop()
{
    // Disconnect from the server
    disconnect();
}

bool Client::connectToServer(const std::string& serverIP) {
#ifdef _WIN32
    WSADATA wsData;
    WSAStartup(MAKEWORD(2, 2), &wsData);
#endif

    // Create a socket for the connection
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket.\n";
        return false; // Socket creation failed
    }

    // Set up server address structure
    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    inet_pton(AF_INET, serverIP.c_str(), &serverHint.sin_addr); // Correctly convert IP string to address

    // Attempt to connect to the server
    if (connect(serverSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        std::cerr << "Cannot connect to server.\n";
        return false; // Connection failed
    }

    isConnected = true; // Mark as connected

    // Start a thread to receive messages
    receiveThread = std::thread(&Client::receiveMessages, this);
    receiveThread.detach(); // Detach the thread

    std::cout << "Connected to server.\n";

    return true; // Connection successful
}
void Client::disconnect() {
    isConnected = false; // Mark as disconnected

    if (receiveThread.joinable())
        receiveThread.join(); // Join the thread
    else
        return;
    closesocket(serverSocket); // Close the socket
#ifdef _WIN32
    WSACleanup(); // Clean up Winsock on Windows
#endif
}


void Client::receiveMessages() {
    while (isConnected) {
        uint32_t msgSize;

        // Receive message size
        int bytesReceived = recv(serverSocket, (char*)&msgSize, sizeof(msgSize), 0);
        if (bytesReceived <= 0) {
            std::cerr << "Server is down or connection lost.\n"; // Notify the client
            notifyServerDown(); // Custom function to handle disconnection
            break; // Exit loop if no bytes received
        }
        msgSize = ntohl(msgSize); // Convert size back to host byte order

        std::vector<uint8_t> buffer(msgSize);
        size_t totalReceived = 0;

        // Receive the entire message
        while (totalReceived < msgSize) {
            bytesReceived = recv(serverSocket, (char*)buffer.data() + totalReceived, msgSize - totalReceived, 0);
            if (bytesReceived <= 0) {
                break; // Exit if no bytes received
            }
            totalReceived += bytesReceived; // Update total received
        }
        if (bytesReceived <= 0) {
            break; // Exit if no bytes received
        }

        // Deserialize the message
        BaseMessage* msg = BaseMessage::deserializeMessage(buffer);
        if (msg) {

            if (msg->messageType == CLIENT_LIST_MESSAGE) { // Handle client list update
                BaseMessage* clm = static_cast<BaseMessage*>(msg);
                updateClientList(clm->message);
            }
            else {
                sortMessageByType(msg); // Sort other message types
            }
            delete msg;
        }
    }

    disconnect(); // Ensure disconnection on exit
}
void Client::sendMessage(const BaseMessage& msg) {
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(msg, buffer); // Serialize the message

    uint32_t msgSize = htonl(buffer.size()); // Convert size to network byte order

    // Send message size and message data
    send(serverSocket, (char*)&msgSize, sizeof(msgSize), 0);
    send(serverSocket, (char*)buffer.data(), buffer.size(), 0);
}

void Client::sortMessageByType(BaseMessage* msg) {
    std::lock_guard<std::mutex> lock(messageMutex); // Lock for thread safety

    // Sort messages based on their type
    switch (msg->messageType) {
    case TEXT_MESSAGE: {
        BaseMessage* tm = static_cast<BaseMessage*>(msg);
        textMessages.push_back(*tm); // Store text message
        break;
    }
    case EVENT_MESSAGE: {
        BaseMessage* em = static_cast<BaseMessage*>(msg);
        eventMessages.push_back(*em); // Store event message
        break;
    }
    case SNAPSHOT_MESSAGE: {
        BaseMessage* sm = static_cast<BaseMessage*>(msg);
        snapshotMessages[sm->senderID] = *sm; // Store snapshot message by sender ID
        break;
    }
    case CLIENT_LIST_MESSAGE: {
        BaseMessage* cl = static_cast<BaseMessage*>(msg);
        updateClientList(cl->message); // Update client list with new IDs
        break;
    }
    case CLIENT_ID_MESSAGE: {
        BaseMessage* idMsg = static_cast<BaseMessage*>(msg);
        clientID = idMsg->senderID; // Store assigned client ID
        std::cout << "Assigned client ID: " << (int)clientID << std::endl; // Output assigned ID
        break;
    }
    }
}

void Client::updateClientList(const std::queue<uint8_t>& clientIDs) {
    std::lock_guard<std::mutex> lock(messageMutex); // Lock for thread safety

    // Clear the existing connected clients queue
    connectedClients = std::queue<uint8_t>();

    // Update the connected clients list with new data
    std::queue<uint8_t> tempQueue = clientIDs; // Create a copy of the input queue
    while (!tempQueue.empty()) {
        connectedClients.push(tempQueue.front()); // Add each client ID to the connected clients queue
        std::cout << std::to_string(tempQueue.front()) << std::endl; // Output each client ID
        tempQueue.pop(); // Remove the processed client ID
    }

    // Notify all listeners about the updated client list
    for (const auto& listener : listeners) {
        // Convert the queue to a vector for passing to the listener
        std::queue<uint8_t> clientIDVector;
        std::queue<uint8_t> tempForListener = connectedClients; // Create a copy for the listener
        while (!tempForListener.empty()) {
            clientIDVector.push(tempForListener.front());
            tempForListener.pop();
        }
        listener(clientIDVector); // Call each listener with the updated client IDs
    }
}

void Client::addListener(std::function<void(const std::queue<uint8_t>&)> listener) {
    listeners.push_back(listener); // Add a new listener to the list
}

void Client::notifyServerDown() {
    isConnected = false; // Set connection flag to false
    std::cout << "Disconnected from server. Please check the server status.\n";
    updateClientList(std::queue<uint8_t>());
}