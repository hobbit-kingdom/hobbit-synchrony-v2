#include "Client.h"
#include <iostream>
#include <vector>
#include <cstring>

// Include Winsock for htonl/ntohl and WSAStartup/WSACleanup
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

// Helper to initialize and clean up Winsock (if not already done by server)
struct ClientWinsockInit {
    ClientWinsockInit() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("Client WSAStartup failed: " + std::to_string(result));
        }
    }
    ~ClientWinsockInit() {
        WSACleanup();
    }
} client_winsock_initializer;


ChatClient::ChatClient(asio::io_context& io_context, const tcp::resolver::results_type& endpoints)
    : io_context_(io_context), socket_(io_context) {
    try {
        std::cout << "Client: Generating RSA key pair..." << std::endl;
        std::tie(my_pub_key_, my_priv_key_) = WinCrypto::generateRsaKeyPair();
        std::cout << "Client: Key pair generated." << std::endl;
        do_connect(endpoints);
        // Run io_context in a separate thread
        io_thread_ = std::thread([&]() { io_context_.run(); });
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Client: Failed to generate key pairs: " << e.what() << std::endl;
        throw;
    }
}

void ChatClient::write(const std::string& message_text) {
    if (!key_exchanged_) {
        std::cerr << "Client: Cannot send message, key exchange not complete." << std::endl;
        return;
    }
    asio::post(io_context_, [this, message_text]() {
        try {
            Message msg_to_send(MessageType::EVENT, server_pub_key_);
            msg_to_send.pushBody(message_text.c_str());
            msg_to_send.generateEncryptedMessageForSending();
            std::string raw_send_data = msg_to_send.getMessageToSend();

            ChatMessage chat_msg;
            chat_msg.body_length(raw_send_data.size());
            std::memcpy(chat_msg.body(), raw_send_data.data(), raw_send_data.size());
            chat_msg.encode_header();

            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(chat_msg);
            if (!write_in_progress) {
                do_write();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Client: Error preparing message to send: " << e.what() << std::endl;
        }
        });
}


void ChatClient::close() {
    asio::post(io_context_, [this]() { socket_.close(); });
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

void ChatClient::set_message_handler(std::function<void(const std::string&)> handler) {
    message_handler_ = handler;
}

void ChatClient::do_connect(const tcp::resolver::results_type& endpoints) {
    std::cout << "Client: Connecting..." << std::endl;
    asio::async_connect(socket_, endpoints,
        [this](asio::error_code ec, tcp::endpoint) {
            if (!ec) {
                std::cout << "Client: Connected. Reading server public key." << std::endl;
                do_read_header(); // Start by reading the server's public key
            }
            else {
                std::cerr << "Client: Connect error: " << ec.message() << std::endl;
                socket_.close();
            }
        });
}

void ChatClient::do_read_header() {
    asio::async_read(socket_,
        asio::buffer(read_msg_.data(), ChatMessage::header_length),
        [this](asio::error_code ec, std::size_t /*length*/) {
            if (!ec && read_msg_.decode_header()) {
                do_read_body();
            }
            else {
                std::cerr << "Client: Read header error or disconnect: " << ec.message() << std::endl;
                socket_.close();
            }
        });
}

void ChatClient::do_read_body() {
    asio::async_read(socket_,
        asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this](asio::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                std::string received_data(read_msg_.body(), read_msg_.body_length());
                // std::cout << "Client: Received raw data (" << received_data.length() << " bytes)." << std::endl;
                if (!key_exchanged_) {
                    handle_server_key(received_data);
                }
                else {
                    handle_message(received_data);
                }
                do_read_header(); // Continue reading
            }
            else {
                std::cerr << "Client: Read body error or disconnect: " << ec.message() << std::endl;
                socket_.close();
            }
        });
}

void ChatClient::handle_server_key(const std::string& hex_key) {
    std::cout << "Client: Received server public key." << std::endl;
    try {
        std::string raw_key_bytes = hexToBytes(hex_key);
        server_pub_key_.blob.assign(raw_key_bytes.begin(), raw_key_bytes.end());
        std::cout << "Client: Server public key stored. Sending client key." << std::endl;
        send_client_key();
        key_exchanged_ = true; // Mark key exchange as complete
    }
    catch (const std::exception& e) {
        std::cerr << "Client: Error processing server key: " << e.what() << std::endl;
        socket_.close();
    }
}

void ChatClient::send_client_key() {
    try {
        std::string key_str(my_pub_key_.blob.begin(), my_pub_key_.blob.end());
        std::string hex_key = bytesToHex(key_str);

        Message greeting_msg(MessageType::GREETING, server_pub_key_);
        greeting_msg.pushBody(hex_key.c_str());
        greeting_msg.generateEncryptedMessageForSending();
        std::string raw_send_data = greeting_msg.getMessageToSend();

        ChatMessage chat_msg;
        chat_msg.body_length(raw_send_data.size());
        std::memcpy(chat_msg.body(), raw_send_data.data(), raw_send_data.size());
        chat_msg.encode_header();

        asio::post(io_context_, [this, chat_msg]() {
            bool write_in_progress = !write_msgs_.empty();
            write_msgs_.push_back(chat_msg);
            if (!write_in_progress) {
                do_write();
            }
            });
        std::cout << "Client: Client key sent to server." << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Client: Error sending client key: " << e.what() << std::endl;
        socket_.close();
    }
}

void ChatClient::handle_message(const std::string& raw_data) {
    try {
        Message received_msg(raw_data, my_priv_key_);
        // std::cout << "Client: Decrypted message. Type: " << messageTypeToString(received_msg.type) << std::endl;

        if (received_msg.type == MessageType::EVENT && !received_msg.messageBodyParts.empty()) {
            std::stringstream bodyStream;
            for (const auto& part : received_msg.messageBodyParts) {
                bodyStream << part;
            }
            std::string chat_text = bodyStream.str();
            if (message_handler_) {
                message_handler_(chat_text);
            }
            else {
                std::cout << "Client Received: " << chat_text << std::endl;
            }
        }
        else {
            std::cerr << "Client: Received unexpected message type: " << messageTypeToString(received_msg.type) << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Client: Error handling received message: " << e.what() << std::endl;
    }
}

void ChatClient::do_write() {
    asio::async_write(socket_,
        asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this](asio::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty()) {
                    do_write();
                }
            }
            else {
                std::cerr << "Client: Write error: " << ec.message() << std::endl;
                socket_.close();
            }
        });
}