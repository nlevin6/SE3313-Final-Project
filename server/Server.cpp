#include "socketserver.h"
#include <iostream>
#include <algorithm>
#include <thread>
#include <vector>
#include <atomic>

using namespace Sync;

std::atomic<bool> terminateServer(false);// Global atomic flag to control server termination
std::vector<std::thread> clientThreads;// Vector to store client threads

void HandleClients(Socket* client1, Socket* client2) {
    try {
        // As logn as server is running, read data
        while (!terminateServer) {
            ByteArray player1, player2;
            int bytesReadP1 = client1 -> Read(player1);
            int bytesReadP2 = client2 -> Read(player2);


            // Message when client leaves server
            if (bytesReadP1 <= 0 && bytesReadP2 <= 0) {
                std::cout << "Both players left the game" << std::endl;
                break;
            } else if (bytesReadP1 <= 0 || bytesReadP2 <= 0) {
                std::cout << "A player left the game" << std::endl;
                break;
            }

            // Convert received message to upper case
            // Both players recieve same message from the server
            std::transform(player1.v.begin(), player1.v.end(), player1.v.begin(), ::toupper);
            std::transform(player2.v.begin(), player2.v.end(), player2.v.begin(), ::toupper);

            // Send back the transformed data
            std::cout << "Transformed message player1: " << player1.ToString() << std::endl;
            client1 -> Write(player1);

            std::cout << "Transformed message player2: " << player2.ToString() << std::endl;
            client2 -> Write(player2);
        }
    } catch (const std::string &error) {
        std::cerr << "Error: " << error << std::endl;
    }
}

// Continuously read input from server terminal
void ReadServerInput(SocketServer& server) {
    std::string input;
    while (true) {
        std::getline(std::cin, input);
        // If user wants to stop the server to gracefully terminate it
        if (input == "stop server") {
            std::cout << "Received request to stop server. Terminating..." << std::endl;
            terminateServer = true;
            server.Shutdown();
            break;
        }
    }
}

int main() {
    try {
        SocketServer server(3002);
        std::cout << "I am a server" << std::endl;

        // Start a thread to continuously read input from server terminal
        std::thread inputThread(ReadServerInput, std::ref(server));

        while (!terminateServer) { //where new clients join a thread
            Socket client1 = server.Accept();
            std::cout << "Player 1 Joined" << std::endl;
            Socket client2 = server.Accept();
            std::cout << "Player 2 Joined" << std::endl;

            // Create a thread to handle both client connections
            std::thread clientThread(HandleClients, &client1, &client2);
            clientThreads.push_back(std::move(clientThread));
        }

        // Wait for all client threads to finish
        for (auto& thread : clientThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        // Wait for the input thread to finish
        inputThread.join();
    } catch (const std::string &error) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }
    std::cout << "Server terminated gracefully." << std::endl;
    return 0;
}
