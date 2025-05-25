#pragma once

#define ASIO_STANDALONE
#include <asio.hpp>
#include <deque>
#include <string>
#include <thread>
#include <functional>
#include "../NetworkUtilities/Message.h"
#include "../NetworkUtilities/WinCrypto.h"
#include "ChatMessage.h"

using asio::ip::tcp;

class ChatClient {
public:
    ChatClient(asio::io_context& io_context, const tcp::resolver::results_type& endpoints);
    void write(const std::string& message_text);
    void close();
    void set_message_handler(std::function<void(const std::string&)> handler);

private:
    void do_connect(const tcp::resolver::results_type& endpoints);
    void do_read_header();
    void do_read_body();
    void do_write();
    void handle_server_key(const std::string& hex_key);
    void handle_message(const std::string& raw_data);
    void send_client_key();

    asio::io_context& io_context_;
    tcp::socket socket_;
    WinCrypto::PublicKey my_pub_key_;
    WinCrypto::PrivateKey my_priv_key_;
    WinCrypto::PublicKey server_pub_key_;
    ChatMessage read_msg_;
    std::deque<ChatMessage> write_msgs_;
    std::thread io_thread_;
    std::function<void(const std::string&)> message_handler_;
    bool key_exchanged_ = false;
};