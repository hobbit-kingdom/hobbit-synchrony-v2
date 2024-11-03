#pragma once
// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h> // Include this header for InetPton
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

