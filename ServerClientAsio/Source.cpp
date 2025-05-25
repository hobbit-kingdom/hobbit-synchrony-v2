#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include "Server.h"
#include "Client.h"

// Ensure Asio is included and configured
#define ASIO_STANDALONE
#include <asio.hpp>

void print_message(const std::string& prefix, const std::string& message) {
    std::cout << prefix << message << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // --- Server Setup ---
        asio::io_context server_io_context;
        tcp::endpoint endpoint(tcp::v4(), 8080); // Listen on port 8080
        ChatServer server(server_io_context, endpoint);
        std::thread server_thread([&server_io_context]() {
            std::cout << "Test: Starting server io_context..." << std::endl;
            try {
                server_io_context.run();
            }
            catch (const std::exception& e) {
                std::cerr << "Test: Server io_context exception: " << e.what() << std::endl;
            }
            std::cout << "Test: Server io_context stopped." << std::endl;
            });

        std::cout << "Test: Server started in background thread. Waiting 2s..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // --- Client Setup ---
        const int num_clients = 3;
        std::vector<std::shared_ptr<asio::io_context>> client_io_contexts;
        std::vector<std::shared_ptr<ChatClient>> clients;
        std::vector<std::thread> client_threads;

        for (int i = 0; i < num_clients; ++i) {
            auto io_ctx = std::make_shared<asio::io_context>();
            tcp::resolver resolver(*io_ctx);
            auto endpoints = resolver.resolve("127.0.0.1", "8080");
            auto client = std::make_shared<ChatClient>(*io_ctx, endpoints);

            std::string client_name = "Client" + std::to_string(i + 1);
            client->set_message_handler([client_name](const std::string& msg) {
                print_message("[" + client_name + " Received]: ", msg);
                });

            client_io_contexts.push_back(io_ctx);
            clients.push_back(client);

            std::cout << "Test: Starting " << client_name << " io_context..." << std::endl;
        }

        // --- Wait for connections and key exchange ---
        std::cout << "Test: Waiting 5s for connections and key exchange..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // --- Simulate Chatting ---
        std::cout << "\n--- STARTING CHAT ---\n" << std::endl;

        clients[0]->write("Hello everyone! This is Client1.");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        clients[1]->write("Hi Client1! Client2 here. Good to be secure.");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        clients[2]->write("Client3 reporting in. Encryption works!");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        clients[0]->write("Did everyone get that?");
        std::this_thread::sleep_for(std::chrono::seconds(2));

        std::cout << "\n--- ENDING CHAT ---\n" << std::endl;

        // --- Cleanup ---
        std::cout << "Test: Closing clients..." << std::endl;
        for (auto& client : clients) {
            client->close();
        }

        std::cout << "Test: Stopping server..." << std::endl;
        server_io_context.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }

        std::cout << "Test: Test completed." << std::endl;

    }
    catch (std::exception& e) {
        std::cerr << "Test: Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}