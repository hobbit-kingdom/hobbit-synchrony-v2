#pragma once

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <functional> // For std::function

#include "Message.h"    // Assuming it's in the same directory
#include "WinCrypto.h"  // Assuming it's in the same directory

using asio::ip::tcp;

class ChatClient : public std::enable_shared_from_this<ChatClient> {
public:
    ChatClient(asio::io_context& io_context);

    // Connect to the server using resolved endpoints
    void connect(const tcp::resolver::results_type& endpoints);

    // Send a plain text message to the server
    void write(const std::string& text_message);

    // Close the connection
    void close();

    // Set a handler for incoming chat messages (string is decrypted message body)
    void set_message_handler(std::function<void(const std::string&)> handler);
    
    // Set a handler for when key exchange successfully completes
    void set_key_exchange_complete_handler(std::function<void()> handler);

    // Check if key exchange has been completed
    bool is_key_exchange_completed() const { return key_exchange_completed_; }


private:
    void generate_client_keys(); // Generate RSA key pair for this client

    void do_connect(const tcp::resolver::results_type& endpoints);
    void handle_connect(const asio::error_code& ec, const tcp::endpoint& endpoint);

    void send_client_public_key_initial(); // ADDED: Sends client PK immediately after connect
    void send_client_public_key(); // Sends this client's public key to the server (RETAINED for now, might be redundant)


    void do_read(); // Start an asynchronous read operation
    void handle_received_data(const asio::error_code& error, std::size_t bytes_transferred);
    void process_message(const std::string& raw_message_data); // Process a complete raw message string

    void do_write(); // Start an asynchronous write operation from the queue
    void handle_write(const asio::error_code& error, std::size_t bytes_transferred);

    asio::io_context& io_context_;
    tcp::socket socket_;

    WinCrypto::PublicKey public_key_;    // This client's public key
    WinCrypto::PrivateKey private_key_;  // This client's private key
    WinCrypto::PublicKey server_public_key_; // Server's public key (once received)

    bool key_exchange_completed_ = false; // True once server's PK is received and client's PK is sent

    asio::streambuf read_streambuf_; // Buffer for incoming data
    std::deque<std::string> write_queue_; // Queue of outgoing messages (serialized Message strings)

    std::function<void(const std::string&)> message_handler_; // Callback for received chat messages
    std::function<void()> key_exchange_complete_handler_; // Callback for key exchange completion
};
