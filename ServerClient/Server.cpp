#include "Server.h"

void Server::start() {
    // Initialize platform-specific networking
#ifdef _WIN32
    WSADATA wsData;
    WSAStartup(MAKEWORD(2, 2), &wsData);
#endif

    // Create listening socket
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        logOption_->LogMessage(LogLevel::Log_Error, "Error creating socket");
        return;
    }

    // Bind socket to IP and port
    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    serverHint.sin_addr.s_addr = INADDR_ANY;

    if (bind(listeningSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        logOption_->LogMessage(LogLevel::Log_Error, "Error binding socket");
        return;
    }

    // Start listening
    listen(listeningSocket, SOMAXCONN);
    logOption_->LogMessage(LogLevel::Log_Info, "Server is listening on port ", PORT);

    // Accept clients in a separate thread
    std::thread(&Server::acceptClients, this).detach();
}

void Server::acceptClients() {
    while (isRunning) {
        sockaddr_in clientHint{};
        socklen_t clientSize = sizeof(clientHint);

        SOCKET clientSocket = accept(listeningSocket, (sockaddr*)&clientHint, &clientSize);
        if (clientSocket != INVALID_SOCKET) {
            // Assign a unique ID to the new client
            uint8_t clientID = nextClientID++;

            // Create a new client handler
            ClientHandler* clientHandler = new ClientHandler{ clientSocket, clientID };
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients.push_back(clientHandler);
            }

            // Notify the client of its ID
            BaseMessage idMessage(CLIENT_ID_MESSAGE, clientID);
            std::vector<uint8_t> buffer;

            BaseMessage::serializeMessage(idMessage, buffer);

            uint32_t msgSize = htonl(buffer.size());
            send(clientSocket, (char*)&msgSize, sizeof(msgSize), 0);
            send(clientSocket, (char*)buffer.data(), buffer.size(), 0);

            notifyClients();  // Notify others about the new client

            // Start client thread
            clientHandler->thread = std::thread(&Server::handleClient, this, clientHandler);
            clientHandler->thread.detach();

            logOption_->LogMessage(LogLevel::Log_Info, "Client", (int)clientID , "connected.");
        }
    }
}

void Server::handleClient(ClientHandler* clientHandler) {
    SOCKET clientSocket = clientHandler->socket;
    uint8_t clientID = clientHandler->clientID;

    while (isRunning) {
        // Receive message size
        uint32_t msgSize;
        int bytesReceived = recv(clientSocket, (char*)&msgSize, sizeof(msgSize), 0);
        if (bytesReceived <= 0) {
            break;
        }
        msgSize = ntohl(msgSize);

        // Receive message data
        std::vector<uint8_t> buffer(msgSize);
        size_t totalReceived = 0;
        while (totalReceived < msgSize) {
            bytesReceived = recv(clientSocket, (char*)buffer.data() + totalReceived, msgSize - totalReceived, 0);
            if (bytesReceived <= 0) {
                break;
            }
            totalReceived += bytesReceived;
        }
        if (bytesReceived <= 0) {
            break;
        }

        // Deserialize message
        BaseMessage* msg = BaseMessage::deserializeMessage(buffer);
        if (msg) {
            msg->senderID = clientID;
            // Broadcast the message to other clients
            broadcastMessage(*msg, clientID);
            delete msg;
        }
    }

    // Remove client from list
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [clientID](ClientHandler* ch) { return ch->clientID == clientID; }), clients.end());
    }
        notifyClients(); // Notify all clients of the disconnection


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

    // Collect the client IDs of all connected clients
    std::vector<uint8_t> clientIDs;
    for (const ClientHandler* clientHandler : clients) {
        clientIDs.push_back(clientHandler->clientID);
    }

    BaseMessage clientListMessage(CLIENT_LIST_MESSAGE, 0); // Sender ID is set to 0 for server
    for(ClientHandler* clientHandler : clients)
        clientListMessage.message.push(clientHandler->clientID);

    // Serialize the ClientListMessage
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(clientListMessage, buffer);

    uint32_t msgSize = htonl(buffer.size());

    // Broadcast the serialized message to all connected clients
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