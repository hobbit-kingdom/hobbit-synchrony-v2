#include "Client.h"

Client::Client() : isConnected(false), logOption_(LogManager::Instance().CreateLogOption("CLIENT")) {}
Client::Client(std::string serverIP) : isConnected(false), logOption_(LogManager::Instance().CreateLogOption("CLIENT")) {
    if (!connectToServer(serverIP)) {
        std::cerr << "Failed to connect to server.\n";
    }
}

int Client::start() {
    std::string serverIP;
    logOption_->LogMessage(LogLevel::Log_Prompt, "Enter server IP address: ");
    std::cin >> serverIP;

    if (!connectToServer(serverIP)) {
        logOption_->LogMessage(LogLevel::Log_Warning, "Failed to connect to server.");
        return 1;
    }

    std::cin.ignore();
    return 0;
}

int Client::start(std::string serverIP) {
    if (!connectToServer(serverIP)) {
        logOption_->LogMessage(LogLevel::Log_Error, "Failed to connect to server", serverIP);
        return 1;
    }
    return 0;
}

void Client::stop() {
    disconnect();
}

bool Client::connectToServer(const std::string& serverIP) {
#ifdef _WIN32
    WSADATA wsData;
    WSAStartup(MAKEWORD(2, 2), &wsData);
#endif

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Issues creating socket");
        return false;
    }

    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    inet_pton(AF_INET, serverIP.c_str(), &serverHint.sin_addr);

    if (connect(serverSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Cannot connect to server");
        return false;
    }

    isConnected = true;
    receiveThread = std::thread(&Client::receiveMessages, this);
    receiveThread.detach();
    logOption_->LogMessage(LogLevel::Log_Info, "", "Connected to server");
    return true;
}

void Client::disconnect() {
    isConnected = false;
    if (receiveThread.joinable())
        receiveThread.join();
    else
        return;
    closesocket(serverSocket);
#ifdef _WIN32
    WSACleanup();
#endif
}

void Client::receiveMessages() {
    while (isConnected) {
        uint32_t msgSize;
        int bytesReceived = recv(serverSocket, (char*)&msgSize, sizeof(msgSize), 0);
        if (bytesReceived <= 0) {
            logOption_->LogMessage(LogLevel::Log_Error, "", "Server is down or connection lost.");
            notifyServerDown();
            break;
        }
        msgSize = ntohl(msgSize);

        std::vector<uint8_t> buffer(msgSize);
        size_t totalReceived = 0;
        while (totalReceived < msgSize) {
            bytesReceived = recv(serverSocket, (char*)buffer.data() + totalReceived, msgSize - totalReceived, 0);
            if (bytesReceived <= 0) break;
            totalReceived += bytesReceived;
        }
        if (bytesReceived <= 0) break;

        BaseMessage* msg = BaseMessage::deserializeMessage(buffer);
        if (msg) {
            if (msg->messageType == CLIENT_LIST_MESSAGE) {
                updateClientList(msg->message);
            }
            else {
                sortMessageByType(msg);
            }
            delete msg;
        }
    }
    disconnect();
}

void Client::sendMessage(const BaseMessage& msg) {
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(msg, buffer);
    uint32_t msgSize = htonl(buffer.size());
    send(serverSocket, (char*)&msgSize, sizeof(msgSize), 0);
    send(serverSocket, (char*)buffer.data(), buffer.size(), 0);
}

void Client::sortMessageByType(BaseMessage* msg) {
    std::lock_guard<std::mutex> lock(messageMutex);
    switch (msg->messageType) {
    case TEXT_MESSAGE:
        textMessages.push_back(*msg);
        break;
    case EVENT_MESSAGE:
        eventMessages.push_back(*msg);
        break;
    case SNAPSHOT_MESSAGE:
        snapshotMessages[msg->senderID] = *msg;
        break;
    case CLIENT_ID_MESSAGE:
        clientID = msg->senderID;
        logOption_->LogMessage(LogLevel::Log_Debug, "", "Assigned client ID: ", int(clientID));
        break;
    }
}

void Client::updateClientList(const std::queue<uint8_t>& data) {
    std::queue<uint8_t> temp = data;
    std::vector<ClientInfo> clientInfos;

    while (!temp.empty()) {
        ClientInfo info;
        if (temp.empty()) break;
        info.clientID = temp.front();
        temp.pop();

        if (temp.empty()) break;
        uint8_t ipLen = temp.front();
        temp.pop();

        std::string ip;
        for (int i = 0; i < ipLen; ++i) {
            if (temp.empty()) break;
            ip += static_cast<char>(temp.front());
            temp.pop();
        }
        info.ipAddress = ip;

        if (temp.size() < 2) break;
        uint8_t b1 = temp.front(); temp.pop();
        uint8_t b2 = temp.front(); temp.pop();
        info.port = ntohs((b1 << 8) | b2);

        clientInfos.push_back(info);
    }

    std::lock_guard<std::mutex> lock(messageMutex);
    connectedClientsInfo.clear();
    for (const auto& info : clientInfos) {
        connectedClientsInfo[info.clientID] = info;
    }

    std::queue<uint8_t> clientIDs;
    for (const auto& pair : connectedClientsInfo) {
        clientIDs.push(pair.first);
    }
    for (const auto& listener : listeners) {
        listener(clientIDs);
    }
}

void Client::addListener(std::function<void(const std::queue<uint8_t>&)> listener) {
    listeners.push_back(listener);
}

void Client::notifyServerDown() {
    isConnected = false;
    logOption_->LogMessage(LogLevel::Log_Info, "", "Disconnected from server. Please check the server status.");
    updateClientList(std::queue<uint8_t>());
}