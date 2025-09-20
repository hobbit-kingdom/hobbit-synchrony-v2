#include "Client.h"
#include "TLS.h"

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
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "WSAStartup failed");
        return false;
    }
#endif

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Issues creating socket");
        return false;
    }

    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    if (inet_pton(AF_INET, serverIP.c_str(), &serverHint.sin_addr) != 1) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Invalid IP address");
        return false;
    }

    if (connect(serverSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Cannot connect to server");
        return false;
    }

    // === TLS 1.3 client + TOFU pinning ===
    if (!tls_.init()) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "TLS init failed");
        return false;
    }

    std::string err;
    std::string serverId = serverIP + ":" + std::to_string(PORT);

#ifdef _WIN32
    // Use _dupenv_s to avoid 'getenv' deprecation warning.
    char* dup = nullptr;
    size_t len = 0;
    std::string knownHosts;
    if (_dupenv_s(&dup, &len, "APPDATA") != 0 || !dup) {
        knownHosts = ".\\MyApp\\known_hosts";
    }
    else {
        knownHosts = std::string(dup) + "\\MyApp\\known_hosts";
        free(dup);
    }
#else
    const char* home = std::getenv("HOME");
    std::string knownHosts = std::string(home ? home : ".") + "/.myapp/known_hosts";
#endif

    ssl = tls_.wrapAndConnect(serverSocket, serverId, knownHosts, err);
    if (!ssl) {
        logOption_->LogMessage(LogLevel::Log_Error, "", err);
#ifdef _WIN32
        closesocket(serverSocket);
        WSACleanup();
#else
        close(serverSocket);
#endif
        return false;
    }

    isConnected = true;

    // Prefer joinable thread (no detach) so disconnect can join cleanly.
    receiveThread = std::thread(&Client::receiveMessages, this);

    logOption_->LogMessage(LogLevel::Log_Info, "", "Connected to server (TLS 1.3)");
    return true;
}

void Client::disconnect() {
    if (!isConnected && !ssl && serverSocket == 0) return;

    isConnected = false;

    if (receiveThread.joinable())
        receiveThread.join();

    if (ssl) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = nullptr;
    }

#ifdef _WIN32
    if (serverSocket != INVALID_SOCKET) closesocket(serverSocket);
    WSACleanup();
#else
    if (serverSocket >= 0) close(serverSocket);
#endif
    serverSocket = 0;
}

void Client::receiveMessages() {
    while (isConnected) {
        // read frame size
        uint32_t netSize = 0;
        int bytesReceived = tls_read_exact(ssl, &netSize, sizeof(netSize));
        if (bytesReceived <= 0) {
            logOption_->LogMessage(LogLevel::Log_Error, "", "Server is down or connection lost.");
            notifyServerDown();
            break;
        }
        uint32_t msgSize = ntohl(netSize);

        std::vector<uint8_t> buffer(msgSize);
        bytesReceived = tls_read_exact(ssl, buffer.data(), (int)buffer.size());
        if (bytesReceived <= 0) {
            logOption_->LogMessage(LogLevel::Log_Error, "", "Server is down or connection lost.");
            notifyServerDown();
            break;
        }

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

    // Ensure cleanup if we exit the loop
    disconnect();
}

void Client::sendMessage(const BaseMessage& msg) {
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(msg, buffer);

    uint32_t netSize = htonl((uint32_t)buffer.size());
    if (tls_write_all(ssl, &netSize, sizeof(netSize)) <= 0) {
        logOption_->LogMessage(LogLevel::Log_Warning, "", "TLS write header failed");
        return;
    }
    if (tls_write_all(ssl, buffer.data(), (int)buffer.size()) <= 0) {
        logOption_->LogMessage(LogLevel::Log_Warning, "", "TLS write payload failed");
        return;
    }
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
    default:
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
        info.port = ntohs((uint16_t)((b1 << 8) | b2));

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
