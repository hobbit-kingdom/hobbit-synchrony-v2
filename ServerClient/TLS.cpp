#include "TLS.h"

#include <openssl/x509v3.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <map>

static void openssl_init_once() {
    static bool inited = false;
    if (!inited) {
        // OpenSSL 1.1+/3.x unified init
        OPENSSL_init_ssl(0, nullptr);
        inited = true;
    }
}

static std::string hex_sha256(X509* cert) {
    unsigned int len = 0;
    unsigned char md[EVP_MAX_MD_SIZE];
    if (!X509_digest(cert, EVP_sha256(), md, &len)) return {};
    std::ostringstream oss;
    for (unsigned i = 0; i < len; ++i) {
        if (i) oss << ":";
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)md[i];
    }
    return oss.str();
}

bool ensure_self_signed_cert(const std::string& certPath, const std::string& keyPath) {
    namespace fs = std::filesystem;
    if (fs::exists(certPath) && fs::exists(keyPath)) return true;

    openssl_init_once();

    // === Generate EC P-256 key using EVP high-level API (OpenSSL 3.x friendly) ===
    EVP_PKEY* pkey = nullptr;
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!pctx) return false;
    if (EVP_PKEY_keygen_init(pctx) <= 0) { EVP_PKEY_CTX_free(pctx); return false; }
    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1) <= 0) { EVP_PKEY_CTX_free(pctx); return false; }
    if (EVP_PKEY_CTX_set_ec_param_enc(pctx, OPENSSL_EC_NAMED_CURVE) <= 0) { EVP_PKEY_CTX_free(pctx); return false; }
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0) { EVP_PKEY_CTX_free(pctx); return false; }
    EVP_PKEY_CTX_free(pctx);

    // === Build self-signed X509 cert ===
    X509* x = X509_new();
    if (!x) { EVP_PKEY_free(pkey); return false; }
    ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
    X509_gmtime_adj(X509_get_notBefore(x), 0);
    X509_gmtime_adj(X509_get_notAfter(x), 60L * 60 * 24 * 365 * 5); // 5 years
    X509_set_pubkey(x, pkey);

    X509_NAME* name = X509_get_subject_name(x);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char*)"LocalServer", -1, -1, 0);
    X509_set_issuer_name(x, name);

    // Basic constraints & key usage
    X509_EXTENSION* ext = nullptr;
    ext = X509V3_EXT_conf_nid(nullptr, nullptr, NID_basic_constraints, (char*)"CA:FALSE"); X509_add_ext(x, ext, -1); X509_EXTENSION_free(ext);
    ext = X509V3_EXT_conf_nid(nullptr, nullptr, NID_key_usage, (char*)"digitalSignature,keyEncipherment"); X509_add_ext(x, ext, -1); X509_EXTENSION_free(ext);

    if (!X509_sign(x, pkey, EVP_sha256())) { X509_free(x); EVP_PKEY_free(pkey); return false; }

    fs::create_directories(fs::path(certPath).parent_path());

    // Use BIO file I/O to avoid MSVC CRT deprecation warnings
    BIO* kb = BIO_new_file(keyPath.c_str(), "wb");
    if (!kb) { X509_free(x); EVP_PKEY_free(pkey); return false; }
    if (!PEM_write_bio_PrivateKey(kb, pkey, nullptr, nullptr, 0, nullptr, nullptr)) { BIO_free(kb); X509_free(x); EVP_PKEY_free(pkey); return false; }
    BIO_free(kb);

    BIO* cb = BIO_new_file(certPath.c_str(), "wb");
    if (!cb) { X509_free(x); EVP_PKEY_free(pkey); return false; }
    if (!PEM_write_bio_X509(cb, x)) { BIO_free(cb); X509_free(x); EVP_PKEY_free(pkey); return false; }
    BIO_free(cb);

    X509_free(x);
    EVP_PKEY_free(pkey);
    return true;
}

bool TLSServer::init(const std::string& certPath, const std::string& keyPath) {
    openssl_init_once();
    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) return false;
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);

    if (!ensure_self_signed_cert(certPath, keyPath)) return false;
    if (SSL_CTX_use_certificate_file(ctx, certPath.c_str(), SSL_FILETYPE_PEM) != 1) return false;
    if (SSL_CTX_use_PrivateKey_file(ctx, keyPath.c_str(), SSL_FILETYPE_PEM) != 1) return false;
    return true;
}

SSL* TLSServer::wrap(SOCKET s) {
    SSL* ssl = SSL_new(ctx);
#ifdef _WIN32
    SSL_set_fd(ssl, (int)s);
#else
    SSL_set_fd(ssl, s);
#endif
    return ssl;
}

TLSServer::~TLSServer() {
    if (ctx) SSL_CTX_free(ctx);
}

bool TLSClient::init() {
    openssl_init_once();
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return false;
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    // Application does TOFU pinning; do not use OS trust store here.
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    return true;
}

static bool read_known_hosts(const std::string& path, std::map<std::string, std::string>& out) {
    std::ifstream in(path);
    if (!in) return true; // no file yet -> first use
    std::string line;
    while (std::getline(in, line)) {
        auto pos = line.find(' ');
        if (pos != std::string::npos) out[line.substr(0, pos)] = line.substr(pos + 1);
    }
    return true;
}

static bool write_known_host(const std::string& path, const std::string& key, const std::string& val) {
    namespace fs = std::filesystem;
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path, std::ios::app);
    if (!out) return false;
    out << key << " " << val << "\n";
    return true;
}

bool check_or_remember_fingerprint(SSL* ssl, const std::string& serverId,
    const std::string& knownHostsPath, std::string& outErr) {
    X509* cert = SSL_get1_peer_certificate(ssl);
    if (!cert) { outErr = "No peer certificate"; return false; }
    std::string fp = hex_sha256(cert);
    X509_free(cert);
    if (fp.empty()) { outErr = "Could not compute certificate fingerprint"; return false; }

    std::map<std::string, std::string> db;
    read_known_hosts(knownHostsPath, db);
    auto it = db.find(serverId);
    if (it == db.end()) {
        if (!write_known_host(knownHostsPath, serverId, fp)) { outErr = "Failed to write known_hosts"; return false; }
        return true; // first-use trust
    }
    if (it->second == fp) return true;

    outErr = "Pinned certificate changed for " + serverId + "\nExpected: " + it->second + "\nGot:      " + fp;
    return false;
}

SSL* TLSClient::wrapAndConnect(SOCKET s, const std::string& serverId,
    const std::string& knownHostsPath, std::string& outErr) {
    SSL* ssl = SSL_new(ctx);
#ifdef _WIN32
    SSL_set_fd(ssl, (int)s);
#else
    SSL_set_fd(ssl, s);
#endif
    if (SSL_connect(ssl) <= 0) {
        outErr = "TLS handshake failed";
        SSL_free(ssl);
        return nullptr;
    }
    if (!check_or_remember_fingerprint(ssl, serverId, knownHostsPath, outErr)) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        return nullptr;
    }
    return ssl;
}

TLSClient::~TLSClient() {
    if (ctx) SSL_CTX_free(ctx);
}

int tls_write_all(SSL* ssl, const void* data, int len) {
    const char* p = static_cast<const char*>(data);
    int sent = 0;
    while (sent < len) {
        int n = SSL_write(ssl, p + sent, len - sent);
        if (n <= 0) return n;
        sent += n;
    }
    return sent;
}

int tls_read_exact(SSL* ssl, void* data, int len) {
    char* p = static_cast<char*>(data);
    int rec = 0;
    while (rec < len) {
        int n = SSL_read(ssl, p + rec, len - rec);
        if (n <= 0) return n;
        rec += n;
    }
    return rec;
}
