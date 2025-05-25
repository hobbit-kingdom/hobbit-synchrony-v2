#pragma once

#define ASIO_STANDALONE
#include <asio.hpp>
#include <deque>
#include <set>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "../NetworkUtilities/Message.h"
#include "../NetworkUtilities/WinCrypto.h"
#include "ChatMessage.h"

using asio::ip::tcp;

class ChatParticipant {
public:
    virtual ~ChatParticipant() {}
    virtual void deliver(const ChatMessage& msg) = 0;
    virtual WinCrypto::PublicKey get_public_key() = 0;
    virtual std::string get_id() = 0;
};

typedef std::shared_ptr<ChatParticipant> ChatParticipant_ptr;

class ChatRoom {
public:
    void join(ChatParticipant_ptr participant);
    void leave(ChatParticipant_ptr participant);
    void broadcast(const std::string& message_body, ChatParticipant_ptr sender, const WinCrypto::PrivateKey& server_priv_key);
    WinCrypto::PublicKey get_participant_key(const std::string& id);

private:
    std::set<ChatParticipant_ptr> participants_;
    std::mutex mutex_;
};

class ChatSession : public ChatParticipant, public std::enable_shared_from_this<ChatSession> {
public:
    ChatSession(tcp::socket socket, ChatRoom& room, const WinCrypto::PublicKey& server_pub_key, const WinCrypto::PrivateKey& server_priv_key);
    void start();
    void deliver(const ChatMessage& msg) override;
    WinCrypto::PublicKey get_public_key() override;
    std::string get_id() override;

private:
    void do_read_header();
    void do_read_body();
    void do_write();
    void handle_server_key_sent();
    void handle_message(const std::string& raw_data);

    tcp::socket socket_;
    ChatRoom& room_;
    ChatMessage read_msg_;
    std::deque<ChatMessage> write_msgs_;
    WinCrypto::PublicKey server_pub_key_;
    WinCrypto::PrivateKey server_priv_key_;
    WinCrypto::PublicKey client_pub_key_;
    std::string client_id_;
    bool key_exchanged_ = false;
};

class ChatServer {
public:
    ChatServer(asio::io_context& io_context, const tcp::endpoint& endpoint);

private:
    void do_accept();

    tcp::acceptor acceptor_;
    ChatRoom room_;
    WinCrypto::PublicKey server_pub_key_;
    WinCrypto::PrivateKey server_priv_key_;
};