#include "Server.h"

void Server::start() {
#ifdef _WIN32
    WSADATA wsData;
    WSAStartup(MAKEWORD(2, 2), &wsData);
#endif

    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        logOption_->LogMessage(LogLevel::Log_Error, "Error creating socket");
        return;
    }

    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    serverHint.sin_addr.s_addr = INADDR_ANY;

    if (bind(listeningSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        logOption_->LogMessage(LogLevel::Log_Error, "Error binding socket");
        return;
    }

    listen(listeningSocket, SOMAXCONN);
    logOption_->LogMessage(LogLevel::Log_Info, "Server is listening on port ", PORT);

    std::thread(&Server::acceptClients, this).detach();
}

void Server::acceptClients() {
    while (isRunning) {
        sockaddr_in clientHint{};
        socklen_t clientSize = sizeof(clientHint);

        SOCKET clientSocket = accept(listeningSocket, (sockaddr*)&clientHint, &clientSize);
        if (clientSocket != INVALID_SOCKET) {
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientHint.sin_addr, ipStr, INET_ADDRSTRLEN);
            std::string ipAddress(ipStr);
            uint16_t port = ntohs(clientHint.sin_port);

            uint8_t clientID = nextClientID++;
            ClientHandler* clientHandler = new ClientHandler{ clientSocket, clientID, ipAddress, port, std::thread() };

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.push_back(clientHandler);
            }

            BaseMessage idMessage(CLIENT_ID_MESSAGE, clientID);
            std::vector<uint8_t> buffer;
            BaseMessage::serializeMessage(idMessage, buffer);

            uint32_t msgSize = htonl(buffer.size());
            send(clientSocket, (char*)&msgSize, sizeof(msgSize), 0);
            send(clientSocket, (char*)buffer.data(), buffer.size(), 0);

            notifyClients();

            clientHandler->thread = std::thread(&Server::handleClient, this, clientHandler);
            clientHandler->thread.detach();

            logOption_->LogMessage(LogLevel::Log_Info, "Client", (int)clientID, "connected.");
        }
    }
}

void Server::handleClient(ClientHandler* clientHandler) {
    SOCKET clientSocket = clientHandler->socket;
    uint8_t clientID = clientHandler->clientID;

    while (isRunning) {
        uint32_t msgSize;
        int bytesReceived = recv(clientSocket, (char*)&msgSize, sizeof(msgSize), 0);
        if (bytesReceived <= 0) break;
        msgSize = ntohl(msgSize);

        std::vector<uint8_t> buffer(msgSize);
        size_t totalReceived = 0;
        while (totalReceived < msgSize) {
            bytesReceived = recv(clientSocket, (char*)buffer.data() + totalReceived, msgSize - totalReceived, 0);
            if (bytesReceived <= 0) break;
            totalReceived += bytesReceived;
        }
        if (bytesReceived <= 0) break;

        BaseMessage* msg = BaseMessage::deserializeMessage(buffer);
        if (msg) {
            msg->senderID = clientID;
            broadcastMessage(*msg, clientID);
            delete msg;
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [clientID](ClientHandler* ch) { return ch->clientID == clientID; }), clients.end());
    }

    notifyClients();
    closesocket(clientSocket);
    logOption_->LogMessage(LogLevel::Log_Info, "Client", (int)clientID, "disconnected.");
}

void Server::broadcastMessage(const BaseMessage& msg, uint8_t excludeID) {
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(msg, buffer);
    uint32_t msgSize = htonl(buffer.size());

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (ClientHandler* clientHandler : clients) {
        if (clientHandler->clientID != excludeID) {
            send(clientHandler->socket, (char*)&msgSize, sizeof(msgSize), 0);
            send(clientHandler->socket, (char*)buffer.data(), buffer.size(), 0);
        }
    }
}

void Server::notifyClients() {
    std::lock_guard<std::mutex> lock(clientsMutex);

    BaseMessage clientListMessage(CLIENT_LIST_MESSAGE, 0);
    for (ClientHandler* clientHandler : clients) {
        clientListMessage.message.push(clientHandler->clientID);

        std::string ip = clientHandler->ipAddress;
        clientListMessage.message.push(static_cast<uint8_t>(ip.size()));
        for (char c : ip) {
            clientListMessage.message.push(static_cast<uint8_t>(c));
        }

        uint16_t netPort = htons(clientHandler->port);
        clientListMessage.message.push(static_cast<uint8_t>((netPort >> 8) & 0xFF));
        clientListMessage.message.push(static_cast<uint8_t>(netPort & 0xFF));
    }

    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(clientListMessage, buffer);
    uint32_t msgSize = htonl(buffer.size());

    for (ClientHandler* clientHandler : clients) {
        send(clientHandler->socket, (char*)&msgSize, sizeof(msgSize), 0);
        send(clientHandler->socket, (char*)buffer.data(), buffer.size(), 0);
    }
    logOption_->LogMessage(LogLevel::Log_Debug, "Notified all clients about current client list");
}

void Server::stop() {
    isRunning = false;
    closesocket(listeningSocket);
#ifdef _WIN32
    WSACleanup();
#endif
}