#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <map>
#include <string> // For std::string, std::stoi
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>    // For std::atomic<bool>
#include <chrono>    // For std::chrono::milliseconds in wait_for
#include <memory>    // For std::unique_ptr

#include "common/NetworkUtilities/Message.h"
#include "common/NetworkUtilities/WinCrypto.h"

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define MAX_RECV_BUFFER_SIZE 4096 // For receiving messages
#define MAX_PUBLIC_KEY_SIZE 2048 

// --- Global Variables & Structures ---
WinCrypto cryptoInstance; 
RsaKeyPair serverRsaKeyPair; 

// Snapshot Structure
struct Snapshot {
    std::string type; // e.g., "POSITION", "ROTATION"
    std::vector<std::string> data;
    time_t receivedTimestamp;
};

// Client Data Structure
struct ClientData {
    std::string name;
    int age = 0;
    int health = 0;
    int level = 0;
    WinCrypto::PublicKey publicKey; 
    SOCKET socket; 
    std::string ipAddress;
    
    std::map<std::string, Snapshot> latestSnapshots;
    std::mutex snapshotsMutex; // Mutex to protect this specific client's snapshots map
};
std::map<SOCKET, std::unique_ptr<ClientData>> clientProfiles; // Changed to store unique_ptr
std::mutex clientProfilesMutex; // To protect the global clientProfiles map

// Event Structure and Queue
struct ClientEvent {
    SOCKET clientSocket;
    std::vector<std::string> eventData;
    Message::MessageType originalMessageType = Message::MessageType::UNDEFINED; 
};
std::queue<ClientEvent> eventQueue;
std::mutex queueMutex;
std::condition_variable queueCondVar;

std::atomic<bool> serverRunning(true); // For graceful shutdown
std::vector<std::thread> clientHandlerThreads;
std::thread eventThread;


// --- Event Processing Thread ---
void processEvents() {
    std::cout << "Event processing thread started." << std::endl;
    while (serverRunning || !eventQueue.empty()) {
        ClientEvent event;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (!queueCondVar.wait_for(lock, std::chrono::milliseconds(100), [&]{ return !eventQueue.empty(); })) {
                // Timeout occurred, check serverRunning flag
                if (!serverRunning && eventQueue.empty()) {
                    break; // Exit if server stopped and queue is empty
                }
                continue; // Spurious wakeup or timeout, re-check conditions
            }
            event = eventQueue.front();
            eventQueue.pop();
        } // Mutex unlocked here

        std::cout << "Processing event from client " << event.clientSocket << ": ";
        for(const auto& part : event.eventData) std::cout << part << " ";
        std::cout << std::endl;

        std::lock_guard<std::mutex> profileLock(clientProfilesMutex);
        auto it = clientProfiles.find(event.clientSocket);
        if (it != clientProfiles.end() && it->second) { // Check if pointer is valid
            ClientData* profilePtr = it->second.get(); // Get raw pointer
            try {
                if (!event.eventData.empty()) {
                    const std::string& command = event.eventData[0];
                    if (command == "SET_HEALTH" && event.eventData.size() > 1) {
                        profilePtr->health = std::stoi(event.eventData[1]);
                        std::cout << "  Client " << profilePtr->name << " health set to " << profilePtr->health << std::endl;
                    } else if (command == "LEVEL_UP") {
                        profilePtr->level++;
                        std::cout << "  Client " << profilePtr->name << " leveled up to " << profilePtr->level << std::endl;
                    } else if (command == "SET_NAME" && event.eventData.size() > 1) {
                        profilePtr->name = event.eventData[1];
                         std::cout << "  Client " << event.clientSocket << " name set to " << profilePtr->name << std::endl;
                    }
                    // Add more event types here
                    else {
                        std::cout << "  Unknown event command: " << command << std::endl;
                    }
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "  Error processing event (invalid argument for stoi): " << ia.what() << std::endl;
            } catch (const std::out_of_range& oor) {
                std::cerr << "  Error processing event (out of range for stoi): " << oor.what() << std::endl;
            }
        } else {
            std::cerr << "  Error processing event: Client profile not found for socket " << event.clientSocket << std::endl;
        }
    }
    std::cout << "Event processing thread finished." << std::endl;
}

// --- Client Handling Thread ---
void handleClient(SOCKET clientSocket, std::string clientIP) {
    std::cout << "Thread started for client: " << clientIP << " (Socket: " << clientSocket << ")" << std::endl;
    // ClientData is now managed by unique_ptr in the map.
    // We'll create it after successful GREETING.
    // Store client's name locally for logging before profile is fully established.
    std::string clientIdentifier = clientIP; 

    try {
        // 1. Key Exchange: Receive client's public key
        WinCrypto::PublicKey clientPubKeyForGreeting; 
        std::vector<BYTE> clientKeyBlobBytes(MAX_PUBLIC_KEY_SIZE);
        int bytesReceived = recv(clientSocket, reinterpret_cast<char*>(clientKeyBlobBytes.data()), clientKeyBlobBytes.size(), 0);
        
        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "Client " << clientIdentifier << ": recv (client public key) failed with error: " << WSAGetLastError() << std::endl;
            closesocket(clientSocket); return;
        }
        if (bytesReceived == 0) {
            std::cout << "Client " << clientIdentifier << " disconnected gracefully during public key exchange." << std::endl;
            closesocket(clientSocket); return;
        }
        clientKeyBlobBytes.resize(bytesReceived);
        clientPubKeyForGreeting.blob = clientKeyBlobBytes;
        std::cout << "Received public key from " << clientIdentifier << " (" << bytesReceived << " bytes)." << std::endl;

        // 2. Key Exchange: Send server's public key
        if (serverRsaKeyPair.publicKeyBlob.empty()) {
            throw std::runtime_error("Server public key is not available. Cannot complete key exchange with " + clientIdentifier);
        }
        
        int bytesToSend = serverRsaKeyPair.publicKeyBlob.size();
        int bytesSent = send(clientSocket, reinterpret_cast<const char*>(serverRsaKeyPair.publicKeyBlob.data()), bytesToSend, 0);
        
        if (bytesSent == SOCKET_ERROR) {
            throw std::runtime_error("send (server public key) to " + clientIdentifier + " failed with error: " + std::to_string(WSAGetLastError()));
        }
        if (bytesSent < bytesToSend) {
            throw std::runtime_error("send (server public key) to " + clientIdentifier + ": Incomplete send. Sent " + std::to_string(bytesSent) + "/" + std::to_string(bytesToSend) + " bytes.");
        }
        std::cout << "Sent server public key to " << clientIdentifier << "." << std::endl;

        // 3. Receive GREETING message (Client Profile)
        char rawRecvBuffer[MAX_RECV_BUFFER_SIZE];
        bytesReceived = recv(clientSocket, rawRecvBuffer, sizeof(rawRecvBuffer) - 1, 0); // -1 for null term space if used as C-string later
        
        if (bytesReceived == SOCKET_ERROR) {
            throw std::runtime_error("recv (GREETING message) from " + clientIdentifier + " failed with error: " + std::to_string(WSAGetLastError()));
        }
        if (bytesReceived == 0) {
            throw std::runtime_error("Client " + clientIdentifier + " disconnected gracefully before sending GREETING message.");
        }
        std::string receivedRawString(rawRecvBuffer, bytesReceived);
        std::cout << "Received GREETING message from " << clientIdentifier << " (" << bytesReceived << " bytes)." << std::endl;
        
        Message greetingMsg(receivedRawString, serverRsaKeyPair.hPrivateKey, cryptoInstance); // This can throw std::runtime_error
        
        if (greetingMsg.getMessageType() != Message::MessageType::GREETING) {
            throw std::runtime_error("Expected GREETING message from " + clientIdentifier + ", but received type " + std::to_string(static_cast<int>(greetingMsg.getMessageType())) + ". Content: " + receivedRawString);
        }

        const auto& bodyParts = greetingMsg.getMessageBodyParts();
        if (bodyParts.size() < 4) { // Name, Age, Health, Level
            throw std::runtime_error("GREETING message from " + clientIdentifier + " has insufficient body parts (" + std::to_string(bodyParts.size()) + "). Expected at least 4.");
        }
        // std::stoi can throw std::invalid_argument or std::out_of_range, caught by main try-catch in handleClient
        
        auto newClientData = std::make_unique<ClientData>();
        newClientData->name = bodyParts[0];
        newClientData->age = std::stoi(bodyParts[1]);
        newClientData->health = std::stoi(bodyParts[2]);
        newClientData->level = std::stoi(bodyParts[3]);
        newClientData->publicKey = clientPubKeyForGreeting; // Store received public key
        newClientData->socket = clientSocket;
        newClientData->ipAddress = clientIP;
        
        clientIdentifier = newClientData->name; // Update identifier for logging

        { // Scope for profile lock
            std::lock_guard<std::mutex> profileLock(clientProfilesMutex);
            clientProfiles[clientSocket] = std::move(newClientData); 
        }
        std::cout << "Client profile for " << clientIdentifier << " (" << clientIP << ") initialized." << std::endl;
        std::cout << "  Name: " << clientProfiles[clientSocket]->name 
                  << ", Age: " << clientProfiles[clientSocket]->age 
                  << ", Health: " << clientProfiles[clientSocket]->health 
                  << ", Level: " << clientProfiles[clientSocket]->level << std::endl;


        // 4. Message Loop for further communication (e.g., EVENTs)
        while (serverRunning) {
            bytesReceived = recv(clientSocket, rawRecvBuffer, sizeof(rawRecvBuffer) - 1, 0);
            if (bytesReceived <= 0) {
                if (bytesReceived == 0) {
                    std::cout << "Client " << clientIdentifier << " (" << clientIP << ") disconnected gracefully in main loop." << std::endl;
                } else { // SOCKET_ERROR
                    std::cerr << "Main loop recv error from " << clientIdentifier << " (" << clientIP << "). Code: " << WSAGetLastError() << std::endl;
                }
                break; // Exit message loop
            }
            receivedRawString.assign(rawRecvBuffer, bytesReceived);
            std::cout << "Received data from " << clientIdentifier << " (" << clientIP << ") (" << bytesReceived << " bytes)." << std::endl;

            Message receivedMsg(receivedRawString, serverRsaKeyPair.hPrivateKey, cryptoInstance); // Can throw
            
            if (receivedMsg.getMessageType() == Message::MessageType::EVENT) {
                ClientEvent event;
                event.clientSocket = clientSocket;
                event.eventData = receivedMsg.getMessageBodyParts(); 
                event.originalMessageType = Message::MessageType::EVENT;

                { 
                    std::lock_guard<std::mutex> queueLock(queueMutex);
                    eventQueue.push(event);
                }
                queueCondVar.notify_one();
                std::cout << "EVENT message from " << clientIdentifier << " queued." << std::endl;
                
                // Optional: Send ACK to client. Requires client's public key.
                // {
                //     std::lock_guard<std::mutex> profileLock(clientProfilesMutex);
                //     auto it = clientProfiles.find(clientSocket);
                //     if (it != clientProfiles.end() && it->second) {
                //         Message ack(Message::MessageType::TEXT, it->second->publicKey, "Server", clientIdentifier);
                //         ack.pushBody("EVENT_ACK");
                //         ack.generateEncryptedMessageForSending(cryptoInstance);
                //         std::string ackStr = ack.getMessageToSend();
                //         send(clientSocket, ackStr.c_str(), ackStr.length(), 0);
                //     }
                // }


            } else if (receivedMsg.getMessageType() == Message::MessageType::TEXT) {
                 std::cout << "TEXT message from " << clientIdentifier << " (" << clientIP << "): ";
                 for(const auto& part : receivedMsg.getMessageBodyParts()) std::cout << part << " ";
                 std::cout << std::endl;

            } else if (receivedMsg.getMessageType() == Message::MessageType::SNAP) {
                const auto& snapBodyParts = receivedMsg.getMessageBodyParts();
                if (snapBodyParts.empty()) {
                    std::cerr << "Malformed SNAP message from " << clientIdentifier << " (" << clientIP << "): body is empty." << std::endl;
                } else {
                    std::string snapshotType = snapBodyParts[0];
                    std::vector<std::string> snapshotData;
                    if (snapBodyParts.size() > 1) {
                        snapshotData.assign(snapBodyParts.begin() + 1, snapBodyParts.end());
                    }
                    Snapshot newSnapshot{snapshotType, snapshotData, std::time(nullptr)};

                    ClientData* clientDataPtr = nullptr;
                    { 
                        std::lock_guard<std::mutex> mainProfileLock(clientProfilesMutex);
                        auto it = clientProfiles.find(clientSocket);
                        if (it != clientProfiles.end() && it->second) {
                            clientDataPtr = it->second.get();
                        }
                    } 

                    if (clientDataPtr) {
                        std::lock_guard<std::mutex> clientSnapshotLock(clientDataPtr->snapshotsMutex);
                        clientDataPtr->latestSnapshots[snapshotType] = newSnapshot;
                        std::cout << "SNAP message (" << snapshotType << ") from " << clientIdentifier 
                                  << " (" << clientIP << ") processed and stored. Data parts: " << snapshotData.size() << std::endl;
                    } else {
                        std::cerr << "Failed to find client profile for " << clientIdentifier 
                                  << " (" << clientIP << ") to store snapshot." << std::endl;
                    }
                }
            }
            else {
                 std::cout << "Unhandled message type " << static_cast<int>(receivedMsg.getMessageType()) 
                           << " from " << clientIdentifier << " (" << clientIP << ")" << std::endl;
            }
        }

    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error in handleClient for " << clientIdentifier << ": " << e.what() << std::endl;
    } catch (const std::invalid_argument& ia) {
        std::cerr << "Invalid argument error in handleClient for " << clientIdentifier << ": " << ia.what() << std::endl;
    } catch (const std::out_of_range& oor) {
        std::cerr << "Out of range error in handleClient for " << clientIdentifier << ": " << oor.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown error in handleClient for " << clientIdentifier << "." << std::endl;
    }

    // Cleanup for this client
    std::cout << "Client " << clientIdentifier << " (" << clientIP << ") disconnected. Cleaning up..." << std::endl;
    {
        std::lock_guard<std::mutex> profileLock(clientProfilesMutex);
        clientProfiles.erase(clientSocket);
    }
    closesocket(clientSocket);
    std::cout << "Thread finished for client: " << clientIP << " (Socket: " << clientSocket << ")" << std::endl;
}


// --- Server Main ---
void cleanupServerKeys() { // To be called explicitly now
    if (serverRsaKeyPair.hPrivateKey) {
        try {
            cryptoInstance.destroyKey(serverRsaKeyPair.hPrivateKey);
            serverRsaKeyPair.hPrivateKey = NULL;
            std::cout << "Server RSA private key destroyed." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error destroying server RSA private key: " << e.what() << std::endl;
        }
    }
}

int main() {
    // std::atexit(cleanupServerKeys); // Replaced by explicit cleanup

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    try {
        std::cout << "Generating server RSA key pair..." << std::endl;
        cryptoInstance.generateRsaKeyPair(serverRsaKeyPair);
        if (!serverRsaKeyPair.hPrivateKey || serverRsaKeyPair.publicKeyBlob.empty()) {
            throw std::runtime_error("Server RSA key pair generation failed.");
        }
        std::cout << "Server RSA key pair generated. Public key size: " << serverRsaKeyPair.publicKeyBlob.size() << " bytes." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: Exception during server RSA key pair generation: " << e.what() << std::endl;
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        cleanupServerKeys(); WSACleanup(); return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket); cleanupServerKeys(); WSACleanup(); return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket); cleanupServerKeys(); WSACleanup(); return 1;
    }
    std::cout << "Server listening on port " << SERVER_PORT << std::endl;

    // Start the event processing thread
    eventThread = std::thread(processEvents);
    std::cout << "Event processing thread create initiated." << std::endl;


    // Main accept loop
    while (serverRunning) { // Check serverRunning for graceful shutdown of accept loop
        // Use select or similar to make accept non-blocking, or set a timeout on the socket
        // For simplicity, this accept is blocking but we'll rely on a signal/flag for shutdown.
        // A more robust shutdown would involve closing listenSocket from another thread.
        
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSocket, &readfds);
        timeval timeout;
        timeout.tv_sec = 1; // 1 second timeout for select
        timeout.tv_usec = 0;

        int activity = select(0, &readfds, nullptr, nullptr, &timeout);
        if (activity == SOCKET_ERROR) {
            std::cerr << "select() failed: " << WSAGetLastError() << std::endl;
            // serverRunning = false; // Optionally trigger shutdown on select error
            continue; // Or break, depending on desired behavior
        }

        if (activity > 0 && FD_ISSET(listenSocket, &readfds)) {
            SOCKET clientSocket = INVALID_SOCKET;
            sockaddr_in clientAddr;
            int clientAddrSize = sizeof(clientAddr);
            clientSocket = accept(listenSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);

            if (clientSocket == INVALID_SOCKET) {
                if (WSAGetLastError() != WSAEWOULDBLOCK) { // WSAEWOULDBLOCK is fine if socket is non-blocking
                     std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
                }
                // Continue if serverRunning is true, otherwise this error might be due to listenSocket closing
                if (!serverRunning) break; 
                continue;
            }

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::cout << "Accepted new connection from " << clientIP << ":" << ntohs(clientAddr.sin_port) 
                      << " (Socket: " << clientSocket << ")" << std::endl;
            
            try {
                 clientHandlerThreads.emplace_back(handleClient, clientSocket, std::string(clientIP));
            } catch (const std::system_error& e) { // Catch errors from std::thread constructor
                 std::cerr << "Fatal: Failed to create thread for client " << clientIP << ": " << e.what() << ". Error code: " << e.code() << std::endl;
                 closesocket(clientSocket); // Clean up the accepted socket
                 // Depending on severity, might consider setting serverRunning = false here.
            } catch (const std::exception& e) { // Catch other potential exceptions
                 std::cerr << "Fatal: Exception while creating thread for client " << clientIP << ": " << e.what() << std::endl;
                 closesocket(clientSocket);
            }
        }
        // If activity == 0, it's a timeout, loop again to check serverRunning
    }

    std::cout << "Server shutting down: Main accept loop exited." << std::endl;
    
    // Graceful shutdown
    serverRunning = false; // Signal all threads to stop
    std::cout << "Notifying event queue to wake up for shutdown..." << std::endl;
    queueCondVar.notify_all(); // Wake up event processing thread if it's waiting
    
    std::cout << "Joining event thread..." << std::endl;
    if(eventThread.joinable()) eventThread.join();
    std::cout << "Event thread joined." << std::endl;

    std::cout << "Joining client handler threads..." << std::endl;
    for (auto& th : clientHandlerThreads) {
        if (th.joinable()) {
            th.join();
        }
    }
    std::cout << "All client handler threads joined." << std::endl;

    closesocket(listenSocket);
    std::cout << "Listen socket closed." << std::endl;
    
    cleanupServerKeys(); // Explicitly clean up server keys
    WSACleanup();
    std::cout << "Server shut down gracefully." << std::endl;
    return 0;
}
