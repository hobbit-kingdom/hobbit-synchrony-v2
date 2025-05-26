#include "Client.h"
#include "WinCrypto.h" // For bytesToHex, hexToBytes, and key generation
#include <iostream>    // For debugging output

// --- ChatClient Implementation ---

ChatClient::ChatClient(asio::io_context& io_context)
    : io_context_(io_context), socket_(io_context), key_exchange_completed_(false) {
    generate_client_keys();
    // std::cout << "[Client] Keys generated. Client Public Key (hex): " << WinCrypto::bytesToHex(public_key_.blob) << std::endl;
}

void ChatClient::generate_client_keys() {
    auto key_pair = WinCrypto::generateRsaKeyPair();
    public_key_ = key_pair.first;
    private_key_ = key_pair.second;
}

void ChatClient::connect(const tcp::resolver::results_type& endpoints) {
    // std::cout << "[Client] Attempting to connect..." << std::endl;
    do_connect(endpoints);
}

void ChatClient::do_connect(const tcp::resolver::results_type& endpoints) {
    auto self = shared_from_this();
    asio::async_connect(socket_, endpoints,
        [this, self](const asio::error_code& ec, const tcp::endpoint& endpoint) {
            handle_connect(ec, endpoint);
        });
}

void ChatClient::handle_connect(const asio::error_code& ec, const tcp::endpoint& endpoint) {
    if (!ec) {
        // std::cout << "[Client] Connected to " << endpoint << std::endl;
        // Client sends its public key first.
        send_client_public_key_initial();
        // Then, client waits for server's public key in response.
        do_read();
    } else {
        // std::cerr << "[Client] Connect error: " << ec.message() << std::endl;
        // Consider adding a connect_error_handler_ callback
    }
}

void ChatClient::send_client_public_key_initial() {
    // std::cout << "[Client] Sending initial public key." << std::endl;
    // Use client's own public key as a placeholder for recipient key in Message constructor
    // This is a workaround for the Message class design. The server will not be able to decrypt
    // the session key of this message, but it should still parse the body for PUBLIC_KEY_EXCHANGE.
    Message client_key_msg(MessageType::PUBLIC_KEY_EXCHANGE, public_key_); 
    
    std::string client_pub_key_hex = WinCrypto::bytesToHex(public_key_.blob);
    client_key_msg.pushBody(client_pub_key_hex); // Add client's actual public key to body
    client_key_msg.generateEncryptedMessageForSending();

    std::string outgoing_data = client_key_msg.getMessageToSend() + "\r\n--END_MSG--\r\n";
    
    asio::post(socket_.get_executor(), [this, outgoing_data]() {
        bool write_in_progress = !write_queue_.empty();
        write_queue_.push_back(outgoing_data);
        if (!write_in_progress) {
            do_write();
        }
    });
}

// This method might be redundant if send_client_public_key_initial covers all initial PK sending.
// It was in the original Client.h plan. If it's meant for a re-key or different scenario,
// its implementation would differ. For now, let's assume initial exchange is primary.
// If it's truly redundant, it can be removed later or from Client.h.
void ChatClient::send_client_public_key() {
    // std::cout << "[Client] send_client_public_key called." << std::endl;
    // This specific implementation assumes server_public_key_ is known,
    // which might be the case if this is a response or a re-key.
    // For initial send, send_client_public_key_initial() is used.
    if (!server_public_key_.blob.empty()) {
        Message client_key_msg(MessageType::PUBLIC_KEY_EXCHANGE, server_public_key_);
        std::string client_pub_key_hex = WinCrypto::bytesToHex(public_key_.blob);
        client_key_msg.pushBody(client_pub_key_hex);
        client_key_msg.generateEncryptedMessageForSending();
        std::string outgoing_data = client_key_msg.getMessageToSend() + "\r\n--END_MSG--\r\n";
        
        asio::post(socket_.get_executor(), [this, outgoing_data]() {
            bool write_in_progress = !write_queue_.empty();
            write_queue_.push_back(outgoing_data);
            if (!write_in_progress) {
                do_write();
            }
        });
    } else {
        // std::cerr << "[Client] Error: Server public key not known, cannot send client public key securely." << std::endl;
    }
}


void ChatClient::do_read() {
    auto self = shared_from_this();
    asio::async_read_until(socket_, read_streambuf_, "\r\n--END_MSG--\r\n",
        [this, self](const asio::error_code& ec, std::size_t bytes_transferred) {
            handle_received_data(ec, bytes_transferred);
        });
}

void ChatClient::handle_received_data(const asio::error_code& ec, std::size_t bytes_transferred) {
    if (!ec) {
        std::string message_str(asio::buffers_begin(read_streambuf_.data()), 
                                asio::buffers_begin(read_streambuf_.data()) + bytes_transferred - std::string("\r\n--END_MSG--\r\n").length());
        read_streambuf_.consume(bytes_transferred);
        // std::cout << "[Client] Raw data received: " << message_str << std::endl;
        process_message(message_str);
        do_read(); // Continue reading for the next message
    } else {
        // std::cerr << "[Client] Error on read: " << ec.message() << std::endl;
        // Consider calling a disconnect_handler_ or attempting reconnect.
        close(); // Simple close on read error
    }
}

void ChatClient::process_message(const std::string& raw_message_data) {
    try {
        // When receiving, we use the client's private key to decrypt the session key
        Message received_msg(raw_message_data, private_key_);

        if (received_msg.type == MessageType::PUBLIC_KEY_EXCHANGE) {
            if (!key_exchange_completed_) { // This must be the server's public key
                if (received_msg.messageBodyParts.empty()) {
                    // std::cerr << "[Client] Server's public key message has no body." << std::endl;
                    return;
                }
                std::string server_key_hex = received_msg.messageBodyParts[0];
                server_public_key_.blob = WinCrypto::hexToBytes(server_key_hex);
                key_exchange_completed_ = true;
                // std::cout << "[Client] Server public key received. Key exchange complete." << std::endl;
                if (key_exchange_complete_handler_) {
                    key_exchange_complete_handler_();
                }
            } else {
                // std::cout << "[Client] Received another PUBLIC_KEY_EXCHANGE after completion. Ignoring." << std::endl;
            }
        } else if (received_msg.type == MessageType::CHAT_MESSAGE) {
            if (key_exchange_completed_) {
                if (!received_msg.messageBodyParts.empty()) {
                    // Assuming chat message content is the first part of the body
                    // Potentially, concatenate all parts if chat messages can be multi-part
                    std::string chat_content;
                    for(size_t i = 0; i < received_msg.messageBodyParts.size(); ++i) {
                        chat_content += received_msg.messageBodyParts[i];
                        if (i < received_msg.messageBodyParts.size() - 1) {
                            chat_content += "\n"; // Or some other separator if needed
                        }
                    }
                    // std::cout << "[Client] Chat message received: " << chat_content << std::endl;
                    if (message_handler_) {
                        message_handler_(chat_content);
                    }
                } else {
                    // std::cout << "[Client] Received empty CHAT_MESSAGE." << std::endl;
                }
            } else {
                // std::cerr << "[Client] Received CHAT_MESSAGE before key exchange completed. Ignoring." << std::endl;
            }
        } else {
            // std::cerr << "[Client] Received UNKNOWN message type." << std::endl;
        }
    } catch (const std::exception& e) {
        // std::cerr << "[Client] Exception processing message: " << e.what() << std::endl;
        // Consider closing connection or specific error handling
    }
}

void ChatClient::write(const std::string& text_message) {
    if (!key_exchange_completed_) {
        // std::cerr << "[Client] Cannot send chat message: Key exchange not complete." << std::endl;
        return;
    }
    if (server_public_key_.blob.empty()) {
        // std::cerr << "[Client] Cannot send chat message: Server public key not available." << std::endl;
        return;
    }

    // std::cout << "[Client] Preparing to send chat message: " << text_message << std::endl;
    Message chat_msg(MessageType::CHAT_MESSAGE, server_public_key_); // Encrypt session key with server's public key
    chat_msg.pushBody(text_message);
    chat_msg.generateEncryptedMessageForSending(); // Encrypt body with session key

    std::string outgoing_data = chat_msg.getMessageToSend() + "\r\n--END_MSG--\r\n";

    asio::post(socket_.get_executor(), [this, outgoing_data]() {
        bool write_in_progress = !write_queue_.empty();
        write_queue_.push_back(outgoing_data);
        if (!write_in_progress) {
            do_write();
        }
    });
}

void ChatClient::do_write() {
    if (write_queue_.empty()) return;

    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(write_queue_.front().data(), write_queue_.front().length()),
        [this, self](const asio::error_code& ec, std::size_t bytes_transferred) {
            handle_write(ec, bytes_transferred);
        });
}

void ChatClient::handle_write(const asio::error_code& ec, std::size_t bytes_transferred) {
    if (!ec) {
        write_queue_.pop_front();
        if (!write_queue_.empty()) {
            do_write();
        }
    } else {
        // std::cerr << "[Client] Error on write: " << ec.message() << std::endl;
        close(); // Simple close on write error
    }
}

void ChatClient::close() {
    // std::cout << "[Client] Closing connection." << std::endl;
    asio::post(io_context_, [this]() { 
        if (socket_.is_open()) {
            socket_.close(); 
        }
    });
}

void ChatClient::set_message_handler(std::function<void(const std::string&)> handler) {
    message_handler_ = handler;
}

void ChatClient::set_key_exchange_complete_handler(std::function<void()> handler) {
    key_exchange_complete_handler_ = handler;
}
