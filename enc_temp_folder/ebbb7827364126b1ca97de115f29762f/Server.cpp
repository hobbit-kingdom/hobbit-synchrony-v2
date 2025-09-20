#include "Server.h"
#include <cstring>
#include <algorithm>

Server::Server() : logOption_(LogManager::Instance().CreateLogOption("SERVER")) {}

bool Server::start() {
#ifdef _WIN32
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "WSAStartup failed");
        return false;
    }
#endif

    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Failed to create socket");
        return false;
    }

    sockaddr_in serverHint{};
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    serverHint.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
#ifdef _WIN32
    setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (bind(listeningSocket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Bind failed");
        return false;
    }

    if (listen(listeningSocket, SOMAXCONN) == SOCKET_ERROR) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "Listen failed");
        return false;
    }

    // TLS 1.3 server with auto self-signed certs (no user setup)
    if (!tls_.init("certs/server_cert.pem", "certs/server_key.pem")) {
        logOption_->LogMessage(LogLevel::Log_Error, "", "TLS server init failed");
        return false;
    }

    isRunning = true;
    std::thread(&Server::acceptClients, this).detach();

    logOption_->LogMessage(LogLevel::Log_Info, "", "Server started on port ", PORT);
    return true;
}

void Server::stop() {
    if (!isRunning) return;
    isRunning = false;

#ifdef _WIN32
    closesocket(listeningSocket);
    listeningSocket = INVALID_SOCKET;
    WSACleanup();
#else
    close(listeningSocket);
    listeningSocket = -1;
#endif

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto* ch : clients) {
        if (!ch) continue;
        if (ch->ssl) {
            SSL_shutdown(ch->ssl);
            SSL_free(ch->ssl);
            ch->ssl = nullptr;
        }
#ifdef _WIN32
        closesocket(ch->socket);
#else
        close(ch->socket);
#endif
        // Threads are detached; they exit when isRunning becomes false.
        delete ch;
    // Notify others about the updated list
    notifyClients();
    }
    clients.clear();
}

void Server::acceptClients() {
    while (isRunning) {
        sockaddr_in clientHint{};
#ifdef _WIN32
        int clientSize = (int)sizeof(clientHint);
#else
        socklen_t clientSize = (socklen_t)sizeof(clientHint);
#endif

        SOCKET clientSocket = accept(listeningSocket, (sockaddr*)&clientHint, &clientSize);
        if (clientSocket == INVALID_SOCKET) continue;

        // Wrap with TLS and perform server-side handshake
        SSL* ssl = tls_.wrap(clientSocket);
        if (SSL_accept(ssl) <= 0) {
            SSL_free(ssl);
#ifdef _WIN32
            closesocket(clientSocket);
#else
            close(clientSocket);
#endif
            continue;
        }

        char ipbuf[64] = { 0 };
        inet_ntop(AF_INET, &clientHint.sin_addr, ipbuf, sizeof(ipbuf));
        uint16_t port = ntohs(clientHint.sin_port);
        std::string ipAddress = ipbuf;

        uint8_t clientID = allocateClientID();
        auto* clientHandler = new ClientHandler{ clientSocket, clientID, ipAddress, port, std::thread(), ssl };

        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            clients.push_back(clientHandler);
        }

        // Send initial CLIENT_ID message over TLS
        BaseMessage idMessage(CLIENT_ID_MESSAGE, clientID);
        std::vector<uint8_t> buffer;
        BaseMessage::serializeMessage(idMessage, buffer);
        uint32_t netSize = htonl((uint32_t)buffer.size());
        SSL_write(ssl, &netSize, sizeof(netSize));
        SSL_write(ssl, buffer.data(), (int)buffer.size());

        
        // Notify all clients of updated list
        notifyClients();
clientHandler->thread = std::thread(&Server::handleClient, this, clientHandler);
        clientHandler->thread.detach();
    }
}

void Server::handleClient(ClientHandler* ch) {
    SOCKET s = ch->socket;
    SSL* ssl = ch->ssl;

    while (isRunning) {
        uint32_t netSize = 0;
        int r = tls_read_exact(ssl, &netSize, sizeof(netSize));
        if (r <= 0) break;

        uint32_t msgSize = ntohl(netSize);
        if (msgSize == 0) continue;

        std::vector<uint8_t> buf(msgSize);
        r = tls_read_exact(ssl, buf.data(), (int)buf.size());
        if (r <= 0) break;

        // Deserialize and (by default) broadcast to others
        std::unique_ptr<BaseMessage> msg(BaseMessage::deserializeMessage(buf));
        if (!msg) continue;

        processMessage(*msg, ch);
}

    // Cleanup on exit
    SSL_shutdown(ssl);
    SSL_free(ssl);
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif

    // Remove from list
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        auto it = std::find_if(clients.begin(), clients.end(),
            [ch](ClientHandler* c) { return c == ch; });
        if (it != clients.end()) {
            clients.erase(it);
        }
    }
    delete ch;
}

void Server::broadcastMessage(const BaseMessage& msg, uint8_t excludeID) {
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(msg, buffer);
    uint32_t netSize = htonl((uint32_t)buffer.size());

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (ClientHandler* c : clients) {
        if (!c || c->clientID == excludeID) continue;

        send(c->socket, (char*)&netSize, sizeof(netSize), 0);
        send(c->socket, (char*)buffer.data(), buffer.size(), 0);
//      SSL_write(c->ssl, &netSize, sizeof(netSize));
 //       SSL_write(c->ssl, buffer.data(), (int)buffer.size());
    }
}

bool Server::sendToClient(uint8_t clientID, const BaseMessage& msg) {
    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(msg, buffer);
    uint32_t netSize = htonl((uint32_t)buffer.size());

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (ClientHandler* c : clients) {
        if (c && c->clientID == clientID) {
            SSL_write(c->ssl, &netSize, sizeof(netSize));
            SSL_write(c->ssl, buffer.data(), (int)buffer.size());
            return true;
        }
    }
    return false;
}


void Server::processMessage(const BaseMessage& msg, ClientHandler* from) {
    switch (msg.messageType)
    {
    case TEXT_MESSAGE: {
        uint8_t targetID = 0;
        if (!msg.message.empty()) {
            targetID = const_cast<std::queue<uint8_t>&>(msg.message).front();
        }
        if (targetID == 0) {
            broadcastMessage(msg, from ? from->clientID : 0);
            logOption_->LogMessage(LogLevel::Log_Debug,
                "Broadcast TEXT_MESSAGE from ", (int)(from ? from->clientID : 0));
        } else {
            BaseMessage routed = msg;
            if (!routed.message.empty()) {
                auto& q = routed.message;
                q.pop();
            }
            bool ok = sendToClient(targetID, routed);
            logOption_->LogMessage(ok ? LogLevel::Log_Debug : LogLevel::Log_Warning,
                ok ? "Routed DM TEXT_MESSAGE to " : "Failed to route DM to ",
                (int)targetID, " from ", (int)(from ? from->clientID : 0));
        }
        break;
    }
    case EVENT_MESSAGE:
    case SNAPSHOT_MESSAGE:
        broadcastMessage(msg, from ? from->clientID : 0);
        break;
    case CLIENT_ID_MESSAGE:
    case CLIENT_LIST_MESSAGE:
        logOption_->LogMessage(LogLevel::Log_Warning,
            "Client ", (int)(from ? from->clientID : 0),
            " sent server-reserved messageType ", (int)msg.messageType, "  ignored.");
        break;
    default:
        break;
    }
}

void Server::notifyClients() {
    std::lock_guard<std::mutex> lock(clientsMutex);

    BaseMessage clientListMessage(CLIENT_LIST_MESSAGE, 0);
    for (ClientHandler* ch : clients) {
        if (!ch) continue;

        clientListMessage.message.push(ch->clientID);

        const std::string& ip = ch->ipAddress;
        clientListMessage.message.push(static_cast<uint8_t>(ip.size()));
        for (char c : ip) {
            clientListMessage.message.push(static_cast<uint8_t>(c));
        }

        uint16_t netPort = htons(ch->port);
        clientListMessage.message.push(static_cast<uint8_t>((netPort >> 8) & 0xFF));
        clientListMessage.message.push(static_cast<uint8_t>(netPort & 0xFF));
    }

    std::vector<uint8_t> buffer;
    BaseMessage::serializeMessage(clientListMessage, buffer);
    uint32_t netSize = htonl((uint32_t)buffer.size());

    for (ClientHandler* c : clients) {
        if (!c) continue;
        SSL_write(c->ssl, &netSize, sizeof(netSize));
        SSL_write(c->ssl, buffer.data(), (int)buffer.size());
    }

    logOption_->LogMessage(LogLevel::Log_Debug, "Notified all clients about current client list");
}
