#pragma once

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <set>
#include <map> // Will be useful for session management if mapping IDs to sessions

#include "Message.h"    // Assuming it's in the same directory
#include "WinCrypto.h"  // Assuming it's in the same directory

using asio::ip::tcp;

// Forward declaration
class ChatServer;

// Represents a connection to a single client
class ChatSession : public std::enable_shared_from_this<ChatSession> {
public:
    ChatSession(tcp::socket socket, ChatServer& server);
    void start(); // Called to start processing for this client
    void deliver(const Message& msg); // Enqueues a message to be sent to this client
    tcp::socket& get_socket() { return socket_; } // Getter for the socket
    WinCrypto::PublicKey get_client_public_key() const { return client_public_key_; }
    bool is_key_exchanged() const { return key_exchanged_; }

private:
    void send_server_public_key_initial(); // ADDED: Server sends its PK immediately
    void do_read(); // Reads data from the client
    void handle_received_data(const asio::error_code& error, std::size_t bytes_transferred);
    void process_message(const std::string& raw_message_data); // Processes a complete message string

    void send_server_public_key(); // RETAINED: May be used if client initiates or for re-key.
    void handle_client_public_key(const Message& msg); // RETAINED: Original handler for client PK

    void do_write(const std::string& msg_data); // Writes data to the client
    void handle_write(const asio::error_code& error, std::size_t bytes_transferred);

    tcp::socket socket_;
    ChatServer& server_; // Reference back to the server
    asio::streambuf read_streambuf_; // Buffer for reading data
    
    WinCrypto::PublicKey client_public_key_;
    bool key_exchanged_ = false; // True once public keys are exchanged

    // For writing messages. The Message class produces a std::string.
    std::deque<std::string> write_queue_; 
};

// Manages all client connections and message broadcasting
class ChatServer {
public:
    ChatServer(asio::io_context& io_context, const tcp::endpoint& endpoint);
    
    WinCrypto::PublicKey get_server_public_key() const { return public_key_; }
    void broadcast(const Message& msg, std::shared_ptr<ChatSession> sender_session);
    void join(std::shared_ptr<ChatSession> session);
    void leave(std::shared_ptr<ChatSession> session);

private:
    void do_accept();
    void generate_server_keys();

    asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::set<std::shared_ptr<ChatSession>> sessions_; // Active sessions

    WinCrypto::PublicKey public_key_;
    WinCrypto::PrivateKey private_key_;
};

#endif // SERVER_H_ // Basic include guard, can be #pragma once
