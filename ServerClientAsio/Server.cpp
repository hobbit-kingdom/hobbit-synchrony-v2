#include "Server.h"
#include <iostream>
#include <functional>
#include <algorithm>

// Include Winsock for htonl/ntohl and WSAStartup/WSACleanup
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

// Helper to initialize and clean up Winsock
struct WinsockInit {
    WinsockInit() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
        }
    }
    ~WinsockInit() {
        WSACleanup();
    }
} winsock_initializer;


// --- ChatRoom Implementation ---
void ChatRoom::join(ChatParticipant_ptr participant) {
    std::lock_guard<std::mutex> lock(mutex_);
    participants_.insert(participant);
    std::cout << "Server: Participant " << participant->get_id() << " joined. Total: " << participants_.size() << std::endl;
    std::string join_msg = participant->get_id() + " has joined the chat.";
    // Broadcasting join message isn't implemented here to focus on core chat
}

void ChatRoom::leave(ChatParticipant_ptr participant) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "Server: Participant " << participant->get_id() << " left. Total: " << participants_.size() - 1 << std::endl;
    participants_.erase(participant);
    std::string leave_msg = participant->get_id() + " has left the chat.";
    // Broadcasting leave message isn't implemented here
}

void ChatRoom::broadcast(const std::string& message_body, ChatParticipant_ptr sender, const WinCrypto::PrivateKey& server_priv_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string full_message = "[" + sender->get_id() + "]: " + message_body;
    std::cout << "Server: Broadcasting from " << sender->get_id() << ": " << message_body << std::endl;

    for (auto participant : participants_) {
        if (participant != sender) {
            try {
                WinCrypto::PublicKey recipient_key = participant->get_public_key();
                Message msg_to_send(MessageType::EVENT, recipient_key);
                msg_to_send.pushBody(full_message);
                msg_to_send.generateEncryptedMessageForSending();

                std::string raw_send_data = msg_to_send.getMessageToSend();

                ChatMessage chat_msg;
                chat_msg.body_length(raw_send_data.size());
                std::memcpy(chat_msg.body(), raw_send_data.data(), raw_send_data.size());
                chat_msg.encode_header();

                participant->deliver(chat_msg);
            }
            catch (const std::exception& e) {
                std::cerr << "Server: Error broadcasting to " << participant->get_id() << ": " << e.what() << std::endl;
            }
        }
    }
}

WinCrypto::PublicKey ChatRoom::get_participant_key(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto p : participants_) {
        if (p->get_id() == id) {
            return p->get_public_key();
        }
    }
    throw std::runtime_error("Participant key not found: " + id);
}

// --- ChatSession Implementation ---
ChatSession::ChatSession(tcp::socket socket, ChatRoom& room, const WinCrypto::PublicKey& server_pub_key, const WinCrypto::PrivateKey& server_priv_key)
    : socket_(std::move(socket)), room_(room), server_pub_key_(server_pub_key), server_priv_key_(server_priv_key) {
    try {
        client_id_ = socket_.remote_endpoint().address().to_string() + ":" + std::to_string(socket_.remote_endpoint().port());
        std::cout << "Server: New session created for " << client_id_ << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Server: Error getting remote endpoint: " << e.what() << std::endl;
        client_id_ = "unknown";
    }
}

void ChatSession::start() {
    std::cout << "Server: Starting session for " << client_id_ << ". Sending server public key." << std::endl;
    // Send the server's public key (hex-encoded) to the client
    std::string key_str(server_pub_key_.blob.begin(), server_pub_key_.blob.end());
    std::string hex_key = bytesToHex(key_str);

    ChatMessage msg;
    msg.body_length(hex_key.size());
    std::memcpy(msg.body(), hex_key.data(), hex_key.size());
    msg.encode_header();

    asio::async_write(socket_,
        asio::buffer(msg.data(), msg.length()),
        [this, self = shared_from_this()](asio::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                handle_server_key_sent();
            }
            else {
                std::cerr << "Server: Error sending server key to " << client_id_ << ": " << ec.message() << std::endl;
                room_.leave(shared_from_this());
            }
        });
}

void ChatSession::handle_server_key_sent() {
    std::cout << "Server: Server key sent to " << client_id_ << ". Waiting for client key." << std::endl;
    do_read_header(); // Start reading messages from the client
}

void ChatSession::deliver(const ChatMessage& msg) {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress) {
        do_write();
    }
}

WinCrypto::PublicKey ChatSession::get_public_key() {
    if (!key_exchanged_) {
        throw std::runtime_error("Client public key not yet available for " + client_id_);
    }
    return client_pub_key_;
}

std::string ChatSession::get_id() {
    return client_id_;
}

void ChatSession::do_read_header() {
    auto self(shared_from_this());
    asio::async_read(socket_,
        asio::buffer(read_msg_.data(), ChatMessage::header_length),
        [this, self](asio::error_code ec, std::size_t /*length*/) {
            if (!ec && read_msg_.decode_header()) {
                do_read_body();
            }
            else {
                std::cerr << "Server: Error reading header from " << client_id_ << ": " << ec.message() << std::endl;
                room_.leave(self);
            }
        });
}

void ChatSession::do_read_body() {
    auto self(shared_from_this());
    asio::async_read(socket_,
        asio::buffer(read_msg_.body(), read_msg_.body_length()),
        [this, self](asio::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                std::string received_data(read_msg_.body(), read_msg_.body_length());
                // std::cout << "Server: Received raw data (" << received_data.length() << " bytes) from " << client_id_ << std::endl;
                handle_message(received_data);
                do_read_header(); // Continue reading
            }
            else {
                std::cerr << "Server: Error reading body from " << client_id_ << ": " << ec.message() << std::endl;
                room_.leave(self);
            }
        });
}

void ChatSession::handle_message(const std::string& raw_data) {
    try {
        Message received_msg(raw_data, server_priv_key_);
        std::cout << "Server: Decrypted message from " << client_id_ << ". Type: " << messageTypeToString(received_msg.type) << std::endl;

        if (received_msg.type == MessageType::GREETING && !received_msg.messageBodyParts.empty()) {
            std::string hex_client_key = received_msg.messageBodyParts[0];
            std::string raw_client_key_bytes = hexToBytes(hex_client_key);
            client_pub_key_.blob.assign(raw_client_key_bytes.begin(), raw_client_key_bytes.end());
            key_exchanged_ = true;
            std::cout << "Server: Received and stored public key for " << client_id_ << std::endl;
            room_.join(shared_from_this());
        }
        else if (received_msg.type == MessageType::EVENT && key_exchanged_ && !received_msg.messageBodyParts.empty()) {
            // Assume the body is the chat message.
            // We need to parse the inner body content.
            std::stringstream bodyStream;
            for (const auto& part : received_msg.messageBodyParts) {
                bodyStream << part; // Reconstruct the original message
            }
            std::string chat_text = bodyStream.str();
            room_.broadcast(chat_text, shared_from_this(), server_priv_key_);
        }
        else {
            std::cerr << "Server: Received invalid or unexpected message type from " << client_id_ << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Server: Error handling message from " << client_id_ << ": " << e.what() << std::endl;
        // Optionally disconnect the client on error
        // room_.leave(shared_from_this());
    }
}


void ChatSession::do_write() {
    auto self(shared_from_this());
    asio::async_write(socket_,
        asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
        [this, self](asio::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                write_msgs_.pop_front();
                if (!write_msgs_.empty()) {
                    do_write();
                }
            }
            else {
                std::cerr << "Server: Error writing to " << client_id_ << ": " << ec.message() << std::endl;
                room_.leave(self);
            }
        });
}

// --- ChatServer Implementation ---
ChatServer::ChatServer(asio::io_context& io_context, const tcp::endpoint& endpoint)
    : acceptor_(io_context, endpoint) {
    try {
        std::cout << "Server: Generating RSA key pair..." << std::endl;
        std::tie(server_pub_key_, server_priv_key_) = WinCrypto::generateRsaKeyPair();
        std::cout << "Server: Key pair generated. Server starting on port " << endpoint.port() << std::endl;
        do_accept();
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Server: Failed to generate key pairs: " << e.what() << std::endl;
        throw; // Re-throw to stop server construction
    }
}

void ChatServer::do_accept() {
    acceptor_.async_accept(
        [this](asio::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "Server: Accepted new connection from " << socket.remote_endpoint() << std::endl;
                std::make_shared<ChatSession>(std::move(socket), room_, server_pub_key_, server_priv_key_)->start();
            }
            else {
                std::cerr << "Server: Accept error: " << ec.message() << std::endl;
            }
            do_accept(); // Continue accepting
        });
}