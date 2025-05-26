#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h> // For inet_pton
#include <vector>     // For std::vector
#include <string>     // For std::string, std::to_string
#include <chrono>     // For std::chrono::milliseconds
#include <thread>     // For std::this_thread::sleep_for

#include "common/NetworkUtilities/Message.h"
#include "common/NetworkUtilities/WinCrypto.h" 

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 8080
#define SERVER_ADDR "127.0.0.1"
#define MAX_PUBLIC_KEY_SIZE 2048 // Max expected size for a public key blob

// Global/static crypto instance and client's RSA key pair
WinCrypto cryptoInstance; // Automatically initializes hProv
RsaKeyPair clientRsaKeyPair; // Will hold client's private and public key blob
PublicKey serverPublicKey; // Will hold the server's public key

void cleanupClientKeys() {
    if (clientRsaKeyPair.hPrivateKey) {
        try {
            cryptoInstance.destroyKey(clientRsaKeyPair.hPrivateKey);
            clientRsaKeyPair.hPrivateKey = NULL; // Mark as destroyed
            std::cout << "Client RSA private key destroyed." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error destroying client RSA private key: " << e.what() << std::endl;
        }
    }
}

int main() {
    std::atexit(cleanupClientKeys); // Register cleanup function

    // 1. Client RSA Key Pair Generation
    try {
        std::cout << "Generating client RSA key pair..." << std::endl;
        cryptoInstance.generateRsaKeyPair(clientRsaKeyPair);
        if (!clientRsaKeyPair.hPrivateKey || clientRsaKeyPair.publicKeyBlob.empty()) { // Simplified check
            std::cerr << "Client RSA key pair generation failed (keys are null/empty)." << std::endl;
            // No WSACleanup or closesocket here as they haven't been initialized/created yet.
            return 1; 
        }
        std::cout << "Client RSA key pair generated successfully. Public key size: "
                  << clientRsaKeyPair.publicKeyBlob.size() << " bytes." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception during client RSA key pair generation: " << e.what() << std::endl;
        return 1; 
    }

    WSADATA wsaData;
    SOCKET connectSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddrStruct; // Renamed to avoid conflict with global serverAddr define

    // Initialize Winsock
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed with error: " << result << std::endl;
        return 1;
    }

    // Create a socket
    connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        std::cerr << "socket() failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Setup server address structure
    serverAddrStruct.sin_family = AF_INET;
    serverAddrStruct.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDR, &serverAddrStruct.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported: " << SERVER_ADDR << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    // Connect to server
    std::cout << "Connecting to server " << SERVER_ADDR << ":" << SERVER_PORT << "..." << std::endl;
    result = connect(connectSocket, (struct sockaddr*)&serverAddrStruct, sizeof(serverAddrStruct));
    if (result == SOCKET_ERROR) {
        std::cerr << "connect failed: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "Connected to server." << std::endl;

    // 2. Sending Client's Public Key to Server
    if (!clientRsaKeyPair.publicKeyBlob.empty()) {
        std::cout << "Sending client public key to server (" << clientRsaKeyPair.publicKeyBlob.size() << " bytes)..." << std::endl;
        int bytesSent = send(connectSocket,
                             reinterpret_cast<const char*>(clientRsaKeyPair.publicKeyBlob.data()),
                             clientRsaKeyPair.publicKeyBlob.size(),
                             0);
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "send (client public key) failed: " << WSAGetLastError() << std::endl;
            closesocket(connectSocket);
            WSACleanup();
            return 1;
        } else if (bytesSent < static_cast<int>(clientRsaKeyPair.publicKeyBlob.size())) {
            std::cerr << "send (client public key): not all bytes sent. Sent " << bytesSent << "/" << clientRsaKeyPair.publicKeyBlob.size() << std::endl;
            closesocket(connectSocket);
            WSACleanup();
            return 1;
        }
        std::cout << "Client public key sent successfully." << std::endl;
    } else {
        std::cerr << "Client public key blob is empty. Cannot send." << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    // 3. Receiving Server's Public Key
    std::vector<BYTE> serverKeyBuffer(MAX_PUBLIC_KEY_SIZE);
    std::cout << "Waiting to receive server's public key..." << std::endl;
    int bytesReceived = recv(connectSocket, reinterpret_cast<char*>(serverKeyBuffer.data()), serverKeyBuffer.size(), 0);

    if (bytesReceived == SOCKET_ERROR) {
        std::cerr << "recv (server public key) failed: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    } else if (bytesReceived == 0) {
        std::cerr << "Server closed connection while sending its public key." << std::endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    } else if (bytesReceived > 0) {
        // 4. Storing Server's Public Key
        serverPublicKey.blob.assign(serverKeyBuffer.begin(), serverKeyBuffer.begin() + bytesReceived);
        std::cout << "Server's public key received and stored successfully. Size: "
                  << serverPublicKey.blob.size() << " bytes." << std::endl;
    }

    // Key exchange phase completed.
    std::cout << "Key exchange phase completed." << std::endl;

    // 5. Define and Send Client Profile Data (Greeting Message) using refactored Message class
    if (serverPublicKey.blob.empty()) {
        std::cerr << "Server public key is not available. Cannot send profile." << std::endl;
    } else {
        try {
            std::cout << "Preparing GREETING message with client profile..." << std::endl;
            // Construct Message with type and recipient's public key. Sender/Receiver are optional defaults.
            Message profileMsg(Message::MessageType::GREETING, serverPublicKey, "ClientPlayer", "GameServer");

            // Define client profile data
            std::string clientName = "PlayerOne"; // As per subtask description
            int clientAge = 28;
            int clientHealth = 100;
            int clientLevel = 10;

            profileMsg.pushBody(clientName);
            profileMsg.pushBody(std::to_string(clientAge));
            profileMsg.pushBody(std::to_string(clientHealth));
            profileMsg.pushBody(std::to_string(clientLevel));

            std::cout << "Generating encrypted message for sending..." << std::endl;
            profileMsg.generateEncryptedMessageForSending(cryptoInstance); // Pass WinCrypto instance

            std::string encryptedDataToSend = profileMsg.getMessageToSend();
            
            if (!encryptedDataToSend.empty()) {
                std::cout << "Sending encrypted profile message to server (" << encryptedDataToSend.length() << " bytes)..." << std::endl;
                int bytesSent = send(connectSocket,
                                     encryptedDataToSend.c_str(),
                                     encryptedDataToSend.length(),
                                     0);
                if (bytesSent == SOCKET_ERROR) {
                    std::cerr << "send (profile message) failed: " << WSAGetLastError() << std::endl;
                } else if (bytesSent < static_cast<int>(encryptedDataToSend.length())) {
                    std::cerr << "send (profile message): not all bytes sent. Sent " << bytesSent << "/" << encryptedDataToSend.length() << std::endl;
                } else {
                    std::cout << "Encrypted profile message sent successfully to server." << std::endl;
                }
            } else {
                std::cerr << "Failed to generate message to send (payload is empty)." << std::endl;
            }
        } catch (const std::runtime_error& e) {
            std::cerr << "Error during profile message preparation or sending: " << e.what() << std::endl;
            // This might catch errors from WinCrypto if RSA/AES operations fail, 
            // or from Message class if something goes wrong during generation.
        }
    }
    
    // Placeholder for further communication (e.g., receiving game state, sending commands)
    std::cout << "Client is now ready for further application-specific communication." << std::endl;

    // Cleanup
    std::cout << "Closing connection and cleaning up..." << std::endl;
    closesocket(connectSocket);
    WSACleanup();

    // clientRsaKeyPair.hPrivateKey is cleaned up by atexit(cleanupClientKeys)
    return 0;
}


// --- Implementation of sendEvent ---
bool sendEvent(SOCKET sock, const WinCrypto::PublicKey& recipientPubKey, WinCrypto& crypto, 
               const std::string& senderName, const std::string& receiverName, 
               const std::vector<std::string>& eventData) {
    if (recipientPubKey.blob.empty()) {
        std::cerr << "sendEvent: Recipient public key is empty. Cannot send event." << std::endl;
        return false;
    }
    try {
        std::cout << "Preparing EVENT message..." << std::endl;
        Message eventMsg(Message::MessageType::EVENT, recipientPubKey, senderName, receiverName);

        for (const auto& part : eventData) {
            eventMsg.pushBody(part);
        }

        eventMsg.generateEncryptedMessageForSending(crypto);
        std::string encryptedPayload = eventMsg.getMessageToSend();

        if (encryptedPayload.empty()) {
            std::cerr << "sendEvent: Failed to generate encrypted payload for event." << std::endl;
            return false;
        }

        std::cout << "Sending EVENT message (" << encryptedPayload.length() << " bytes)... ";
        for(const auto& part : eventData) std::cout << part << " ";
        std::cout << std::endl;

        int sendResult = send(sock, encryptedPayload.c_str(), (int)encryptedPayload.length(), 0);
        if (sendResult == SOCKET_ERROR) {
            std::cerr << "sendEvent: send failed with error: " << WSAGetLastError() << std::endl;
            return false;
        }
        if (sendResult < (int)encryptedPayload.length()) {
            std::cerr << "sendEvent: Not all bytes sent. Sent " << sendResult << "/" << encryptedPayload.length() << std::endl;
            return false;
        }

        std::cout << "Event message sent successfully." << std::endl;
        return true;

    } catch (const std::runtime_error& e) {
        std::cerr << "sendEvent: Runtime error: " << e.what() << std::endl;
        return false;
    }
}

// --- Implementation of sendSnapshot ---
bool sendSnapshot(SOCKET sock, const WinCrypto::PublicKey& recipientPubKey, WinCrypto& crypto, 
                  const std::string& senderName, const std::string& receiverName, 
                  const std::string& snapshotType, const std::vector<std::string>& snapshotData) {
    if (recipientPubKey.blob.empty()) {
        std::cerr << "sendSnapshot: Recipient public key is empty. Cannot send snapshot." << std::endl;
        return false;
    }
    try {
        std::cout << "Preparing SNAP message..." << std::endl;
        Message snapMsg(Message::MessageType::SNAP, recipientPubKey, senderName, receiverName);

        snapMsg.pushBody(snapshotType); // First body part is the snapshot type
        for (const auto& part : snapshotData) {
            snapMsg.pushBody(part);
        }

        snapMsg.generateEncryptedMessageForSending(crypto);
        std::string encryptedPayload = snapMsg.getMessageToSend();

        if (encryptedPayload.empty()) {
            std::cerr << "sendSnapshot: Failed to generate encrypted payload for snapshot." << std::endl;
            return false;
        }
        
        std::cout << "Sending SNAP message (" << snapshotType << ", " << encryptedPayload.length() << " bytes)... ";
        for(const auto& part : snapshotData) std::cout << part << " ";
        std::cout << std::endl;


        int sendResult = send(sock, encryptedPayload.c_str(), (int)encryptedPayload.length(), 0);
        if (sendResult == SOCKET_ERROR) {
            std::cerr << "sendSnapshot: send failed with error: " << WSAGetLastError() << std::endl;
            return false;
        }
        if (sendResult < (int)encryptedPayload.length()) {
            std::cerr << "sendSnapshot: Not all bytes sent. Sent " << sendResult << "/" << encryptedPayload.length() << std::endl;
            return false;
        }

        std::cout << "Snapshot message (" << snapshotType << ") sent successfully." << std::endl;
        return true;

    } catch (const std::runtime_error& e) {
        std::cerr << "sendSnapshot: Runtime error: " << e.what() << std::endl;
        return false;
    }
}
