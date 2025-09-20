#pragma once

// Tiny TLS helper for TLS 1.3 + self-signed certs (server) + TOFU (client).
// Visual Studio / OpenSSL 3.x compatible.

#include <string>
#include <cstdint>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
typedef int SOCKET;
#endif

struct TLSServer {
    SSL_CTX* ctx = nullptr;

    // Initializes a TLS 1.3 server context and ensures a self-signed cert exists at the given paths.
    bool init(const std::string& certPath, const std::string& keyPath);

    // Creates an SSL* bound to the accepted socket. Call SSL_accept() afterwards.
    SSL* wrap(SOCKET s);

    ~TLSServer();
};

struct TLSClient {
    SSL_CTX* ctx = nullptr;

    // Initializes a TLS 1.3 client context with application-level TOFU verification (no system PKI).
    bool init();

    // Performs TLS handshake and TOFU pinning (stores/validates SHA-256 fingerprint in knownHostsPath).
    // serverId is a stable key like "<ip>:<port>" for the pin.
    SSL* wrapAndConnect(SOCKET s, const std::string& serverId,
        const std::string& knownHostsPath, std::string& outErr);

    ~TLSClient();
};

// Utility: create a self-signed ECDSA P-256 certificate and private key if they don't already exist.
bool ensure_self_signed_cert(const std::string& certPath, const std::string& keyPath);

// Utility: TOFU pin check or first-time store of fingerprint.
bool check_or_remember_fingerprint(SSL* ssl, const std::string& serverId,
    const std::string& knownHostsPath, std::string& outErr);

// TLS read/write helpers that mirror blocking semantics of your existing framing.
int tls_write_all(SSL* ssl, const void* data, int len); // <=0 on error
int tls_read_exact(SSL* ssl, void* data, int len);      // <=0 on error
