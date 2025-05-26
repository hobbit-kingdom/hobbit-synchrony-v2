// SecureChatAsio/TestServerClient.cpp

#define ASIO_STANDALONE
#include <asio.hpp>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <functional>
#include <string>
#include <memory> // For std::make_shared
#include <mutex>  // For std::mutex

#include "Server.h"
#include "Client.h"

// Using declarations for brevity
using asio::ip::tcp;

void print_client_message(const std::string& client_name, const std::string& message) {
    // Basic thread-safe print
    static std::mutex cout_mutex;
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << "[" << client_name << "]: " << message << std::endl;
}

int main() {
    const std::string host = "127.0.0.1";
    const unsigned short port = 8080;
    const int num_clients = 2;

    try {
        // --- Server Setup ---
        asio::io_context server_io_context;
        auto server_endpoint = tcp::endpoint(asio::ip::address::from_string(host), port);
        // ChatServer is managed by shared_ptr, but its lifetime is tied to server_io_context.run()
        // It will be cleaned up when server_io_context is stopped and server_thread joined.
        auto chat_server = std::make_shared<ChatServer>(server_io_context, server_endpoint);
        
        std::thread server_thread([&server_io_context]() {
            print_client_message("TestMain", "Server io_context starting...");
            try {
                asio::executor_work_guard<asio::io_context::executor_type> work_guard(server_io_context.get_executor());
                server_io_context.run();
            } catch (const std::exception& e) {
                print_client_message("TestMain-ServerThread", std::string("Exception: ") + e.what());
            }
            print_client_message("TestMain", "Server io_context stopped.");
        });

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Give server a moment to start

        // --- Clients Setup ---
        asio::io_context client_io_context;
        std::vector<std::shared_ptr<ChatClient>> clients;
        tcp::resolver resolver(client_io_context);
        auto endpoints = resolver.resolve(host, std::to_string(port));

        for (int i = 0; i < num_clients; ++i) {
            std::string client_name = "Client" + std::to_string(i + 1);
            auto client = std::make_shared<ChatClient>(client_io_context);

            client->set_message_handler([client_name](const std::string& msg) {
                print_client_message(client_name + " Received", msg);
            });

            client->set_key_exchange_complete_handler([client_name /* client capture not strictly needed here if only printing */ ]() {
                print_client_message(client_name, "Key exchange complete. Ready to chat!");
            });

            clients.push_back(client);
            print_client_message("TestMain", "Connecting " + client_name + "...");
            client->connect(endpoints); // connect is asynchronous
        }

        std::thread client_thread([&client_io_context]() {
            print_client_message("TestMain", "Client io_context starting...");
            try {
                asio::executor_work_guard<asio::io_context::executor_type> work_guard(client_io_context.get_executor());
                client_io_context.run();
            } catch (const std::exception& e) {
                print_client_message("TestMain-ClientThread", std::string("Exception: ") + e.what());
            }
            print_client_message("TestMain", "Client io_context stopped.");
        });

        print_client_message("TestMain", "Waiting for connections and key exchange (5 seconds)...");
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // --- Simulate Chatting ---
        // Ensure key exchange has happened for clients before they write.
        if (num_clients > 0 && clients[0] && clients[0]->is_key_exchange_completed()) {
            print_client_message("TestMain", "Client1 sending message 'Hello everyone! This is Client1.'");
            clients[0]->write("Hello everyone! This is Client1.");
        } else if (num_clients > 0) {
            print_client_message("TestMain", "Client1 key exchange not complete. Message not sent.");
        }
        std::this_thread::sleep_for(std::chrono::seconds(2)); 

        if (num_clients > 1 && clients[1] && clients[1]->is_key_exchange_completed()) {
            print_client_message("TestMain", "Client2 sending message 'Hi Client1! Client2 here.'");
            clients[1]->write("Hi Client1! Client2 here.");
        } else if (num_clients > 1) {
            print_client_message("TestMain", "Client2 key exchange not complete. Message not sent.");
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (num_clients > 0 && clients[0] && clients[0]->is_key_exchange_completed()) {
            print_client_message("TestMain", "Client1 sending message 'How is everyone doing?'");
            clients[0]->write("How is everyone doing?");
        } else if (num_clients > 0) {
            print_client_message("TestMain", "Client1 key exchange not complete for second message. Message not sent.");
        }
        std::this_thread::sleep_for(std::chrono::seconds(3)); 

        // --- Cleanup ---
        print_client_message("TestMain", "Closing clients...");
        for (auto& client : clients) {
            if (client) client->close(); // close is asynchronous
        }
        
        // Allow time for close operations to be processed by client_io_context
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        print_client_message("TestMain", "Stopping client io_context...");
        client_io_context.stop(); // Request client_io_context to stop
        if (client_thread.joinable()) {
            client_thread.join();
        }
        print_client_message("TestMain", "Client thread joined.");

        // ChatServer shared_ptr will be released. If acceptor/sockets are still active
        // they should be closed by ChatServer's destructor or a dedicated stop method.
        // For this test, stopping server_io_context should lead to cleanup.
        print_client_message("TestMain", "Stopping server io_context...");
        server_io_context.stop(); // Request server_io_context to stop
        if (server_thread.joinable()) {
            server_thread.join();
        }
        print_client_message("TestMain", "Server thread joined.");

        print_client_message("TestMain", "Test completed.");

    } catch (const std::exception& e) {
        print_client_message("TestMain", std::string("Unhandled Exception in main: ") + e.what());
        return 1;
    }

    return 0;
}

// Note: For the checks like `clients[0]->is_key_exchange_completed()` to compile,
// you need to add this public method to your `ChatClient` class in `Client.h`:
//
// public:
//     // ... existing methods ...
//     bool is_key_exchange_completed() const { return key_exchange_completed_; } 
// private:
//     // ... existing members ...
//     // bool key_exchange_completed_ = false; // This member should already exist
//
// Ensure this member `key_exchange_completed_` is appropriately set to `true` in your
// client's logic once the key exchange process with the server is successfully finished.
// Based on previous steps, this should be happening in `ChatClient::process_message`
// when it receives the server's public key.
