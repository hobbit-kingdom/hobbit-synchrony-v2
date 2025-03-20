#include "IPv4.h"

std::string getLocalIPv4Address() {
    // Initialize Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "WSAStartup failed";
    }

    // Get the host name
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
        WSACleanup();
        return "Error getting hostname";
    }

    // Resolve the host name to get the IP address
    struct addrinfo hints = {}, * result = nullptr;
    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
        WSACleanup();
        return "getaddrinfo failed";
    }

    // Extract the IP address
    std::string ipAddress;
    for (struct addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        struct sockaddr_in* sockaddr_ipv4 = reinterpret_cast<struct sockaddr_in*>(ptr->ai_addr);
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sockaddr_ipv4->sin_addr, ipStr, INET_ADDRSTRLEN);
        ipAddress = ipStr;
        break;  // Just take the first result
    }

    // Clean up
    freeaddrinfo(result);
    WSACleanup();

    return ipAddress.empty() ? "IP Address not found" : ipAddress;
}