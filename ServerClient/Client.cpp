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

    std::cin.ignore(); // Clear newline character from input buffer

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
    disconnect();
}

bool Client::connectToServer(const std::string& serverIP) {
#ifdef _WIN32
    WSADATA wsData;
    WSAStartup(MAKEWORD(2, 2), &wsData);
#endif

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating socket.\n";
        return false;
    }

    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    inet_pton(AF_INET, serverIP.c_str(), &serverHint.sin_addr);

    if (connect(serverSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        std::cerr << "Cannot connect to server.\n";
        return false;
    }

    isConnected = true;

    // Start receive thread
    receiveThread = std::thread(&Client::receiveMessages, this);
    receiveThread.detach();

    std::cout << "Connected to server.\n";

    return true;
}

void Client::disconnect() {
    isConnected = false;
    closesocket(serverSocket);
#ifdef _WIN32
    WSACleanup();
#endif
}

void Client::sendMessage(BaseMessage* msg) {
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(msg, buffer);

    uint32_t msgSize = htonl(buffer.size());

    send(serverSocket, (char*)&msgSize, sizeof(msgSize), 0);
    send(serverSocket, (char*)buffer.data(), buffer.size(), 0);
}

void Client::receiveMessages() {
    while (isConnected) {
        uint32_t msgSize;
        int bytesReceived = recv(serverSocket, (char*)&msgSize, sizeof(msgSize), 0);
        if (bytesReceived <= 0) {
            break;
        }
        msgSize = ntohl(msgSize);

        std::vector<uint8_t> buffer(msgSize);
        size_t totalReceived = 0;
        while (totalReceived < msgSize) {
            bytesReceived = recv(serverSocket, (char*)buffer.data() + totalReceived, msgSize - totalReceived, 0);
            if (bytesReceived <= 0) {
                break;
            }
            totalReceived += bytesReceived;
        }
        if (bytesReceived <= 0) {
            break;
        }


        BaseMessage* msg = BaseMessage::deserializeMessage(buffer);
        if (msg) {
            if (msg->messageType == CLIENT_LIST_MESSAGE) { // Handle client list update
                ClientListMessage* clm = static_cast<ClientListMessage*>(msg);
                updateClientList(clm->clientIDs); // Update client list
            }
            else {
                sortMessageByType(msg);
            }
            delete msg;
        }
    }

    disconnect();
}

void Client::sortMessageByType(BaseMessage* msg) {
    std::lock_guard<std::mutex> lock(messageMutex); // Lock for thread safety

    switch (msg->messageType) {
    case TEXT_MESSAGE: {
        TextMessage* tm = static_cast<TextMessage*>(msg);
        textMessages.push_back(*tm);
        break;
    }
    case EVENT_MESSAGE: {
        EventMessage* em = static_cast<EventMessage*>(msg);
        eventMessages.push_back(*em);
        break;
    }
    case SNAPSHOT_MESSAGE: {
        SnapshotMessage* sm = static_cast<SnapshotMessage*>(msg);
        snapshotMessages[sm->senderID] = *sm;
        break;
    }
    case CLIENT_LIST_MESSAGE: {
        ClientListMessage* cl = static_cast<ClientListMessage*>(msg);
        updateClientList(cl->clientIDs);
        break;
    }
    case CLIENT_ID_MESSAGE: {
        ClientIDMessage* idMsg = static_cast<ClientIDMessage*>(msg);
        clientID = idMsg->clientID;
        std::cout << "Assigned client ID: " << (int)clientID << std::endl;
        break;
    }
    }
}

// Functions to get messages
std::optional<TextMessage> Client::getTextMessage() {
    std::lock_guard<std::mutex> lock(messageMutex); // Lock for thread-safe access
    if (textMessages.empty()) {
        return std::nullopt; // Return an empty optional if no messages
    }
    TextMessage message = std::move(textMessages.front()); // Get the first element
    textMessages.erase(textMessages.begin()); // Remove the first element
    return message; // Return the message
}

std::optional<EventMessage> Client::getEventMessage() {
    std::lock_guard<std::mutex> lock(messageMutex); // Lock for thread-safe access
    if (eventMessages.empty()) {
        return std::nullopt; // Return an empty optional if no events
    }
    EventMessage event = std::move(eventMessages.front()); // Get the first element
    eventMessages.erase(eventMessages.begin()); // Remove the first element
    return event; // Return the event
}

// Modified function to return and delete all snapshots
std::map<uint8_t, SnapshotMessage> Client::getSnapshotMessages() {
    std::lock_guard<std::mutex> lock(messageMutex); // Lock for thread-safe access
    std::map<uint8_t, SnapshotMessage> snapshots = std::move(snapshotMessages); // Move the map to a local variable
    snapshotMessages.clear(); // Clear the original map to delete the snapshots
    return snapshots; // Return the moved snapshots
}

void Client::updateClientList(const std::vector<uint8_t>& clientIDs) {
    std::lock_guard<std::mutex> lock(messageMutex);


    connectedClients = std::vector<uint8_t>(clientIDs.begin(), clientIDs.end()); // Update the set with new data
    std::cout << "*Client List Updated*" << std::endl;
    std::cout << "*New List: " << std::endl;
    for (auto e : connectedClients)
    {
        std::cout << std::to_string(e) << std::endl;
    }

    // Notify all listeners about the updated client list
    for (const auto& listener : listeners) {
        listener(clientIDs);
    }
}

void Client::addListener(std::function<void(const std::vector<uint8_t>&)> listener) {
    listeners.push_back(listener);
}