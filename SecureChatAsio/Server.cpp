#include "Server.h"
#include "WinCrypto.h" // For bytesToHex and hexToBytes, make sure they are callable or move them.
                       // For simplicity, assume they are globally accessible or part of WinCrypto namespace.

// Helper function (consider moving to a utility file or WinCrypto if not already accessible)
// Ensure these are available or define them if they were static in WinCrypto.cpp
// For the subtask, assume they can be called via WinCrypto::bytesToHex and WinCrypto::hexToBytes

// --- ChatSession Implementation ---
ChatSession::ChatSession(tcp::socket socket, ChatServer& server)
    : socket_(std::move(socket)), server_(server), key_exchanged_(false) {
}

void ChatSession::send_server_public_key_initial() {
    //std::cout << "[Server] Sending initial public key." << std::endl;
    // Use server's own public key as a placeholder for recipient key in Message constructor.
    // The client's Message parsing logic for an initial PUBLIC_KEY_EXCHANGE
    // must be able to extract the body (the server's actual public key) even if session key
    // decryption fails due to the placeholder.
    Message server_key_msg(MessageType::PUBLIC_KEY_EXCHANGE, server_.public_key_); 
    
    std::string server_pub_key_hex = WinCrypto::bytesToHex(server_.get_server_public_key().blob);
    server_key_msg.pushBody(server_pub_key_hex); // Add server's actual public key to body
    server_key_msg.generateEncryptedMessageForSending(); // Encrypts body with a session key. Session key encrypted with placeholder.

    // deliver() already adds delimiter and posts to write queue
    deliver(server_key_msg); 
    // Key exchange is NOT complete yet. Server has only sent its key.
    // It needs to receive client's key in response.
}

void ChatSession::start() {
    // Server sends its public key FIRST.
    send_server_public_key_initial();
    // Then, server waits for client's public key in response.
    do_read();
}

void ChatSession::deliver(const Message& msg) {
    std::string outgoing_data = msg.getMessageToSend() + "\r\n--END_MSG--\r\n";
    // Ensure thread safety if server's io_context runs on multiple threads
    // For now, assume single threaded io_context or handle strand explicitly later
    asio::post(socket_.get_executor(), [this, outgoing_data]() {
        bool write_in_progress = !write_queue_.empty();
        write_queue_.push_back(outgoing_data);
        if (!write_in_progress) {
            do_write();
        }
    });
}

void ChatSession::do_read() {
    auto self = shared_from_this();
    asio::async_read_until(socket_, read_streambuf_, "\r\n--END_MSG--\r\n",
        [this, self](const asio::error_code& ec, std::size_t bytes_transferred) {
            handle_received_data(ec, bytes_transferred);
        });
}

void ChatSession::handle_received_data(const asio::error_code& ec, std::size_t bytes_transferred) {
    if (!ec) {
        std::string message_str(asio::buffers_begin(read_streambuf_.data()), 
                                asio::buffers_begin(read_streambuf_.data()) + bytes_transferred - std::string("\r\n--END_MSG--\r\n").length());
        read_streambuf_.consume(bytes_transferred);

        //std::cout << "[Server] Raw data received: " << message_str << std::endl; // Debug
        process_message(message_str);
        do_read(); // Continue reading for the next message
    } else {
        //std::cerr << "[Server] Error on read: " << ec.message() << std::endl; // Debug
        server_.leave(shared_from_this());
    }
}

void ChatSession::process_message(const std::string& raw_message_data) {
    try {
        // When receiving, we use the server's private key to decrypt the session key
        Message client_msg(raw_message_data, server_.private_key_); // Server uses its private key to decrypt session key

        if (!key_exchanged_) { // This means we're expecting the client's public key
            if (client_msg.type == MessageType::PUBLIC_KEY_EXCHANGE) {
                if (client_msg.messageBodyParts.empty()) {
                    //std::cerr << "[Server] Client's public key message has no body." << std::endl; 
                    // Consider closing connection or sending an error
                    return;
                }
                std::string client_key_hex = client_msg.messageBodyParts[0];
                client_public_key_.blob = WinCrypto::hexToBytes(client_key_hex);
                key_exchanged_ = true; // Key exchange now complete from server's perspective
                //std::cout << "[Server] Client public key received and processed. Key exchange complete." << std::endl; 
                
                // DO NOT send server public key again here. It was sent initially by send_server_public_key_initial().
                server_.join(shared_from_this()); 
            } else {
                //std::cerr << "[Server] Expected PUBLIC_KEY_EXCHANGE message from client, got something else." << std::endl; 
                // Handle error, maybe close connection. Client should have received server PK and responded with its own.
            }
        } else { // Key already exchanged, expect CHAT_MESSAGE
            if (client_msg.type == MessageType::CHAT_MESSAGE) {
                //std::cout << "[Server] CHAT_MESSAGE received. Broadcasting..." << std::endl; // Debug
                server_.broadcast(client_msg, shared_from_this());
            } else {
                //std::cerr << "[Server] Expected CHAT_MESSAGE, got something else after key exchange." << std::endl; // Debug
            }
        }
    } catch (const std::exception& e) {
        //std::cerr << "[Server] Exception processing message: " << e.what() << std::endl; // Debug
        // server_.leave(shared_from_this()); // Optional: disconnect on error
    }
}

void ChatSession::send_server_public_key() {
    //std::cout << "[Server] Preparing to send server public key to client." << std::endl; // Debug
    // This method is now primarily for sending the server's PK *in response* to a client's PK,
    // if the client were to initiate.
    // However, with server-first initiation, this method as called from process_message is no longer primary for initial exchange.
    // It might still be useful if we implement a re-key mechanism or if server needs to send its PK again for some reason.
    // Critical: client_public_key_ MUST be valid if this is called.
    if (client_public_key_.blob.empty()) {
        //std::cerr << "[Server] Error: send_server_public_key called but client_public_key_ is not set." << std::endl;
        return;
    }
    Message server_key_msg(MessageType::PUBLIC_KEY_EXCHANGE, client_public_key_); // Encrypt session key with client's public key
    
    std::string server_pub_key_hex = WinCrypto::bytesToHex(server_.get_server_public_key().blob);
    server_key_msg.pushBody(server_pub_key_hex); // Add server's public key as hex string to body
    server_key_msg.generateEncryptedMessageForSending(); // Encrypt body with session key
    
    //std::cout << "[Server] Sending server public key message (via send_server_public_key): " << server_key_msg.getMessageToSend() << std::endl; // Debug
    deliver(server_key_msg);
}

void ChatSession::do_write() {
    if (write_queue_.empty()) return;

    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(write_queue_.front().data(), write_queue_.front().length()),
        [this, self](const asio::error_code& ec, std::size_t bytes_transferred) {
            handle_write(ec, bytes_transferred);
        });
}

void ChatSession::handle_write(const asio::error_code& ec, std::size_t bytes_transferred) {
    if (!ec) {
        write_queue_.pop_front();
        if (!write_queue_.empty()) {
            do_write();
        }
    } else {
        //std::cerr << "[Server] Error on write: " << ec.message() << std::endl; // Debug
        server_.leave(shared_from_this());
    }
}

// --- ChatServer Implementation ---
ChatServer::ChatServer(asio::io_context& io_context, const tcp::endpoint& endpoint)
    : io_context_(io_context), acceptor_(io_context, endpoint) {
    generate_server_keys();
    //std::cout << "[Server] Keys generated. Server Public Key (hex): " << WinCrypto::bytesToHex(public_key_.blob) << std::endl; // Debug
    do_accept();
}

void ChatServer::generate_server_keys() {
    auto key_pair = WinCrypto::generateRsaKeyPair();
    public_key_ = key_pair.first;
    private_key_ = key_pair.second;
}

void ChatServer::do_accept() {
    acceptor_.async_accept(
        [this](asio::error_code ec, tcp::socket socket) {
            if (!ec) {
                //std::cout << "[Server] New connection accepted." << std::endl; // Debug
                std::make_shared<ChatSession>(std::move(socket), *this)->start();
            } else {
                //std::cerr << "[Server] Error accepting connection: " << ec.message() << std::endl; // Debug
            }
            do_accept(); // Continue accepting other connections
        });
}

void ChatServer::broadcast(const Message& original_msg, std::shared_ptr<ChatSession> sender_session) {
    //std::cout << "[Server] Broadcasting message from a client." << std::endl; // Debug
    for (auto& session : sessions_) {
        if (session == sender_session || !session->is_key_exchanged()) {
            continue; // Don't send to self or to sessions not fully initialized
        }
        
        // Create a new Message, encrypting with this specific session's public key
        Message msg_to_send(original_msg.type, session->get_client_public_key());
        for (const auto& part : original_msg.messageBodyParts) {
            msg_to_send.pushBody(part);
        }
        msg_to_send.generateEncryptedMessageForSending();
        
        session->deliver(msg_to_send);
    }
}

void ChatServer::join(std::shared_ptr<ChatSession> session) {
    //std::cout << "[Server] Client session joined." << std::endl; // Debug
    sessions_.insert(session);
    // Potentially broadcast a "user has joined" message if desired
}

void ChatServer::leave(std::shared_ptr<ChatSession> session) {
    //std::cout << "[Server] Client session left." << std::endl; // Debug
    sessions_.erase(session);
    // Potentially broadcast a "user has left" message if desired
}
